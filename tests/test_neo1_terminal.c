#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "terminal/neo1_terminal.h"
#include "neo1_terminal_pico.h"
#include "neo1_terminal_sdl.h"

static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static void check_row_is(const neo1_terminal_t* term, uint32_t row, uint8_t ch) {
    for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
        CHECK(term->chars[row][col] == ch);
    }
}

static void test_clear(void) {
    neo1_terminal_t term;
    memset(&term, 0xA5, sizeof(term));
    neo1_terminal_clear(&term);

    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 0);
    for (uint32_t row = 0; row < NEO1_TERM_ROWS; row++) {
        check_row_is(&term, row, ' ');
    }
}

static void test_glyph_wrap_and_high_bit(void) {
    neo1_terminal_t term;
    neo1_terminal_clear(&term);

    for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
        neo1_terminal_put_glyph(&term, (uint8_t)('A' + (col % 26)));
    }
    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 1);
    CHECK(term.chars[0][0] == 'A');
    CHECK(term.chars[0][NEO1_TERM_COLS - 1] == (uint8_t)('A' + 13));

    neo1_terminal_put_glyph(&term, 0xC1);
    CHECK(term.chars[1][0] == 0xC1);
    CHECK(term.cursor_x == 1);
    CHECK(term.cursor_y == 1);
}

static void test_newline_and_scroll(void) {
    neo1_terminal_t term;
    neo1_terminal_clear(&term);
    for (uint32_t row = 0; row < NEO1_TERM_ROWS; row++) {
        memset(term.chars[row], (int)('A' + row), NEO1_TERM_COLS);
    }
    term.cursor_x = 7;
    term.cursor_y = NEO1_TERM_ROWS - 1;

    neo1_terminal_newline(&term);

    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == NEO1_TERM_ROWS - 1);
    for (uint32_t row = 0; row < NEO1_TERM_ROWS - 1; row++) {
        check_row_is(&term, row, (uint8_t)('B' + row));
    }
    check_row_is(&term, NEO1_TERM_ROWS - 1, ' ');
}

static void test_backspace(void) {
    neo1_terminal_t term;
    neo1_terminal_clear(&term);
    neo1_terminal_put_glyph(&term, 'X');
    neo1_terminal_backspace(&term);
    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 0);
    CHECK(term.chars[0][0] == ' ');

    term.chars[0][0] = 'Y';
    neo1_terminal_backspace(&term);
    CHECK(term.cursor_x == 0);
    CHECK(term.chars[0][0] == 'Y');
}

static void test_pico_policy(void) {
    neo1_terminal_t term;
    neo1_terminal_clear(&term);
    neo1_terminal_pico_putc(&term, 'A');
    neo1_terminal_pico_putc(&term, 0x08);
    neo1_terminal_pico_putc(&term, '\n');
    CHECK(term.cursor_x == 1);
    CHECK(term.cursor_y == 0);
    CHECK(term.chars[0][0] == 'A');

    neo1_terminal_pico_putc(&term, '\r');
    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 1);
    neo1_terminal_pico_putc(&term, 0x0C);
    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 0);
    check_row_is(&term, 0, ' ');
}

static void test_sdl_policy(void) {
    neo1_terminal_t term;
    neo1_terminal_clear(&term);
    neo1_terminal_sdl_putc(&term, 'A');
    neo1_terminal_sdl_putc(&term, '\n');
    neo1_terminal_sdl_putc(&term, 0x0C);
    CHECK(term.cursor_x == 1);
    CHECK(term.cursor_y == 0);
    CHECK(term.chars[0][0] == 'A');

    neo1_terminal_sdl_putc(&term, 0x08);
    CHECK(term.cursor_x == 0);
    CHECK(term.chars[0][0] == ' ');
    neo1_terminal_sdl_putc(&term, '\r');
    CHECK(term.cursor_x == 0);
    CHECK(term.cursor_y == 1);
}

int main(void) {
    test_clear();
    test_glyph_wrap_and_high_bit();
    test_newline_and_scroll();
    test_backspace();
    test_pico_policy();
    test_sdl_policy();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_terminal_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts("neo1_terminal_tests: shared grid and target policies preserved");
    return 0;
}
