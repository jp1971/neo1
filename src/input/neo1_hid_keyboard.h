#pragma once

// Pure-C USB HID boot-keyboard report decoder.
//
// The decoder owns only six-key rollover edge detection and US keyboard usage
// translation. It has no TinyUSB, GPIO, machine, or target lifecycle state.

#include <stdint.h>

enum {
    NEO1_HID_KEY_COUNT = 6,
};

typedef struct neo1_hid_keyboard {
    uint8_t previous_keycodes[NEO1_HID_KEY_COUNT];
} neo1_hid_keyboard_t;

typedef enum neo1_hid_event_kind {
    NEO1_HID_EVENT_CHARACTER,
    NEO1_HID_EVENT_F12,
} neo1_hid_event_kind_t;

typedef struct neo1_hid_event {
    neo1_hid_event_kind_t kind;
    uint8_t character;
} neo1_hid_event_t;

typedef void (*neo1_hid_event_handler_t)(
    const neo1_hid_event_t* event,
    void* user_data);

// Forget held keys, as required when a keyboard mounts or unmounts.
void neo1_hid_keyboard_reset(neo1_hid_keyboard_t* keyboard);

// Emit events for newly pressed keys in a standard six-key boot report.
// Modifier bits follow the USB HID keyboard convention.
void neo1_hid_keyboard_process(
    neo1_hid_keyboard_t* keyboard,
    uint8_t modifiers,
    const uint8_t keycodes[NEO1_HID_KEY_COUNT],
    neo1_hid_event_handler_t handler,
    void* user_data);
