#include "terminal/neo1_terminal.h"

#include <string.h>

static void neo1_terminal_scroll(neo1_terminal_t* term) {
    memmove(&term->chars[0][0],
            &term->chars[1][0],
            (NEO1_TERM_ROWS - 1) * NEO1_TERM_COLS);
    memset(&term->chars[NEO1_TERM_ROWS - 1][0], ' ', NEO1_TERM_COLS);
    term->cursor_y = NEO1_TERM_ROWS - 1;
}

void neo1_terminal_clear(neo1_terminal_t* term) {
    memset(term->chars, ' ', sizeof(term->chars));
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void neo1_terminal_newline(neo1_terminal_t* term) {
    term->cursor_x = 0;
    if (term->cursor_y + 1 < NEO1_TERM_ROWS) {
        term->cursor_y++;
    } else {
        neo1_terminal_scroll(term);
    }
}

void neo1_terminal_put_glyph(neo1_terminal_t* term, uint8_t ch) {
    term->chars[term->cursor_y][term->cursor_x] = ch;
    if (term->cursor_x + 1 < NEO1_TERM_COLS) {
        term->cursor_x++;
    } else {
        neo1_terminal_newline(term);
    }
}

void neo1_terminal_backspace(neo1_terminal_t* term) {
    if (term->cursor_x > 0) {
        term->cursor_x--;
        term->chars[term->cursor_y][term->cursor_x] = ' ';
    }
}
