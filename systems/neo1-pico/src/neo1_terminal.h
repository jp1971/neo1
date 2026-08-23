#pragma once

// neo1_terminal.h
//
// Pico-owned software text grid used by the DVI and UART-facing runner.
//
// This is not part of the Apple-1 PIA register model, and the SDL target has a
// separate terminal implementation with different control-character behavior.
// The grid owns 40x24 character cells and a cursor. CR advances to the next row,
// form feed clears, printable ASCII writes a cell, and all other bytes—including
// backspace and LF—are ignored. Column overflow wraps and the final row scrolls.
//
// The retained framebuffer helper renders 8x8 slots using seven font bits; the
// active PicoDVI path instead snapshots the cells and rasterizes them directly
// in neo1_video.c.

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

// Append one character with Pico terminal control behavior:
// - '\r'      -> newline
// - 0x0C      -> clear screen
// - printable -> placed at cursor and advances with wrapping/scroll
void neo1_terminal_putc(neo1_terminal_t* term, uint8_t ch);

// Debug helper: print terminal contents and cursor position to stdout.
void neo1_terminal_dump(const neo1_terminal_t* term);

// Rasterize into a caller-owned 320x192 byte-per-pixel monochrome array. This
// utility is not called by the active DVI renderer. `character_rom` must contain
// 128 glyphs of eight rows; only bits 0-6 are drawn and column 7 remains blank.
void neo1_terminal_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]);
