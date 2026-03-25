#pragma once
#include <stdint.h>

#define FONT_CHAR_WIDTH  8
#define FONT_CHAR_HEIGHT 8
#define FONT_NUM_CHARS   256

extern const uint8_t console_font_8x8[FONT_NUM_CHARS * FONT_CHAR_HEIGHT];