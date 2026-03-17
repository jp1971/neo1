#include "neo1_cffa1.h"

#include "ff.h"

#include <string.h>
#include <stdio.h>

#ifndef NEO1_CFFA1_DEBUG
#define NEO1_CFFA1_DEBUG 1
#endif

#define NEO1_CFFA1_BLOCK_SIZE 512u

static uint8_t g_regs[NEO1_CFFA1_IO_SIZE];
static FATFS g_fatfs;
static FIL g_image;
static bool g_image_open;
static uint32_t g_image_blocks;
static uint8_t g_block_buffer[NEO1_CFFA1_BLOCK_SIZE];
static uint16_t g_block_offset;

static void set_status(uint8_t status) {
    g_regs[NEO1_CFFA1_REG_STATUS_COMMAND] = status;
    g_regs[NEO1_CFFA1_REG_DEVCTRL_ALTSTATUS] = status;
}

static void set_error(uint8_t err_code) {
    g_regs[NEO1_CFFA1_REG_ERROR_FEATURE] = err_code;
    set_status(NEO1_CFFA1_STATUS_DRDY | NEO1_CFFA1_STATUS_DSC | NEO1_CFFA1_STATUS_ERR);
}

static void set_ok(uint8_t with_drq) {
    g_regs[NEO1_CFFA1_REG_ERROR_FEATURE] = NEO1_CFFA1_ERR_OK;
    set_status(NEO1_CFFA1_STATUS_DRDY | NEO1_CFFA1_STATUS_DSC | (with_drq ? NEO1_CFFA1_STATUS_DRQ : 0));
}

static bool filename_has_disk_ext(const char* name) {
    const size_t len = strlen(name);
    if (len < 4) {
        return false;
    }
    const char c0 = (char)(name[len - 3] | 0x20);
    const char c1 = (char)(name[len - 2] | 0x20);
    const char c2 = (char)(name[len - 1] | 0x20);
    if ((c0 == 'p') && (c1 == 'o')) {
        return true;
    }
    if ((len >= 5) && (c0 == 'h') && (c1 == 'd') && (c2 == 'v')) {
        return true;
    }
    if ((len >= 5) && (c0 == '2') && (c1 == 'm') && (c2 == 'g')) {
        return true;
    }
    return false;
}

static FRESULT mount_fs(void) {
    return f_mount(&g_fatfs, "0:", 1);
}

static void close_image_if_open(void) {
    if (g_image_open) {
        f_close(&g_image);
        g_image_open = false;
        g_image_blocks = 0;
    }
}

static bool open_first_image(void) {
    if (g_image_open) {
        return true;
    }

    if (mount_fs() != FR_OK) {
        return false;
    }

    const char* preferred[] = {
        "CFFA1.PO",
        "cffa1.po",
        "CFFA1.HDV",
        "cffa1.hdv",
    };

    for (unsigned i = 0; i < (unsigned)(sizeof(preferred) / sizeof(preferred[0])); i++) {
        if (f_open(&g_image, preferred[i], FA_READ) == FR_OK) {
            g_image_open = true;
            g_image_blocks = (uint32_t)(f_size(&g_image) / NEO1_CFFA1_BLOCK_SIZE);
#if NEO1_CFFA1_DEBUG
            printf("[cffa1] image '%s' blocks=%lu\n", preferred[i], (unsigned long)g_image_blocks);
#endif
            return true;
        }
    }

    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, "0:") != FR_OK) {
        return false;
    }

    bool opened = false;
    while (true) {
        const FRESULT res = f_readdir(&dir, &fno);
        if (res != FR_OK) {
            break;
        }
        if (fno.fname[0] == '\0') {
            break;
        }
        if ((fno.fattrib & AM_DIR) != 0) {
            continue;
        }
        if (!filename_has_disk_ext(fno.fname)) {
            continue;
        }
        if (f_open(&g_image, fno.fname, FA_READ) == FR_OK) {
            g_image_open = true;
            g_image_blocks = (uint32_t)(f_size(&g_image) / NEO1_CFFA1_BLOCK_SIZE);
            opened = true;
#if NEO1_CFFA1_DEBUG
            printf("[cffa1] image '%s' blocks=%lu\n", fno.fname, (unsigned long)g_image_blocks);
#endif
            break;
        }
    }

    f_closedir(&dir);
    return opened;
}

