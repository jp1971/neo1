#pragma once

// neo1_usb.h
//
// Pico-owned TinyUSB host adapter for keyboard input and USB mass-storage
// lifecycle. It supplies transport events to the runner and FatFs; it does not
// own Apple-1 keyboard registers or either 6502-visible storage protocol.
//
// Responsibilities:
// - initialize/pump TinyUSB host stack
// - edge-detect six-key HID reports and decode new presses into ASCII/control
// - expose keyboard/MSC mounted state for the main loop
// - mount/unmount FatFs volume "0:" for USB MSC media
// - provide a diagnostic-only root-directory listing

#include <stdbool.h>
#include <stdint.h>

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS 0
#endif

// Callback invoked from TinyUSB report processing. The runner owns subsequent
// case conversion, LF normalization, reset handling, and machine injection.
typedef void (*neo1_usb_char_handler_t)(uint8_t ch, void* user_data);

// Initialize TinyUSB host support and register character callback.
// Must be called once before `neo1_usb_task()`.
void neo1_usb_init(neo1_usb_char_handler_t handler, void* user_data);

// Pump TinyUSB host callbacks. The Pico runner calls this between bus batches.
void neo1_usb_task(void);

// Return the HID keyboard attachment state maintained by mount callbacks.
bool neo1_usb_keyboard_mounted(void);

// Return the USB MSC attachment state maintained by mount callbacks.
bool neo1_usb_msc_mounted(void);

// Diagnostic helper: list root directory entries of mounted MSC media.
#if NEO1_DIAGNOSTICS
void neo1_msc_list_files(void);
#endif
