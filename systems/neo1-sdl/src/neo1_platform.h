#pragma once

#include <stdbool.h>
#include <stdint.h>

void neo1_platform_init(int width, int height, const char* title);
void neo1_platform_shutdown(void);
void neo1_platform_update_display(const uint32_t* pixels, int width, int height);
bool neo1_platform_poll_key(uint8_t* out_apple1_keycode, bool* out_pressed);
uint64_t neo1_platform_time_us(void);
bool neo1_platform_disk_read(uint32_t lba, uint8_t* buf, uint32_t count);
bool neo1_platform_disk_write(uint32_t lba, const uint8_t* buf, uint32_t count);
bool neo1_platform_should_quit(void);