// neo1_usb.c
//
// TinyUSB host integration for Neo1.
//
// Input path:
// - TinyUSB HID callbacks deliver keyboard reports
// - a shared decoder edge-detects six-key reports against the previous report
// - Shift selects the US-layout mapping; Ctrl-letter emits $01-$1A
// - Enter, Backspace, Tab, and Space have explicit translations
// - F12 emits a Pico-owned video-aspect action rather than an Apple-1 key
// - decoded bytes are forwarded to the runner callback
//
// Storage path:
// - TinyUSB MSC mount/unmount events update mounted state
// - FatFs volume "0:" is mounted for the storage backends and diagnostics
//
// These host callbacks never assert the 6502 IRQ or NMI lines.

#include "neo1_usb.h"

#include <stdio.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "class/msc/msc.h"
#include "ff.h"
#include "input/neo1_hid_keyboard.h"

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS 0
#endif

static neo1_usb_char_handler_t g_char_handler = NULL;
static neo1_usb_action_handler_t g_action_handler = NULL;
static void* g_handler_user_data = NULL;

static bool g_keyboard_mounted = false;
static neo1_hid_keyboard_t g_keyboard_decoder;

static bool g_msc_mounted = false;
static FATFS fs;

// -----------------------------------------------------------------------------
// internal helpers
// -----------------------------------------------------------------------------

static void neo1_usb_hid_event(
    const neo1_hid_event_t* event,
    void* user_data)
{
    (void)user_data;
    if (event->kind == NEO1_HID_EVENT_CHARACTER) {
        if (g_char_handler) {
            g_char_handler(event->character, g_handler_user_data);
        }
    } else if (event->kind == NEO1_HID_EVENT_F12 && g_action_handler) {
        g_action_handler(
            NEO1_USB_ACTION_TOGGLE_VIDEO_ASPECT, g_handler_user_data);
    }
}

// -----------------------------------------------------------------------------
// public API
// -----------------------------------------------------------------------------

void neo1_usb_init(
    neo1_usb_char_handler_t char_handler,
    neo1_usb_action_handler_t action_handler,
    void* user_data) {
    // Reset module state before bringing up TinyUSB host stack.
    g_char_handler = char_handler;
    g_action_handler = action_handler;
    g_handler_user_data = user_data;
    g_keyboard_mounted = false;
    neo1_hid_keyboard_reset(&g_keyboard_decoder);

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
        printf("[usb] keyboard ready\n");
#if NEO1_DIAGNOSTICS
        printf("[usb] keyboard dev=%u inst=%u\n", dev_addr, instance);
#endif
        g_keyboard_mounted = true;
        neo1_hid_keyboard_reset(&g_keyboard_decoder);
        tuh_hid_receive_report(dev_addr, instance);
    }
}

// A HID device was unmounted.
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void) dev_addr;
    (void) instance;

    printf("[usb] keyboard removed\n");
    g_keyboard_mounted = false;
    neo1_hid_keyboard_reset(&g_keyboard_decoder);
}

// A report was received.
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void) len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        // Keyboard reports are parsed into ASCII/control bytes for Neo1 input.
        const hid_keyboard_report_t* keyboard_report =
            (const hid_keyboard_report_t*)report;
        neo1_hid_keyboard_process(
            &g_keyboard_decoder,
            keyboard_report->modifier,
            keyboard_report->keycode,
            neo1_usb_hid_event,
            NULL);
    }

    // Request the next report.
    tuh_hid_receive_report(dev_addr, instance);
}

// -----------------------------------------------------------------------------
// MSC callbacks
// -----------------------------------------------------------------------------

// Publish media presence and mount its FatFs volume synchronously.
void tuh_msc_mount_cb(uint8_t dev_addr) {
    (void)dev_addr;
    g_msc_mounted = true;

    FRESULT res = f_mount(&fs, "0:", 1);
    if (res != FR_OK) {
        printf("[msc] FatFs mount failed: %d\n", res);
    } else {
        printf("[msc] storage ready\n");
#if NEO1_DIAGNOSTICS
        printf("[msc] mounted dev=%u\n", dev_addr);
#endif
    }
}

// Withdraw media presence and detach the FatFs volume.
void tuh_msc_umount_cb(uint8_t dev_addr) {
    (void)dev_addr;
    printf("[msc] storage removed\n");
    g_msc_mounted = false;

    f_mount(NULL, "0:", 0);
}

// General device diagnostics are deliberately excluded from normal serial output.
void tuh_mount_cb(uint8_t dev_addr) {
#if NEO1_DIAGNOSTICS
    printf("[usb] device mounted dev=%u\n", dev_addr);
    
    // Get device descriptor (blocking)
    tusb_desc_device_t desc;
    if (tuh_descriptor_get_device_sync(dev_addr, &desc, sizeof(desc)) == sizeof(desc)) {
        printf("[usb] VID=%04X PID=%04X class=%02X\n", desc.idVendor, desc.idProduct, desc.bDeviceClass);
    }
#else
    (void)dev_addr;
#endif
}

// General device removal diagnostics follow the same policy.
void tuh_umount_cb(uint8_t dev_addr) {
#if NEO1_DIAGNOSTICS
    printf("[usb] device unmounted dev=%u\n", dev_addr);
#else
    (void)dev_addr;
#endif
}

// Debug helper to list root directory entries on mounted MSC media.
#if NEO1_DIAGNOSTICS
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
#endif
