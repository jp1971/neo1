#pragma once

// Shared host-side text grid used by the PicoDVI and SDL display adapters.
//
// This module is not the Apple-1 PIA and does not interpret control bytes. It
// owns only the 40x24 cells, cursor, wrapping, scrolling, and explicit editing
// primitives. Callers retain byte normalization and target-specific policy.

#include <stdint.h>

#define NEO1_TERM_COLS 40
#define NEO1_TERM_ROWS 24

typedef struct {
    uint8_t chars[NEO1_TERM_ROWS][NEO1_TERM_COLS];
    uint8_t cursor_x;
    uint8_t cursor_y;
} neo1_terminal_t;

// Fill every cell with a space and reset the cursor to (0, 0).
void neo1_terminal_clear(neo1_terminal_t* term);

// Move to column zero on the next row, scrolling once at the bottom.
void neo1_terminal_newline(neo1_terminal_t* term);

// Place one already-approved glyph and advance with wrap/scroll. All eight
// bits are retained; target callbacks own any high-bit stripping.
void neo1_terminal_put_glyph(neo1_terminal_t* term, uint8_t ch);

// Move left and replace that cell with a space. Column zero is unchanged.
void neo1_terminal_backspace(neo1_terminal_t* term);
