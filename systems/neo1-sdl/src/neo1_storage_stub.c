#include <string.h>

#include "../../neo1-pico/src/neo1_msc.h"
#include "../../neo1-pico/src/neo1_cffa1.h"
#include "neo1_platform.h"

// SDL storage accommodation. Both nominal devices map to the one raw host image
// exposed by neo1_platform_disk_read/write; this file duplicates their visible
// state machines rather than providing a backend for the Pico implementations.
// It must not be treated as equivalent VACI/VCFFA1 behavior or as the intended
// shared storage boundary.
//
// MSC deviations: READ and WRITE address raw 512-byte sectors. OPEN, CLOSE,
// directory, indexed-file, and unknown commands succeed as no-ops; SIZE, INFO,
// and INDEX have no file-service effect. WRITE accepts either buffer bytes before
// the command or exactly 512 bytes after it is armed. It never reports BUSY.
//
// VCFFA1 deviations: STATUS probes raw block zero; READ/WRITE delegate range and
// media checks to the platform; WRITE does not preflight media or expose a
// distinct write-protect error. Direct ALTSTATUS/DEVCTRL writes do not update
// STATUS. Neither emulated device asserts IRQ or NMI.

static uint8_t g_msc_regs[9];
static uint8_t g_msc_buffer[512];
static uint16_t g_msc_offset;
static bool g_msc_write_armed;
static bool g_msc_data_dirty;

static uint8_t g_cffa_regs[NEO1_CFFA1_IO_SIZE];
static uint8_t g_cffa_buffer[512];
static uint16_t g_cffa_offset;
static uint32_t g_cffa_write_lba;
static bool g_cffa_write_pending;

static uint16_t msc_sector(void) {
    return (uint16_t)g_msc_regs[(unsigned)(NEO1_IO_MSC_SECTOR_LO - NEO1_IO_MSC_CMD)] |
           ((uint16_t)g_msc_regs[(unsigned)(NEO1_IO_MSC_SECTOR_HI - NEO1_IO_MSC_CMD)] << 8);
}

static void msc_set_status(uint8_t status) {
    g_msc_regs[(unsigned)(NEO1_IO_MSC_STATUS - NEO1_IO_MSC_CMD)] = status;
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

void neo1_msc_init(void) {
    memset(g_msc_regs, 0, sizeof(g_msc_regs));
    memset(g_msc_buffer, 0, sizeof(g_msc_buffer));
    g_msc_offset = 0;
    g_msc_write_armed = false;
    g_msc_data_dirty = false;
    msc_set_status(NEO1_MSC_STATUS_READY);
}

uint8_t neo1_msc_io_read(uint16_t addr) {
    if (addr == NEO1_IO_MSC_DATA) {
        uint8_t out = 0;
        if (g_msc_offset < sizeof(g_msc_buffer)) {
            out = g_msc_buffer[g_msc_offset++];
        }
        return out;
    }

    if ((addr >= NEO1_IO_MSC_CMD) && (addr <= NEO1_IO_MSC_SIZE_HI)) {
        return g_msc_regs[(unsigned)(addr - NEO1_IO_MSC_CMD)];
    }
    return 0;
}

void neo1_msc_io_write(uint16_t addr, uint8_t data) {
    if (addr == NEO1_IO_MSC_DATA) {
        if (g_msc_offset < sizeof(g_msc_buffer)) {
            g_msc_buffer[g_msc_offset++] = data;
            g_msc_data_dirty = true;
        }
        if (g_msc_write_armed && (g_msc_offset >= sizeof(g_msc_buffer))) {
            if (neo1_platform_disk_write(msc_sector(), g_msc_buffer, 1)) {
                msc_set_status(NEO1_MSC_STATUS_READY);
            } else {
                msc_set_status((uint8_t)(NEO1_MSC_STATUS_ERROR | 1u));
            }
            g_msc_write_armed = false;
            g_msc_offset = 0;
            g_msc_data_dirty = false;
        }
        return;
    }

    if ((addr >= NEO1_IO_MSC_CMD) && (addr <= NEO1_IO_MSC_SIZE_HI)) {
        const unsigned idx = (unsigned)(addr - NEO1_IO_MSC_CMD);
        g_msc_regs[idx] = data;

        if (addr == NEO1_IO_MSC_CMD) {
            switch (data) {
                case NEO1_MSC_CMD_READ:
                    g_msc_offset = 0;
                    if (neo1_platform_disk_read(msc_sector(), g_msc_buffer, 1)) {
                        msc_set_status(NEO1_MSC_STATUS_READY);
                    } else {
                        msc_set_status((uint8_t)(NEO1_MSC_STATUS_ERROR | 1u));
                    }
                    g_msc_write_armed = false;
                    break;

                case NEO1_MSC_CMD_WRITE:
                    if (g_msc_data_dirty) {
                        if (neo1_platform_disk_write(msc_sector(), g_msc_buffer, 1)) {
                            msc_set_status(NEO1_MSC_STATUS_READY);
                        } else {
                            msc_set_status((uint8_t)(NEO1_MSC_STATUS_ERROR | 1u));
                        }
                        g_msc_data_dirty = false;
                        g_msc_offset = 0;
                        g_msc_write_armed = false;
                    } else {
                        g_msc_offset = 0;
                        g_msc_write_armed = true;
                        msc_set_status(NEO1_MSC_STATUS_READY);
                    }
                    break;

                default:
                    // File-oriented and unknown commands have no raw-image action.
                    msc_set_status(NEO1_MSC_STATUS_READY);
                    g_msc_write_armed = false;
                    break;
            }
        }
    }
}

void neo1_cffa1_init(void) {
    memset(g_cffa_regs, 0, sizeof(g_cffa_regs));
    memset(g_cffa_buffer, 0, sizeof(g_cffa_buffer));
    g_cffa_offset = 0;
    g_cffa_write_lba = 0;
    g_cffa_write_pending = false;
    cffa_set_ok(0);
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
