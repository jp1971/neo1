#pragma once

// Pico terminal policy and retained diagnostics/raster helper. Grid state and
// primitive mutations are shared in terminal/neo1_terminal.h.

#include <stdint.h>

#include "terminal/neo1_terminal.h"

#define NEO1_CHAR_WIDTH 8
#define NEO1_CHAR_HEIGHT 8
#define NEO1_FB_WIDTH (NEO1_TERM_COLS * NEO1_CHAR_WIDTH)
#define NEO1_FB_HEIGHT (NEO1_TERM_ROWS * NEO1_CHAR_HEIGHT)

// Apply Pico policy: CR advances, form feed clears, printable ASCII is placed,
// and LF/backspace/other bytes are ignored.
void neo1_terminal_pico_putc(neo1_terminal_t* term, uint8_t ch);

// Debug helper: print terminal contents and cursor position to stdout.
void neo1_terminal_pico_dump(const neo1_terminal_t* term);

// Retained utility, not used by active DVI scanout. Rasterize into a caller-
// owned 320x192 byte-per-pixel monochrome array using seven font bits.
void neo1_terminal_pico_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]);
