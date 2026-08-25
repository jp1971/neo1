#include "neo1_terminal_pico.h"

#include <stdio.h>
#include <string.h>

void neo1_terminal_pico_putc(neo1_terminal_t* term, uint8_t ch) {
    if (ch == '\r') {
        neo1_terminal_newline(term);
    } else if (ch == 0x0C) {
        neo1_terminal_clear(term);
    } else if ((ch >= 32) && (ch <= 126)) {
        neo1_terminal_put_glyph(term, ch);
    }
}

void neo1_terminal_pico_dump(const neo1_terminal_t* term) {
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

void neo1_terminal_pico_render_to_framebuffer(
    const neo1_terminal_t* term,
    const uint8_t* character_rom,
    uint8_t fb[NEO1_FB_HEIGHT][NEO1_FB_WIDTH]) {

    memset(fb, 0, NEO1_FB_HEIGHT * NEO1_FB_WIDTH);

    for (uint32_t row = 0; row < NEO1_TERM_ROWS; row++) {
        for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
            const uint8_t ch = term->chars[row][col];
            const uint32_t glyph_index = ((uint32_t)ch & 0x7F) * NEO1_CHAR_HEIGHT;

            for (uint32_t gy = 0; gy < NEO1_CHAR_HEIGHT; gy++) {
                const uint8_t bits = character_rom[glyph_index + gy] & 0x7F;
                const uint32_t y = row * NEO1_CHAR_HEIGHT + gy;

                for (uint32_t gx = 0; gx < 7; gx++) {
                    const uint32_t x = col * NEO1_CHAR_WIDTH + gx;
                    fb[y][x] = (bits & (1u << gx)) ? 1 : 0;
                }
            }
        }
    }
}
