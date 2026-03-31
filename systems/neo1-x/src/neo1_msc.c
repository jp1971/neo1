// neo1_msc.c
//
// Minimal MSC (Mass Storage Class) block device interface for the Neo1 6502
// runtime. This module presents a small set of memory-mapped I/O registers that
// the 6502 can use to open a file on the mounted USB drive and read/write 512B
// sectors.
//
// Design notes:
// - this is a command-driven register interface, not a full filesystem API
// - operations complete synchronously and expose completion via STATUS
// - DATA acts as a byte stream for both sector data and command payloads

#include "neo1_msc.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>

// Enable to print MSC debug messages to the host console.
#ifndef NEO1_MSC_DEBUG
#define NEO1_MSC_DEBUG 1
#endif

// Internal state
static FATFS g_fatfs;
static FIL g_file;
static DIR g_dir;
static bool g_file_open = false;
static bool g_dir_open = false;
static uint32_t g_sector = 0;
static uint16_t g_data_offset = 0;
static uint8_t g_status = NEO1_MSC_STATUS_READY;
static uint8_t g_index = 0;
static uint8_t g_info = 0;
static uint8_t g_last_dir_index = 0xFF;
static uint16_t g_file_size = 0;
static uint8_t g_buffer[512];
static bool g_open_pending = false;

// During an OPEN command, we buffer the filename bytes written to the DATA port
// until we receive a null terminator.
static char g_open_filename[NEO1_MSC_FILENAME_MAX];
static uint16_t g_open_filename_pos = 0;

// Filter directory entries to files intended for load/open workflows.
static bool is_loadable_entry(const FILINFO* fno) {
    if (fno->fname[0] == '\0') {
        return false;
    }
    if (fno->fname[0] == '.') {
        return false;
    }
    if ((fno->fattrib & AM_DIR) != 0) {
        return false;
    }
    return true;
}

// Close active enumeration cursor, if present.
static void close_dir_if_open(void) {
    if (g_dir_open) {
        f_closedir(&g_dir);
        g_dir_open = false;
    }
}

// Ensure volume "0:" is mounted before any file/directory operation.
static FRESULT ensure_mounted(void) {
    return f_mount(&g_fatfs, "0:", 1);
}

// Set STATUS register to error state with FatFs-compatible low bits.
static void set_error(uint8_t err) {
    // Store a nonzero status so the CPU can detect the error.
    // High bit indicates error; lower bits are the FatFs result code.
    g_status = NEO1_MSC_STATUS_ERROR | (err & 0x7F);
}

// Set STATUS register to ready/success.
static void set_ready(void) {
    g_status = NEO1_MSC_STATUS_READY;
}

// Set STATUS register to busy/in-progress.
static void set_busy(void) {
    g_status = NEO1_MSC_STATUS_BUSY;
}

// OPEN handler: open/create filename previously streamed through DATA writes.
static void do_open(void) {
    // Mount the filesystem if needed.
    // Note: the USB MSC driver mounts the volume as "0:".
    FRESULT res = ensure_mounted();
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    // Open or create file for read/write.
    res = f_open(&g_file, g_open_filename, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] open '%s' failed: err=%d\n", g_open_filename, res);
#endif
        set_error((uint8_t)res);
        return;
    }

#if NEO1_MSC_DEBUG
    printf("[msc] opened '%s'\n", g_open_filename);
#endif

    g_file_open = true;
    {
        const FSIZE_t size = f_size(&g_file);
        g_file_size = (size > 0xFFFFu) ? 0xFFFFu : (uint16_t)size;
    }
    // Reset data offset so the write buffer is clean after filename streaming.
    g_data_offset = 0;
    set_ready();
}

// CLOSE handler: close file and reset related state.
static void do_close(void) {
    if (g_file_open) {
        f_close(&g_file);
        g_file_open = false;
    }
    g_file_size = 0;
    close_dir_if_open();
    set_ready();
}

// DIR_OPEN handler: open root directory and reset enumeration metadata.
static void do_dir_open(void) {
    FRESULT res = ensure_mounted();
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    close_dir_if_open();
    res = f_opendir(&g_dir, "0:");
    if (res != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] dir open failed: err=%d\n", res);
#endif
        set_error((uint8_t)res);
        return;
    }

    g_dir_open = true;
    g_info = 0;
    g_last_dir_index = 0xFF;
    g_file_size = 0;
#if NEO1_MSC_DEBUG
    printf("[msc] dir open ok\n");
#endif
    set_ready();
}

