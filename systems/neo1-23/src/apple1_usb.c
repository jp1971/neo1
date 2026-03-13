#include "apple1_usb.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"

// Keep the HID keycode -> ASCII lookup local to Neo 1 instead of depending on
// TinyUSB example helper headers.
static const uint8_t keycode2ascii[128][2] = { HID_KEYCODE_TO_ASCII };

static apple1_usb_char_handler_t g_char_handler = NULL;
static void* g_char_handler_user_data = NULL;

static bool g_keyboard_mounted = false;
static hid_keyboard_report_t g_prev_report = { 0 };

// -----------------------------------------------------------------------------
// internal helpers
// -----------------------------------------------------------------------------

static inline bool apple1_usb_is_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode) {
    for (uint32_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

static void apple1_usb_emit_char(uint8_t ch) {
    if (g_char_handler) {
        g_char_handler(ch, g_char_handler_user_data);
    }
}

static void apple1_usb_process_kbd_report(hid_keyboard_report_t const* report) {
    // Modifier bits from TinyUSB HID definitions.
    const bool shift =
        (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
    const bool ctrl =
        (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) != 0;

    for (uint32_t i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];
        if (keycode == 0) {
            continue;
        }

        // Only emit newly pressed keys.
        if (apple1_usb_is_key_in_report(&g_prev_report, keycode)) {
            continue;
        }

        switch (keycode) {
            case HID_KEY_ENTER:
                apple1_usb_emit_char('\r');
                break;

            case HID_KEY_BACKSPACE:
                apple1_usb_emit_char(0x08);
                break;

            case HID_KEY_TAB:
                apple1_usb_emit_char('\t');
                break;

            case HID_KEY_SPACE:
                apple1_usb_emit_char(' ');
                break;

            default: {
                if (keycode < 128) {
                    uint8_t ch = keycode2ascii[keycode][shift ? 1 : 0];
                    if (ch) {
                        if (ctrl) {
                            uint8_t upper = (uint8_t)toupper((int)ch);
                            if ((upper >= 'A') && (upper <= 'Z')) {
                                ch = (uint8_t)(upper - '@');
                            }
                        }
                        apple1_usb_emit_char(ch);
                    }
                }
                break;
            }
        }
    }

    g_prev_report = *report;
}

// -----------------------------------------------------------------------------
// public API
// -----------------------------------------------------------------------------

void apple1_usb_init(apple1_usb_char_handler_t handler, void* user_data) {
    g_char_handler = handler;
    g_char_handler_user_data = user_data;
    g_keyboard_mounted = false;
    memset(&g_prev_report, 0, sizeof(g_prev_report));

    board_init();
    tusb_init();
}

void apple1_usb_task(void) {
    tuh_task();
}

bool apple1_usb_keyboard_mounted(void) {
    return g_keyboard_mounted;
}

// -----------------------------------------------------------------------------
// TinyUSB host callbacks
// -----------------------------------------------------------------------------

// A HID device was mounted.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void) desc_report;
    (void) desc_len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        printf("[usb] keyboard mounted dev=%u inst=%u\n", dev_addr, instance);
        g_keyboard_mounted = true;
        memset(&g_prev_report, 0, sizeof(g_prev_report));
        tuh_hid_receive_report(dev_addr, instance);
    }
}

// A HID device was unmounted.
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void) dev_addr;
    (void) instance;

    printf("[usb] keyboard unmounted dev=%u inst=%u\n", dev_addr, instance);
    g_keyboard_mounted = false;
    memset(&g_prev_report, 0, sizeof(g_prev_report));
}

// A report was received.
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void) len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        // printf("[usb] keyboard report dev=%u inst=%u\n", dev_addr, instance);
        apple1_usb_process_kbd_report((hid_keyboard_report_t const*) report);
    }

    // Request the next report.
    tuh_hid_receive_report(dev_addr, instance);
}