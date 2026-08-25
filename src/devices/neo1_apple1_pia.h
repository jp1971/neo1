#pragma once

// Minimal Apple-1 keyboard/display device shared by every Neo1 target.
//
// This models only the data-direction/control behavior required by WozMon. It
// is not a complete 6820/6821. Input and output are polled; no IRQ or NMI is
// asserted. The Replica 1 display addresses mirror the Apple-1 DSP registers.

#include <stdbool.h>
#include <stdint.h>

enum {
    NEO1_IO_KBD       = 0xD010,
    NEO1_IO_KBDCR     = 0xD011,
    NEO1_IO_DSP       = 0xD012,
    NEO1_IO_DSPCR     = 0xD013,
    NEO1_IO_DSP_ALT   = 0xD0F2,
    NEO1_IO_DSPCR_ALT = 0xD0F3,
};

typedef struct {
    uint8_t keyboard_latch;
    uint8_t keyboard_control;
    uint8_t keyboard_ddr;
    uint8_t keyboard_data;
    uint8_t display_control;
    uint8_t display_ddr;
    uint8_t display_data;
} neo1_apple1_pia_t;

// Clear all registers and discard pending keyboard input.
void neo1_apple1_pia_reset(neo1_apple1_pia_t* pia);

// Report ownership of the four Apple-1 addresses and two display mirrors.
bool neo1_apple1_pia_handles_addr(uint16_t addr);

// Read a decoded register. KBD and DSP select DDR when control bit 2 is clear.
// A KBD data read consumes pending input. DSP data reads return ready value $00.
uint8_t neo1_apple1_pia_read(neo1_apple1_pia_t* pia, uint16_t addr);

// Write a decoded register. Returns true only when the write emits a display
// byte; when true, display_byte receives the unmodified 6502 value.
bool neo1_apple1_pia_write(
    neo1_apple1_pia_t* pia,
    uint16_t addr,
    uint8_t data,
    uint8_t* display_byte);

// Normalize LF to CR, set bit 7, and retain the first byte until KBD consumes
// it. Additional input is ignored while a byte is pending.
void neo1_apple1_pia_key_down(neo1_apple1_pia_t* pia, uint8_t ascii);
