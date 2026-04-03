// neo1_terminal.c
//
// Software text terminal implementation for Neo1.
//
// Responsibilities:
// - maintain a 40x24 character buffer and cursor state
// - implement newline, wrapping, and scrolling behavior
// - provide a debug dump view
// - render character cells into the monochrome framebuffer consumed by video

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "neo1_terminal.h"

// Scroll one text row upward and clear the last row.
static void neo1_terminal_scroll(neo1_terminal_t* term) {
    memmove(&term->chars[0][0],
            &term->chars[1][0],
            (NEO1_TERM_ROWS - 1) * NEO1_TERM_COLS);
    memset(&term->chars[NEO1_TERM_ROWS - 1][0], ' ', NEO1_TERM_COLS);
    term->cursor_y = NEO1_TERM_ROWS - 1;
}

// Move to column 0 on the next row, scrolling if already on the last row.
static void neo1_terminal_newline(neo1_terminal_t* term) {
    term->cursor_x = 0;
    if (term->cursor_y + 1 < NEO1_TERM_ROWS) {
        term->cursor_y++;
    } else {
        neo1_terminal_scroll(term);
    }
}

void neo1_terminal_clear(neo1_terminal_t* term) {
    memset(term->chars, ' ', sizeof(term->chars));
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void neo1_terminal_putc(neo1_terminal_t* term, uint8_t ch) {
    // CR is the runtime's line-advance convention.
    if (ch == '\r') {
        neo1_terminal_newline(term);
        return;
    }

    // Form feed clears the software terminal.
    if (ch == 0x0C) {
        neo1_terminal_clear(term);
        return;
    }

    // Ignore non-printable / non-ASCII bytes.
    if ((ch < 32) || (ch > 126)) {
        return;
    }

    term->chars[term->cursor_y][term->cursor_x] = ch;

    if (term->cursor_x + 1 < NEO1_TERM_COLS) {
        term->cursor_x++;
    } else {
        neo1_terminal_newline(term);
    }
}

void neo1_terminal_dump(const neo1_terminal_t* term) {
    printf("\n[term] dump begin\n");
    for (uint32_t row = 0; row < NEO1_TERM_ROWS; row++) {
        printf("[term] |");
        for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
            uint8_t ch = term->chars[row][col];
            if ((ch < 32) || (ch > 126)) {
                ch = ' ';
            }
            putchar((int)ch);
        }
        printf("|\n");
    }
    printf("[term] cursor=(%u,%u)\n\n",
           (unsigned)term->cursor_x,
           (unsigned)term->cursor_y);
}

void neo1_terminal_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]) {

    // Start with a fully blank 1-bpp framebuffer.
    memset(fb, 0, NEO1_FB_HEIGHT * NEO1_FB_WIDTH);

    for (uint32_t row = 0; row < NEO1_TERM_ROWS; row++) {
        for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
            uint8_t ch = term->chars[row][col];
            uint32_t glyph_index = ((uint32_t)ch & 0x7F) * NEO1_CHAR_HEIGHT;

            for (uint32_t gy = 0; gy < NEO1_CHAR_HEIGHT; gy++) {
                // Character ROM uses 7 visible bits per row for current font data.
                uint8_t bits = character_rom[glyph_index + gy] & 0x7F;
                uint32_t y = row * NEO1_CHAR_HEIGHT + gy;

                for (uint32_t gx = 0; gx < 7; gx++) {
                    uint32_t x = col * NEO1_CHAR_WIDTH + gx;
                    fb[y][x] = (bits & (1u << gx)) ? 1 : 0;
                }
            }
        }
    }
}