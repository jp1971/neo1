#pragma once

#include <stdbool.h>
#include <stdint.h>

// Optional callback for delivering decoded ASCII to the system.
typedef void (*apple1_usb_char_handler_t)(uint8_t ch, void* user_data);

// Initialize TinyUSB host-side keyboard support for Neo 1.
void apple1_usb_init(apple1_usb_char_handler_t handler, void* user_data);

// Pump TinyUSB host tasks. Call this regularly from the main loop.
void apple1_usb_task(void);

// Returns true if a keyboard is currently attached.
bool apple1_usb_keyboard_mounted(void);