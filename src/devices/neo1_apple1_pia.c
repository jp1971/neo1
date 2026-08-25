#include "devices/neo1_apple1_pia.h"

#include <string.h>

static uint16_t neo1_apple1_pia_normalize_addr(uint16_t addr) {
    switch (addr) {
        case NEO1_IO_DSP_ALT:
            return NEO1_IO_DSP;
        case NEO1_IO_DSPCR_ALT:
            return NEO1_IO_DSPCR;
        default:
            return addr;
    }
}

void neo1_apple1_pia_reset(neo1_apple1_pia_t* pia) {
    memset(pia, 0, sizeof(*pia));
}

bool neo1_apple1_pia_handles_addr(uint16_t addr) {
    switch (addr) {
        case NEO1_IO_KBD:
        case NEO1_IO_KBDCR:
        case NEO1_IO_DSP:
        case NEO1_IO_DSPCR:
        case NEO1_IO_DSP_ALT:
        case NEO1_IO_DSPCR_ALT:
            return true;
        default:
            return false;
    }
}

uint8_t neo1_apple1_pia_read(neo1_apple1_pia_t* pia, uint16_t addr) {
    switch (neo1_apple1_pia_normalize_addr(addr)) {
        case NEO1_IO_KBD:
            if ((pia->keyboard_control & 0x04) == 0) {
                return pia->keyboard_ddr;
            } else {
                const uint8_t data = pia->keyboard_latch;
                pia->keyboard_latch = 0;
                pia->keyboard_data = 0;
                return data;
            }
        case NEO1_IO_KBDCR:
            return (pia->keyboard_control & 0x7F) |
                   ((pia->keyboard_latch != 0) ? 0x80 : 0x00);
        case NEO1_IO_DSP:
            if ((pia->display_control & 0x04) == 0) {
                return pia->display_ddr;
            }
            return 0x00;
        case NEO1_IO_DSPCR:
            return (pia->display_control & 0x7F) | 0x80;
        default:
            return 0x00;
    }
}

bool neo1_apple1_pia_write(
    neo1_apple1_pia_t* pia,
    uint16_t addr,
    uint8_t data,
    uint8_t* display_byte)
{
    switch (neo1_apple1_pia_normalize_addr(addr)) {
        case NEO1_IO_KBD:
            if ((pia->keyboard_control & 0x04) == 0) {
                pia->keyboard_ddr = data;
            } else {
                pia->keyboard_data = data;
            }
            return false;
        case NEO1_IO_KBDCR:
            pia->keyboard_control = data;
            return false;
        case NEO1_IO_DSP:
            if ((pia->display_control & 0x04) == 0) {
                pia->display_ddr = data;
                return false;
            }
            pia->display_data = data;
            if (display_byte) {
                *display_byte = data;
            }
            return true;
        case NEO1_IO_DSPCR:
            pia->display_control = data;
            return false;
        default:
            return false;
    }
}

void neo1_apple1_pia_key_down(neo1_apple1_pia_t* pia, uint8_t ascii) {
    if (ascii == '\n') {
        ascii = '\r';
    }
    if (pia->keyboard_latch == 0) {
        pia->keyboard_latch = ascii | 0x80;
    }
}
