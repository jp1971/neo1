// hid_app.c
//
// TinyUSB HID host input bridge for keyboard + selected gamepad devices.
//
// Responsibilities:
// - detect key press/release edges from HID keyboard reports
// - translate keycodes into ASCII/raw key events for emulator input path
// - parse supported gamepad report formats into normalized hat/button state

#include "tusb.h"
#include "class/hid/hid.h"

#define GAMEPAD_MAX_DEVICES 2

// Runtime state for each connected gamepad instance.
typedef struct {
    uint16_t id;
    uint32_t type;
    uint8_t index;
    uint8_t hat_state;
    uint32_t button_state;
} gamepad_t;

// TinyUSB helper table for keycode -> ASCII (unshifted/shifted).
uint8_t const keycode_to_ascii_table[128][2] = {HID_KEYCODE_TO_ASCII};

// Last keyboard report for edge detection.
static hid_keyboard_report_t prev_report = {0, 0, {0}};

static gamepad_t gamepads[GAMEPAD_MAX_DEVICES];
static int8_t gamepads_count = 0;

extern void kbd_raw_key_down(int code);
extern void kbd_raw_key_up(int code);
extern void gamepad_state_update(uint8_t index, uint8_t hat_state, uint32_t button_state);

// Returns true if keycode appears in a 6-key rollover report.
static inline bool find_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

// Compare two keyboard reports and emit edge transitions using the provided callback.
static void process_kbd_report(hid_keyboard_report_t const* r1, hid_keyboard_report_t const* r2,
                               void (*kbd_raw_key_cb)(int code)) {
    // Left GUI modifier
    if (r1->modifier & KEYBOARD_MODIFIER_LEFTGUI && !(r2->modifier & KEYBOARD_MODIFIER_LEFTGUI)) {
        kbd_raw_key_cb(HID_KEY_GUI_LEFT | 0x100);
    }

    // Right GUI modifier
    if (r1->modifier & KEYBOARD_MODIFIER_RIGHTGUI && !(r2->modifier & KEYBOARD_MODIFIER_RIGHTGUI)) {
        kbd_raw_key_cb(HID_KEY_GUI_RIGHT | 0x100);
    }

    // Process keycodes
    for (int i = 0; i < 6; i++) {
        if (r1->keycode[i]) {
            if (!find_key_in_report(r2, r1->keycode[i])) {
                bool is_shift = r1->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
                bool is_ctrl = r1->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);
                uint8_t keycode = r1->keycode[i];
                int code = keycode_to_ascii_table[keycode][is_shift];
                if (code == 0 && keycode != 0) {
                    code = keycode | 0x100;
                } else if (is_ctrl) {
                    code &= ~0x60;
                }
                kbd_raw_key_cb(code);
            }
        }
    }
}

// Emit key-down events for keys newly present in current report.
static void find_pressed_keys(hid_keyboard_report_t const* report) {
    process_kbd_report(report, &prev_report, &kbd_raw_key_down);
}

// Emit key-up events for keys no longer present in current report.
static void find_released_keys(hid_keyboard_report_t const* report) {
    process_kbd_report(&prev_report, report, &kbd_raw_key_up);
}

// Find existing gamepad slot by combined dev/instance identifier.
static gamepad_t* find_gamepad(uint16_t id) {
    for (int i = 0; i < gamepads_count; i++) {
        if (gamepads[i].id == id) {
            return &gamepads[i];
        }
    }
    return NULL;
}

// Decode hat state for VID/PID 081F:E401 report format.
static uint8_t get_hat_state_081FE401(uint8_t const* report) {
    uint8_t hat_state = 0;

    switch (report[0] << 8 | report[1]) {
        case 0x7F7F:
            hat_state = GAMEPAD_HAT_CENTERED;
            break;

        case 0x7F00:
            hat_state = GAMEPAD_HAT_UP;
            break;

        case 0xFF00:
            hat_state = GAMEPAD_HAT_UP_RIGHT;
            break;

        case 0xFF7F:
            hat_state = GAMEPAD_HAT_RIGHT;
            break;

        case 0xFFFF:
            hat_state = GAMEPAD_HAT_DOWN_RIGHT;
            break;

        case 0x7FFF:
            hat_state = GAMEPAD_HAT_DOWN;
            break;

        case 0x00FF:
            hat_state = GAMEPAD_HAT_DOWN_LEFT;
            break;

        case 0x007F:
            hat_state = GAMEPAD_HAT_LEFT;
            break;

        case 0x0000:
            hat_state = GAMEPAD_HAT_UP_LEFT;
            break;

        default:
            break;
    }

    return hat_state;
}

// Decode button bitfield for VID/PID 081F:E401 report format.
static uint32_t get_button_state_081FE401(uint8_t const* report) {
    uint32_t button_state = 0;

    if (report[5] & 0x20) {
        button_state |= GAMEPAD_BUTTON_A;
    }
    if (report[5] & 0x40) {
        button_state |= GAMEPAD_BUTTON_B;
    }
    if (report[5] & 0x10) {
        button_state |= GAMEPAD_BUTTON_X;
    }
    if (report[5] & 0x80) {
        button_state |= GAMEPAD_BUTTON_Y;
    }
    if (report[6] & 0x01) {
        button_state |= GAMEPAD_BUTTON_TL;
    }
    if (report[6] & 0x02) {
        button_state |= GAMEPAD_BUTTON_TR;
    }

    return button_state;
}

// Parse one gamepad report and publish normalized state.
static void process_gamepad_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint16_t id = dev_addr << 8 | instance;
    gamepad_t* gamepad = find_gamepad(id);

    switch (gamepad->type) {
        case 0x081FE401:
            if (len != 8) {
                return;
            }
            gamepad->hat_state = get_hat_state_081FE401(report);
            gamepad->button_state = get_button_state_081FE401(report);
            break;

        default:
            return;
    }

    gamepad_state_update(gamepad->index, gamepad->hat_state, gamepad->button_state);
}

// TinyUSB host callback: HID interface mounted.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_len;
    (void)desc_report;

    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            break;

        case HID_ITF_PROTOCOL_NONE:
            if (gamepads_count < GAMEPAD_MAX_DEVICES) {
                gamepad_t* gamepad = &gamepads[gamepads_count];
                gamepad->id = dev_addr << 8 | instance;
                gamepad->type = vid << 16 | pid;
                gamepad->index = gamepads_count;
                gamepad->hat_state = 0;
                gamepad->button_state = 0;
                gamepads_count++;

                printf("Gamepad connected: VID: %04X PID: %04X\n", vid, pid);
            }
            break;

        default:
            break;
    }

    tuh_hid_receive_report(dev_addr, instance);
}

// TinyUSB host callback: HID report received.
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)instance;
    (void)len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            find_pressed_keys((const hid_keyboard_report_t*)report);
            find_released_keys((const hid_keyboard_report_t*)report);
            memcpy(&prev_report, report, sizeof(hid_keyboard_report_t));
            break;

        case HID_ITF_PROTOCOL_NONE:
            process_gamepad_report(dev_addr, instance, report, len);
            break;

        default:
            break;
    }

    tuh_hid_receive_report(dev_addr, instance);
}

// TinyUSB host callback: HID interface unmounted.
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;
}