// DIR_NEXT handler: return next loadable entry name through DATA reads.
static void do_dir_next(void) {
    if (!g_dir_open) {
        set_error((uint8_t)FR_INVALID_OBJECT);
        return;
    }

    FILINFO fno;
    while (true) {
        FRESULT res = f_readdir(&g_dir, &fno);
        if (res != FR_OK) {
            set_error((uint8_t)res);
            return;
        }

        if (fno.fname[0] == '\0') {
            g_info = 0;
            g_data_offset = 0;
            g_file_size = 0;
            g_buffer[0] = '\0';
            set_ready();
            return;
        }

        if (!is_loadable_entry(&fno)) {
            continue;
        }

        const size_t name_len = strlen(fno.fname);
        const size_t copy_len = (name_len < (sizeof(g_buffer) - 1)) ? name_len : (sizeof(g_buffer) - 1);
        memcpy(g_buffer, fno.fname, copy_len);
        g_buffer[copy_len] = '\0';
        g_data_offset = 0;
        g_last_dir_index = (uint8_t)(g_last_dir_index + 1u);
        g_info = NEO1_MSC_INFO_VALID;
        g_file_size = (fno.fsize > 0xFFFFu) ? 0xFFFFu : (uint16_t)fno.fsize;
#if NEO1_MSC_DEBUG
        printf("[msc] dir[%u] '%s'\n", (unsigned)g_last_dir_index, (char*)g_buffer);
#endif
        set_ready();
        return;
    }
}

// Find Nth loadable file in root directory.
static bool find_indexed_file(uint8_t index, char* out_name, size_t out_name_size, FRESULT* out_res) {
    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, "0:");
    if (res != FR_OK) {
        if (out_res) {
            *out_res = res;
        }
        return false;
    }

    uint8_t current = 0;
    while (true) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            f_closedir(&dir);
            if (out_res) {
                *out_res = res;
            }
            return false;
        }
        if (fno.fname[0] == '\0') {
            break;
        }
        if (!is_loadable_entry(&fno)) {
            continue;
        }
        if (current == index) {
            const size_t name_len = strlen(fno.fname);
            const size_t copy_len = (name_len < (out_name_size - 1)) ? name_len : (out_name_size - 1);
            memcpy(out_name, fno.fname, copy_len);
            out_name[copy_len] = '\0';
            f_closedir(&dir);
            if (out_res) {
                *out_res = FR_OK;
            }
            return true;
        }
        current++;
    }

    f_closedir(&dir);
    if (out_res) {
        *out_res = FR_NO_FILE;
    }
    return false;
}

// OPEN_INDEX handler: resolve selected entry and dispatch regular OPEN.
static void do_open_index(void) {
    FRESULT res = ensure_mounted();
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    if (!find_indexed_file(g_index, g_open_filename, sizeof(g_open_filename), &res)) {
        set_error((uint8_t)res);
        return;
    }

#if NEO1_MSC_DEBUG
    printf("[msc] open index %u -> '%s'\n", (unsigned)g_index, g_open_filename);
#endif
    do_open();
}

// DELETE_INDEX handler: resolve selected entry and delete it.
static void do_delete_index(void) {
    FRESULT res = ensure_mounted();
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    if (!find_indexed_file(g_index, g_open_filename, sizeof(g_open_filename), &res)) {
        set_error((uint8_t)res);
        return;
    }

#if NEO1_MSC_DEBUG
    printf("[msc] delete index %u -> '%s'\n", (unsigned)g_index, g_open_filename);
#endif

    if (g_file_open) {
        f_close(&g_file);
        g_file_open = false;
    }
    close_dir_if_open();

    res = f_unlink(g_open_filename);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    g_info = 0;
    g_file_size = 0;
    g_data_offset = 0;
    set_ready();
}

// READ handler: load sector-sized payload into DATA buffer.
static void do_read(void) {
    if (!g_file_open) {
        set_error(1);
        return;
    }

    g_data_offset = 0;
    UINT br;
    FRESULT res = f_lseek(&g_file, (DWORD)g_sector * 512);
    if (res != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] read sector %lu failed: err=%d\n", (unsigned long)g_sector, res);
#endif
        set_error((uint8_t)res);
        return;
    }

    res = f_read(&g_file, g_buffer, sizeof(g_buffer), &br);
    if (res != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] read sector %lu: f_read failed: err=%d\n", (unsigned long)g_sector, res);
#endif
        set_error((uint8_t)res);
        return;
    }

    // If we read fewer than 512 bytes, pad the remainder with zeros.
    if (br < sizeof(g_buffer)) {
        memset(&g_buffer[br], 0, sizeof(g_buffer) - br);
    }

#if NEO1_MSC_DEBUG
    printf("[msc] read sector %lu: %u bytes\n", (unsigned long)g_sector, (unsigned)br);
#endif

    set_ready();
}

