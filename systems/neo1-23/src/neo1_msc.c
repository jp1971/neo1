// neo1_msc.c
//
// Minimal MSC (Mass Storage Class) block device interface for the Neo1 6502
// runtime. This module presents a small set of memory-mapped I/O registers that
// the 6502 can use to open a file on the mounted USB drive and read/write 512B
// sectors.

#include "neo1_msc.h"
#include "ff.h"

#include <string.h>

// Internal state
static FATFS g_fatfs;
static FIL g_file;
static bool g_file_open = false;
static uint32_t g_sector = 0;
static uint16_t g_data_offset = 0;
static uint8_t g_status = NEO1_MSC_STATUS_READY;
static uint8_t g_buffer[512];

// During an OPEN command, we buffer the filename bytes written to the DATA port
// until we receive a null terminator.
static char g_open_filename[NEO1_MSC_FILENAME_MAX];
static uint16_t g_open_filename_pos = 0;

static void set_error(uint8_t err) {
    g_status = NEO1_MSC_STATUS_ERROR;
    (void)err;
}

static void set_ready(void) {
    g_status = NEO1_MSC_STATUS_READY;
}

static void do_open(void) {
    // Mount the filesystem if needed.
    FRESULT res = f_mount(&g_fatfs, "0:", 1);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    // Open or create file for read/write.
    res = f_open(&g_file, g_open_filename, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    g_file_open = true;
    set_ready();
}

static void do_close(void) {
    if (g_file_open) {
        f_close(&g_file);
        g_file_open = false;
    }
    set_ready();
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
        set_error((uint8_t)res);
        return;
    }

    res = f_read(&g_file, g_buffer, sizeof(g_buffer), &br);
    if (res != FR_OK) {
        set_error((uint8_t)res);
        return;
    }

    // If we read fewer than 512 bytes, pad the remainder with zeros.
    if (br < sizeof(g_buffer)) {
        memset(&g_buffer[br], 0, sizeof(g_buffer) - br);
    }

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
    g_sector = 0;
    g_data_offset = 0;
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
                    set_ready();
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

        default:
            break;
    }
}
