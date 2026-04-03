#pragma once

// neo1_usb.h
//
// Neo1 USB host module built on TinyUSB.
//
// Responsibilities:
// - initialize/pump TinyUSB host stack
// - decode HID keyboard reports into ASCII-like bytes
// - expose keyboard/MSC mounted state for the main loop
// - provide a small FatFs root-list helper for bring-up/debug

#include <stdbool.h>
#include <stdint.h>

// Optional callback for delivering decoded keyboard bytes to Neo1 runtime.
typedef void (*neo1_usb_char_handler_t)(uint8_t ch, void* user_data);

// Initialize TinyUSB host support and register character callback.
// Must be called once before `neo1_usb_task()`.
void neo1_usb_init(neo1_usb_char_handler_t handler, void* user_data);

// Pump TinyUSB host tasks. Call regularly from the main loop.
void neo1_usb_task(void);

// Returns true if a keyboard is currently attached.
bool neo1_usb_keyboard_mounted(void);

// Returns true if an MSC device is currently mounted.
bool neo1_usb_msc_mounted(void);

// Debug helper: list root directory entries of mounted MSC drive.
void neo1_msc_list_files(void);