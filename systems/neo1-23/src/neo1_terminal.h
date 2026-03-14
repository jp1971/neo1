#pragma once

#include <stdint.h>

#define NEO1_TERM_COLS    40
#define NEO1_TERM_ROWS    24
#define NEO1_CHAR_WIDTH   8
#define NEO1_CHAR_HEIGHT  8
#define NEO1_FB_WIDTH     (NEO1_TERM_COLS * NEO1_CHAR_WIDTH)
#define NEO1_FB_HEIGHT    (NEO1_TERM_ROWS * NEO1_CHAR_HEIGHT)

typedef struct {
    uint8_t chars[NEO1_TERM_ROWS][NEO1_TERM_COLS];
    uint8_t cursor_x;
    uint8_t cursor_y;
} neo1_terminal_t;

void neo1_terminal_clear(neo1_terminal_t* term);
void neo1_terminal_putc(neo1_terminal_t* term, uint8_t ch);
void neo1_terminal_dump(const neo1_terminal_t* term);
void neo1_terminal_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]);