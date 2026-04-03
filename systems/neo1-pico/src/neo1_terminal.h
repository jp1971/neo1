#pragma once

// neo1_terminal.h
//
// Minimal software text terminal used by Neo1.
//
// This module holds terminal state (character cells + cursor) and provides
// helpers to:
// - clear and append characters with basic control handling
// - dump the current screen for debugging
// - convert terminal cells into a 1-bpp framebuffer for the DVI path
//
// Rendering convention:
// - terminal geometry is 40x24 cells
// - each glyph slot is 8x8 pixels
// - glyph fetch currently uses 7 visible bits per row from the character ROM

#include <stdint.h>

// -----------------------------------------------------------------------------
// terminal geometry
// -----------------------------------------------------------------------------

#define NEO1_TERM_COLS    40
#define NEO1_TERM_ROWS    24
#define NEO1_CHAR_WIDTH   8
#define NEO1_CHAR_HEIGHT  8
#define NEO1_FB_WIDTH     (NEO1_TERM_COLS * NEO1_CHAR_WIDTH)
#define NEO1_FB_HEIGHT    (NEO1_TERM_ROWS * NEO1_CHAR_HEIGHT)

// -----------------------------------------------------------------------------
// types
// -----------------------------------------------------------------------------

typedef struct {
    uint8_t chars[NEO1_TERM_ROWS][NEO1_TERM_COLS];
    uint8_t cursor_x;
    uint8_t cursor_y;
} neo1_terminal_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

// Fill the terminal with spaces and return cursor to (0, 0).
void neo1_terminal_clear(neo1_terminal_t* term);

// Append one character with minimal control behavior:
// - '\r'      -> newline
// - 0x0C      -> clear screen
// - printable -> placed at cursor and advances with wrapping/scroll
void neo1_terminal_putc(neo1_terminal_t* term, uint8_t ch);

// Debug helper: print terminal contents and cursor position to stdout.
void neo1_terminal_dump(const neo1_terminal_t* term);

// Rasterize current terminal cells into a 1-bpp framebuffer.
// `character_rom` is expected to be laid out as 128 glyphs * 8 rows each.
void neo1_terminal_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]);