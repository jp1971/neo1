#include "input/neo1_hid_keyboard.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum {
    NEO1_HID_MOD_LEFT_CTRL = 1u << 0,
    NEO1_HID_MOD_LEFT_SHIFT = 1u << 1,
    NEO1_HID_MOD_RIGHT_CTRL = 1u << 4,
    NEO1_HID_MOD_RIGHT_SHIFT = 1u << 5,
    NEO1_HID_USAGE_F12 = 0x45,
};

// USB HID usage to US-layout ASCII. This preserves the TinyUSB mapping the
// Neo6502 target previously consumed, including keypad behavior.
static const uint8_t keycode_to_ascii[128][2] = {
    [0x04] = {'a', 'A'}, [0x05] = {'b', 'B'}, [0x06] = {'c', 'C'},
    [0x07] = {'d', 'D'}, [0x08] = {'e', 'E'}, [0x09] = {'f', 'F'},
    [0x0A] = {'g', 'G'}, [0x0B] = {'h', 'H'}, [0x0C] = {'i', 'I'},
    [0x0D] = {'j', 'J'}, [0x0E] = {'k', 'K'}, [0x0F] = {'l', 'L'},
    [0x10] = {'m', 'M'}, [0x11] = {'n', 'N'}, [0x12] = {'o', 'O'},
    [0x13] = {'p', 'P'}, [0x14] = {'q', 'Q'}, [0x15] = {'r', 'R'},
    [0x16] = {'s', 'S'}, [0x17] = {'t', 'T'}, [0x18] = {'u', 'U'},
    [0x19] = {'v', 'V'}, [0x1A] = {'w', 'W'}, [0x1B] = {'x', 'X'},
    [0x1C] = {'y', 'Y'}, [0x1D] = {'z', 'Z'},
    [0x1E] = {'1', '!'}, [0x1F] = {'2', '@'}, [0x20] = {'3', '#'},
    [0x21] = {'4', '$'}, [0x22] = {'5', '%'}, [0x23] = {'6', '^'},
    [0x24] = {'7', '&'}, [0x25] = {'8', '*'}, [0x26] = {'9', '('},
    [0x27] = {'0', ')'}, [0x28] = {'\r', '\r'}, [0x29] = {0x1B, 0x1B},
    [0x2A] = {'\b', '\b'}, [0x2B] = {'\t', '\t'}, [0x2C] = {' ', ' '},
    [0x2D] = {'-', '_'}, [0x2E] = {'=', '+'}, [0x2F] = {'[', '{'},
    [0x30] = {']', '}'}, [0x31] = {'\\', '|'}, [0x32] = {'#', '~'},
    [0x33] = {';', ':'}, [0x34] = {'\'', '"'}, [0x35] = {'`', '~'},
    [0x36] = {',', '<'}, [0x37] = {'.', '>'}, [0x38] = {'/', '?'},
    [0x54] = {'/', '/'}, [0x55] = {'*', '*'}, [0x56] = {'-', '-'},
    [0x57] = {'+', '+'}, [0x58] = {'\r', '\r'}, [0x59] = {'1', 0},
    [0x5A] = {'2', 0}, [0x5B] = {'3', 0}, [0x5C] = {'4', 0},
    [0x5D] = {'5', '5'}, [0x5E] = {'6', 0}, [0x5F] = {'7', 0},
    [0x60] = {'8', 0}, [0x61] = {'9', 0}, [0x62] = {'0', 0},
    [0x63] = {'.', 0},
};

static bool neo1_hid_key_was_down(
    const neo1_hid_keyboard_t* keyboard,
    uint8_t keycode)
{
    for (size_t i = 0; i < NEO1_HID_KEY_COUNT; ++i) {
        if (keyboard->previous_keycodes[i] == keycode) {
            return true;
        }
    }
    return false;
}

static void neo1_hid_emit(
    neo1_hid_event_handler_t handler,
    void* user_data,
    neo1_hid_event_kind_t kind,
    uint8_t character)
{
    if (handler) {
        const neo1_hid_event_t event = {
            .kind = kind,
            .character = character,
        };
        handler(&event, user_data);
    }
}

void neo1_hid_keyboard_reset(neo1_hid_keyboard_t* keyboard) {
    if (keyboard) {
        memset(keyboard, 0, sizeof(*keyboard));
    }
}

void neo1_hid_keyboard_process(
    neo1_hid_keyboard_t* keyboard,
    uint8_t modifiers,
    const uint8_t keycodes[NEO1_HID_KEY_COUNT],
    neo1_hid_event_handler_t handler,
    void* user_data)
{
    if (!keyboard || !keycodes) {
        return;
    }

    const bool shift =
        (modifiers & (NEO1_HID_MOD_LEFT_SHIFT | NEO1_HID_MOD_RIGHT_SHIFT)) != 0;
    const bool ctrl =
        (modifiers & (NEO1_HID_MOD_LEFT_CTRL | NEO1_HID_MOD_RIGHT_CTRL)) != 0;

    for (size_t i = 0; i < NEO1_HID_KEY_COUNT; ++i) {
        const uint8_t keycode = keycodes[i];
        if (keycode == 0 || neo1_hid_key_was_down(keyboard, keycode)) {
            continue;
        }

        if (keycode == NEO1_HID_USAGE_F12) {
            neo1_hid_emit(handler, user_data, NEO1_HID_EVENT_F12, 0);
            continue;
        }
        if (keycode >= 128) {
            continue;
        }

        uint8_t character = keycode_to_ascii[keycode][shift ? 1 : 0];
        if (ctrl && character != 0) {
            uint8_t upper = character;
            if (upper >= 'a' && upper <= 'z') {
                upper = (uint8_t)(upper - ('a' - 'A'));
            }
            if (upper >= 'A' && upper <= 'Z') {
                character = (uint8_t)(upper - '@');
            }
        }
        if (character != 0) {
            neo1_hid_emit(
                handler, user_data, NEO1_HID_EVENT_CHARACTER, character);
        }
    }

    memcpy(keyboard->previous_keycodes, keycodes,
           sizeof(keyboard->previous_keycodes));
}