static uint32_t get_requested_block(void) {
    return ((uint32_t)g_regs[NEO1_CFFA1_REG_LBA3] << 24) |
           ((uint32_t)g_regs[NEO1_CFFA1_REG_LBA2] << 16) |
           ((uint32_t)g_regs[NEO1_CFFA1_REG_LBA1] << 8) |
           (uint32_t)g_regs[NEO1_CFFA1_REG_LBA0];
}

static void do_cmd_status(void) {
    if (!open_first_image()) {
        set_error(NEO1_CFFA1_ERR_NODEV);
        return;
    }
    set_ok(0);
}

static void do_cmd_read(void) {
    const uint32_t block = get_requested_block();

    if (!open_first_image()) {
        set_error(NEO1_CFFA1_ERR_NODEV);
        return;
    }

    if (block >= g_image_blocks) {
        set_error(NEO1_CFFA1_ERR_BADBLOCK);
        return;
    }

    const FRESULT seek_res = f_lseek(&g_image, (DWORD)(block * NEO1_CFFA1_BLOCK_SIZE));
    if (seek_res != FR_OK) {
        set_error(NEO1_CFFA1_ERR_IO);
        return;
    }

    UINT nread = 0;
    const FRESULT read_res = f_read(&g_image, g_block_buffer, NEO1_CFFA1_BLOCK_SIZE, &nread);
    if ((read_res != FR_OK) || (nread != NEO1_CFFA1_BLOCK_SIZE)) {
        set_error(NEO1_CFFA1_ERR_IO);
        return;
    }

    g_block_offset = 0;
    set_ok(1);
}

static void do_cmd_write(void) {
    set_error(NEO1_CFFA1_ERR_BADCMD);
}

static void handle_command(uint8_t cmd) {
    switch (cmd) {
        case NEO1_CFFA1_CMD_PRODOS_STATUS:
            do_cmd_status();
            break;
        case NEO1_CFFA1_CMD_PRODOS_READ:
            do_cmd_read();
            break;
        case NEO1_CFFA1_CMD_PRODOS_WRITE:
            do_cmd_write();
            break;
        default:
            set_error(NEO1_CFFA1_ERR_BADCMD);
            break;
    }
}

void neo1_cffa1_init(void) {
    memset(g_regs, 0, sizeof(g_regs));
    memset(g_block_buffer, 0, sizeof(g_block_buffer));

    close_image_if_open();
    g_block_offset = 0;

    // Conservative ATA-like ready state.
    set_ok(0);
}

bool neo1_cffa1_handles_addr(uint16_t addr) {
    if ((addr == NEO1_CFFA1_ID1_ADDR) || (addr == NEO1_CFFA1_ID2_ADDR)) {
        return true;
    }
    return (addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END);
}

uint8_t neo1_cffa1_io_read(uint16_t addr) {
    if (addr == NEO1_CFFA1_ID1_ADDR) {
        return NEO1_CFFA1_ID1_VALUE;
    }
    if (addr == NEO1_CFFA1_ID2_ADDR) {
        return NEO1_CFFA1_ID2_VALUE;
    }
    if ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END)) {
        const uint16_t index = (uint16_t)(addr - NEO1_CFFA1_IO_BASE);
        if (index == NEO1_CFFA1_REG_DATA) {
            uint8_t data = 0x00;
            if (g_block_offset < NEO1_CFFA1_BLOCK_SIZE) {
                data = g_block_buffer[g_block_offset++];
            }
            if (g_block_offset >= NEO1_CFFA1_BLOCK_SIZE) {
                set_ok(0);
            }
            return data;
        }
        return g_regs[index];
    }
    return 0x00;
}

void neo1_cffa1_io_write(uint16_t addr, uint8_t data) {
    if ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END)) {
        const uint16_t index = (uint16_t)(addr - NEO1_CFFA1_IO_BASE);
        g_regs[index] = data;

        if (index == NEO1_CFFA1_REG_STATUS_COMMAND) {
            handle_command(data);
            return;
        }

        // Keep status and alt-status coherent for direct manual pokes.
        if (index == NEO1_CFFA1_REG_DEVCTRL_ALTSTATUS) {
            set_status(data);
        }
    }
}