// WRITE handler: write DATA buffer to selected sector and sync file.
static void do_write(void) {
    if (!g_file_open) {
        set_error(1);
        return;
    }

    g_data_offset = 0;
    UINT bw;
    uint16_t write_len = g_file_size;
    if (write_len == 0 || write_len > sizeof(g_buffer)) {
        write_len = (uint16_t)sizeof(g_buffer);
    }
    FRESULT res = f_lseek(&g_file, (DWORD)g_sector * 512);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    res = f_write(&g_file, g_buffer, write_len, &bw);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }
    if (bw != write_len) {
        set_error(1);
        return;
    }

    // Keep file length consistent with bytes intentionally written.
    {
        const DWORD target_size = ((DWORD)g_sector * 512u) + (DWORD)write_len;
        res = f_lseek(&g_file, target_size);
        if (res != FR_OK) {
            set_error((uint8_t)res);
            return;
        }
        res = f_truncate(&g_file);
        if (res != FR_OK) {
            set_error((uint8_t)res);
            return;
        }
    }

    {
        const FSIZE_t size = f_size(&g_file);
        g_file_size = (size > 0xFFFFu) ? 0xFFFFu : (uint16_t)size;
    }

    // Ensure data hits disk.
    res = f_sync(&g_file);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    set_ready();
}

void neo1_msc_init(void) {
    // Clear internal state.
    g_file_open = false;
    g_dir_open = false;
    g_sector = 0;
    g_data_offset = 0;
    g_index = 0;
    g_info = 0;
    g_last_dir_index = 0xFF;
    g_file_size = 0;
    g_open_pending = false;
    g_open_filename_pos = 0;
    g_open_filename[0] = '\0';
    set_ready();
}

// Read side of MSC memory-mapped register interface.
uint8_t neo1_msc_io_read(uint16_t addr) {
    switch (addr) {
        case NEO1_IO_MSC_STATUS:
            return g_status;
        case NEO1_IO_MSC_DATA:
            if (g_data_offset >= sizeof(g_buffer)) {
                return 0x00;
            }
            return g_buffer[g_data_offset++];
        case NEO1_IO_MSC_INDEX:
            return g_index;
        case NEO1_IO_MSC_INFO:
            return g_info;
        case NEO1_IO_MSC_SIZE_LO:
            return (uint8_t)(g_file_size & 0x00FFu);
        case NEO1_IO_MSC_SIZE_HI:
            return (uint8_t)((g_file_size >> 8) & 0x00FFu);
        default:
            return 0x00;
    }
}

// Write side of MSC memory-mapped register interface.
void neo1_msc_io_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case NEO1_IO_MSC_CMD:
            // Command dispatch point for 6502-side control flow.
            switch (data) {
                case NEO1_MSC_CMD_OPEN:
                    // Prepare to receive filename via DATA port writes.
                    g_open_filename_pos = 0;
                    g_open_filename[0] = '\0';
                    g_open_pending = true;
                    g_data_offset = 0;
                    set_busy();
                    break;

                case NEO1_MSC_CMD_CLOSE:
                    do_close();
                    break;

                case NEO1_MSC_CMD_READ:
                    do_read();
                    break;

                case NEO1_MSC_CMD_WRITE:
                    do_write();
                    break;

                case NEO1_MSC_CMD_DIR_OPEN:
                    do_dir_open();
                    break;

                case NEO1_MSC_CMD_DIR_NEXT:
                    do_dir_next();
                    break;

                case NEO1_MSC_CMD_OPEN_INDEX:
                    do_open_index();
                    break;

                case NEO1_MSC_CMD_DELETE_INDEX:
                    do_delete_index();
                    break;

                default:
                    set_error(1);
                    break;
            }
            break;

        case NEO1_IO_MSC_SECTOR_LO:
            g_sector = (g_sector & 0xFF00u) | (uint32_t)data;
            break;

        case NEO1_IO_MSC_SECTOR_HI:
            g_sector = (g_sector & 0x00FFu) | ((uint32_t)data << 8);
            break;

        case NEO1_IO_MSC_DATA:
            // During an active OPEN command we receive filename bytes here.
            if (g_open_pending) {
                if (g_open_filename_pos < (sizeof(g_open_filename) - 1)) {
                    g_open_filename[g_open_filename_pos++] = (char)data;
                    g_open_filename[g_open_filename_pos] = '\0';
                }
                if (data == '\0' || g_open_filename_pos >= (sizeof(g_open_filename) - 1)) {
                    // Filename complete (or max length reached): perform OPEN once.
                    g_open_pending = false;
                    do_open();
                }
                break;
            }

            // Otherwise DATA targets the sector payload buffer.
            if (g_data_offset < sizeof(g_buffer)) {
                g_buffer[g_data_offset++] = data;
            }
            break;

        case NEO1_IO_MSC_INDEX:
            g_index = data;
            break;

        case NEO1_IO_MSC_SIZE_LO:
            g_file_size = (uint16_t)((g_file_size & 0xFF00u) | (uint16_t)data);
            break;

        case NEO1_IO_MSC_SIZE_HI:
            g_file_size = (uint16_t)((g_file_size & 0x00FFu) | ((uint16_t)data << 8));
            break;

        default:
            break;
    }
}
