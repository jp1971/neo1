#pragma once

// SDL terminal control-byte policy over the shared 40x24 grid.

#include <stdint.h>

#include "terminal/neo1_terminal.h"

// Apply SDL policy after the platform callback has stripped bit 7: CR advances,
// backspace erases, bytes >= $20 are placed, and LF/form feed are ignored.
void neo1_terminal_sdl_putc(neo1_terminal_t* term, uint8_t ch);
