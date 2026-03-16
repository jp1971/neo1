// neo1_msc.c
//
// Minimal MSC (Mass Storage Class) block device interface for the Neo1 6502
// runtime. This module presents a small set of memory-mapped I/O registers that
// the 6502 can use to open a file on the mounted USB drive and read/write 512B
// sectors.

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
static uint8_t g_buffer[512];

// During an OPEN command, we buffer the filename bytes written to the DATA port
// until we receive a null terminator.
static char g_open_filename[NEO1_MSC_FILENAME_MAX];
static uint16_t g_open_filename_pos = 0;

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

static void close_dir_if_open(void) {
    if (g_dir_open) {
        f_closedir(&g_dir);
        g_dir_open = false;
    }
}

static FRESULT ensure_mounted(void) {
    return f_mount(&g_fatfs, "0:", 1);
}

static void set_error(uint8_t err) {
    // Store a nonzero status so the CPU can detect the error.
    // High bit indicates error; lower bits are the FatFs result code.
    g_status = NEO1_MSC_STATUS_ERROR | (err & 0x7F);
}

static void set_ready(void) {
    g_status = NEO1_MSC_STATUS_READY;
}

static void set_busy(void) {
    g_status = NEO1_MSC_STATUS_BUSY;
}

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
    set_ready();
}

static void do_close(void) {
    if (g_file_open) {
        f_close(&g_file);
        g_file_open = false;
    }
    close_dir_if_open();
    set_ready();
}

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
#if NEO1_MSC_DEBUG
    printf("[msc] dir open ok\n");
#endif
    set_ready();
}

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
#if NEO1_MSC_DEBUG
        printf("[msc] dir[%u] '%s'\n", (unsigned)g_last_dir_index, (char*)g_buffer);
#endif
        set_ready();
        return;
    }
}

static void do_open_index(void) {
    FRESULT res = ensure_mounted();
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    DIR dir;
    FILINFO fno;
    res = f_opendir(&dir, "0:");
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    uint8_t current = 0;
    bool found = false;
    while (true) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            f_closedir(&dir);
            set_error((uint8_t)res);
            return;
        }
        if (fno.fname[0] == '\0') {
            break;
        }
        if (!is_loadable_entry(&fno)) {
            continue;
        }
        if (current == g_index) {
            const size_t name_len = strlen(fno.fname);
            const size_t copy_len = (name_len < (sizeof(g_open_filename) - 1)) ? name_len : (sizeof(g_open_filename) - 1);
            memcpy(g_open_filename, fno.fname, copy_len);
            g_open_filename[copy_len] = '\0';
            found = true;
            break;
        }
        current++;
    }

    f_closedir(&dir);
    if (!found) {
        set_error((uint8_t)FR_NO_FILE);
        return;
    }

#if NEO1_MSC_DEBUG
    printf("[msc] open index %u -> '%s'\n", (unsigned)g_index, g_open_filename);
#endif
    do_open();
}

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

static void do_write(void) {
    if (!g_file_open) {
        set_error(1);
        return;
    }

    g_data_offset = 0;
    UINT bw;
    FRESULT res = f_lseek(&g_file, (DWORD)g_sector * 512);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    res = f_write(&g_file, g_buffer, sizeof(g_buffer), &bw);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }
    if (bw != sizeof(g_buffer)) {
        set_error(1);
        return;
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
    g_open_filename_pos = 0;
    g_open_filename[0] = '\0';
    set_ready();
}

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
        default:
            return 0x00;
    }
}

void neo1_msc_io_write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case NEO1_IO_MSC_CMD:
            switch (data) {
                case NEO1_MSC_CMD_OPEN:
                    // Prepare to receive filename via DATA port writes.
                    g_open_filename_pos = 0;
                    g_open_filename[0] = '\0';
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
            // During an OPEN command we receive the filename here.
            if (g_open_filename_pos < (sizeof(g_open_filename) - 1)) {
                g_open_filename[g_open_filename_pos++] = (char)data;
                g_open_filename[g_open_filename_pos] = '\0';
                if (data == '\0') {
                    // Null-terminator indicates filename is complete.
                    do_open();
                }
            }
            // Also allow direct access to the sector buffer for read/write operations.
            if (g_data_offset < sizeof(g_buffer)) {
                g_buffer[g_data_offset++] = data;
            }
            break;

        case NEO1_IO_MSC_INDEX:
            g_index = data;
            break;

        default:
            break;
    }
}
