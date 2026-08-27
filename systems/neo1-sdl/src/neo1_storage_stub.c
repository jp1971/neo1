#include <string.h>

#include "neo1_platform.h"
#include "neo1_storage_stub.h"

// SDL storage adapters. MSC supplies raw-image I/O to the shared Neo1 register
// protocol; VCFFA1 remains an SDL-local compatibility state machine.
//
// The MSC backend permits sector READ/WRITE without OPEN, has an empty
// directory, and cannot truncate the raw image after a short logical write.
// Command/status/data sequencing is owned by the shared protocol.
//
// VCFFA1 deviations: STATUS probes raw block zero; READ/WRITE delegate range and
// media checks to the platform; WRITE does not preflight media or expose a
// distinct write-protect error. Direct ALTSTATUS/DEVCTRL writes do not update
// STATUS. Neither emulated device asserts IRQ or NMI.

static uint8_t g_cffa_regs[NEO1_CFFA1_IO_SIZE];
static uint8_t g_cffa_buffer[512];
static uint16_t g_cffa_offset;
static uint32_t g_cffa_write_lba;
static bool g_cffa_write_pending;

static uint8_t neo1_sdl_msc_open(void* context,
                                  const char* name,
                                  uint32_t* out_size) {
    (void)context;
    (void)name;
    *out_size = 0;
    return 0;
}

static void neo1_sdl_msc_close(void* context) {
    (void)context;
}

static uint8_t neo1_sdl_msc_read(void* context,
                                  uint16_t sector,
                                  uint8_t* buffer,
                                  size_t* out_size) {
    (void)context;
    if (!neo1_platform_disk_read(sector, buffer, 1)) {
        return 1;
    }
    *out_size = NEO1_MSC_SECTOR_SIZE;
    return 0;
}

static uint8_t neo1_sdl_msc_write(void* context,
                                   uint16_t sector,
                                   const uint8_t* buffer,
                                   uint16_t size,
                                   uint32_t* out_file_size) {
    (void)context;
    (void)size;
    if (!neo1_platform_disk_write(sector, buffer, 1)) {
        return 1;
    }
    *out_file_size = 0;
    return 0;
}

static uint8_t neo1_sdl_msc_dir_open(void* context) {
    (void)context;
    return 0;
}

static uint8_t neo1_sdl_msc_dir_next(void* context,
                                      neo1_msc_dir_entry_t* out_entry) {
    (void)context;
    out_entry->valid = false;
    return 0;
}

static void neo1_sdl_msc_dir_close(void* context) {
    (void)context;
}

static uint8_t neo1_sdl_msc_delete(void* context, const char* name) {
    (void)context;
    (void)name;
    return 1;
}

static const neo1_msc_backend_ops_t neo1_sdl_msc_ops = {
    .open = neo1_sdl_msc_open,
    .close = neo1_sdl_msc_close,
    .read_sector = neo1_sdl_msc_read,
    .write_sector = neo1_sdl_msc_write,
    .dir_open = neo1_sdl_msc_dir_open,
    .dir_next = neo1_sdl_msc_dir_next,
    .dir_close = neo1_sdl_msc_dir_close,
    .delete_file = neo1_sdl_msc_delete,
};

const neo1_msc_backend_t* neo1_sdl_msc_backend(void) {
    static const neo1_msc_backend_t backend = {
        .ops = &neo1_sdl_msc_ops,
        .context = NULL,
        .flags = NEO1_MSC_BACKEND_OPEN_OPTIONAL,
        .not_found_error = 1,
        .invalid_state_error = 1,
    };
    return &backend;
}

static uint32_t cffa_lba(void) {
    return ((uint32_t)g_cffa_regs[NEO1_CFFA1_REG_LBA3] << 24) |
           ((uint32_t)g_cffa_regs[NEO1_CFFA1_REG_LBA2] << 16) |
           ((uint32_t)g_cffa_regs[NEO1_CFFA1_REG_LBA1] << 8) |
           ((uint32_t)g_cffa_regs[NEO1_CFFA1_REG_LBA0]);
}

static void cffa_set_status(uint8_t status) {
    g_cffa_regs[NEO1_CFFA1_REG_STATUS_COMMAND] = status;
    g_cffa_regs[NEO1_CFFA1_REG_DEVCTRL_ALTSTATUS] = status;
}

