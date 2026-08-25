#include "neo1_terminal_sdl.h"

void neo1_terminal_sdl_putc(neo1_terminal_t* term, uint8_t ch) {
    if (ch == '\r') {
        neo1_terminal_newline(term);
    } else if (ch == 0x08) {
        neo1_terminal_backspace(term);
    } else if (ch >= 0x20) {
        neo1_terminal_put_glyph(term, ch);
    }
}
