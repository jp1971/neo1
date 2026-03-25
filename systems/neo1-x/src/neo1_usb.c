#include "neo1_usb.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "class/msc/msc.h"
#include "ff.h"

// Keep the HID keycode -> ASCII lookup local to Neo 1 instead of depending on
// TinyUSB example helper headers.
static const uint8_t keycode2ascii[128][2] = { HID_KEYCODE_TO_ASCII };

static neo1_usb_char_handler_t g_char_handler = NULL;
static void* g_char_handler_user_data = NULL;

static bool g_keyboard_mounted = false;
static hid_keyboard_report_t g_prev_report = { 0 };

static bool g_msc_mounted = false;
static FATFS fs;

// -----------------------------------------------------------------------------
// internal helpers
// -----------------------------------------------------------------------------

static inline bool neo1_usb_is_key_in_report(hid_keyboard_report_t const* report, uint8_t keycode) {
    for (uint32_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) {
            return true;
        }
    }
    return false;
}

static void neo1_usb_emit_char(uint8_t ch) {
    if (g_char_handler) {
        g_char_handler(ch, g_char_handler_user_data);
    }
}

static void neo1_usb_process_kbd_report(hid_keyboard_report_t const* report) {
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
        if (neo1_usb_is_key_in_report(&g_prev_report, keycode)) {
            continue;
        }

        switch (keycode) {
            case HID_KEY_ENTER:
                neo1_usb_emit_char('\r');
                break;

            case HID_KEY_BACKSPACE:
                neo1_usb_emit_char(0x08);
                break;

            case HID_KEY_TAB:
                neo1_usb_emit_char('\t');
                break;

            case HID_KEY_SPACE:
                neo1_usb_emit_char(' ');
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
                        neo1_usb_emit_char(ch);
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

void neo1_usb_init(neo1_usb_char_handler_t handler, void* user_data) {
    g_char_handler = handler;
    g_char_handler_user_data = user_data;
    g_keyboard_mounted = false;
    memset(&g_prev_report, 0, sizeof(g_prev_report));

    board_init();
    tusb_init();
}

void neo1_usb_task(void) {
    tuh_task();
}

bool neo1_usb_keyboard_mounted(void) {
    return g_keyboard_mounted;
}

bool neo1_usb_msc_mounted(void) {
    return g_msc_mounted;
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
        neo1_usb_process_kbd_report((hid_keyboard_report_t const*) report);
    }

    // Request the next report.
    tuh_hid_receive_report(dev_addr, instance);
}

// -----------------------------------------------------------------------------
// MSC callbacks
// -----------------------------------------------------------------------------

// MSC device is mounted
void tuh_msc_mount_cb(uint8_t dev_addr) {
    printf("[msc] mounted dev=%u\n", dev_addr);
    g_msc_mounted = true;

    // Mount FatFs
    FRESULT res = f_mount(&fs, "0:", 1);
    if (res != FR_OK) {
        printf("[msc] FatFs mount failed: %d\n", res);
    } else {
        printf("[msc] FatFs mounted\n");
    }
}

// MSC device is unmounted
void tuh_msc_umount_cb(uint8_t dev_addr) {
    printf("[msc] unmounted dev=%u\n", dev_addr);
    g_msc_mounted = false;

    // Unmount FatFs
    f_mount(NULL, "0:", 0);
}

// General device mount callback for debugging
void tuh_mount_cb(uint8_t dev_addr) {
    printf("[usb] device mounted dev=%u\n", dev_addr);
    
    // Get device descriptor (blocking)
    tusb_desc_device_t desc;
    if (tuh_descriptor_get_device_sync(dev_addr, &desc, sizeof(desc)) == sizeof(desc)) {
        printf("[usb] VID=%04X PID=%04X class=%02X\n", desc.idVendor, desc.idProduct, desc.bDeviceClass);
    }
}

// General device unmount callback for debugging
void tuh_umount_cb(uint8_t dev_addr) {
    printf("[usb] device unmounted dev=%u\n", dev_addr);
}

// Test function to list files
void neo1_msc_list_files(void) {
    if (!g_msc_mounted) {
        printf("[msc] no drive mounted\n");
        return;
    }

    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, "0:");
    if (res != FR_OK) {
        printf("[msc] opendir failed: %d\n", res);
        return;
    }

    printf("[msc] files:\n");
    while (true) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;
        printf("  %s\n", fno.fname);
    }
    f_closedir(&dir);
}