static void cffa_set_ok(uint8_t with_drq) {
    g_cffa_regs[NEO1_CFFA1_REG_ERROR_FEATURE] = NEO1_CFFA1_ERR_OK;
    cffa_set_status(NEO1_CFFA1_STATUS_DRDY | NEO1_CFFA1_STATUS_DSC |
                    (with_drq ? NEO1_CFFA1_STATUS_DRQ : 0));
}

static void cffa_set_error(uint8_t code) {
    g_cffa_regs[NEO1_CFFA1_REG_ERROR_FEATURE] = code;
    cffa_set_status(NEO1_CFFA1_STATUS_DRDY | NEO1_CFFA1_STATUS_DSC | NEO1_CFFA1_STATUS_ERR);
}

void neo1_cffa1_init(void) {
    memset(g_cffa_regs, 0, sizeof(g_cffa_regs));
    memset(g_cffa_buffer, 0, sizeof(g_cffa_buffer));
    g_cffa_offset = 0;
    g_cffa_write_lba = 0;
    g_cffa_write_pending = false;
    cffa_set_ok(0);
}

uint8_t neo1_cffa1_io_read(uint16_t addr) {
    if (addr == NEO1_CFFA1_ID1_ADDR) {
        return NEO1_CFFA1_ID1_VALUE;
    }
    if (addr == NEO1_CFFA1_ID2_ADDR) {
        return NEO1_CFFA1_ID2_VALUE;
    }
    if ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END)) {
        const unsigned idx = (unsigned)(addr - NEO1_CFFA1_IO_BASE);
        if (idx == NEO1_CFFA1_REG_DATA) {
            uint8_t out = 0;
            if (g_cffa_offset < sizeof(g_cffa_buffer)) {
                out = g_cffa_buffer[g_cffa_offset++];
            }
            if (!g_cffa_write_pending && (g_cffa_offset >= sizeof(g_cffa_buffer))) {
                cffa_set_ok(0);
            }
            return out;
        }
        return g_cffa_regs[idx];
    }
    return 0;
}

void neo1_cffa1_io_write(uint16_t addr, uint8_t data) {
    if ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END)) {
        const unsigned idx = (unsigned)(addr - NEO1_CFFA1_IO_BASE);

        if ((idx == NEO1_CFFA1_REG_DATA) && g_cffa_write_pending) {
            if (g_cffa_offset < sizeof(g_cffa_buffer)) {
                g_cffa_buffer[g_cffa_offset++] = data;
            }
            if (g_cffa_offset >= sizeof(g_cffa_buffer)) {
                if (neo1_platform_disk_write(g_cffa_write_lba, g_cffa_buffer, 1)) {
                    cffa_set_ok(0);
                } else {
                    cffa_set_error(NEO1_CFFA1_ERR_IO);
                }
                g_cffa_write_pending = false;
                g_cffa_offset = 0;
            }
            return;
        }

        g_cffa_regs[idx] = data;

        if (idx == NEO1_CFFA1_REG_STATUS_COMMAND) {
            const uint32_t lba = cffa_lba();
            g_cffa_offset = 0;
            g_cffa_write_pending = false;

            switch (data) {
                case NEO1_CFFA1_CMD_PRODOS_STATUS: {
                    uint8_t probe[512];
                    if (neo1_platform_disk_read(0, probe, 1)) {
                        cffa_set_ok(0);
                    } else {
                        cffa_set_error(NEO1_CFFA1_ERR_NODEV);
                    }
                    break;
                }

                case NEO1_CFFA1_CMD_PRODOS_READ:
                    if (neo1_platform_disk_read(lba, g_cffa_buffer, 1)) {
                        cffa_set_ok(1);
                    } else {
                        cffa_set_error(NEO1_CFFA1_ERR_IO);
                    }
                    break;

                case NEO1_CFFA1_CMD_PRODOS_WRITE:
                    g_cffa_write_lba = lba;
                    g_cffa_write_pending = true;
                    cffa_set_ok(1);
                    break;

                default:
                    cffa_set_error(NEO1_CFFA1_ERR_BADCMD);
                    break;
            }
        }
    }
}
