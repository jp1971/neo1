#pragma once

// Fruit Jam-owned PIO USB-host transport. The shared decoder translates HID
// reports; the caller owns lifecycle controls and Apple-1 machine injection.

#include <stdbool.h>
#include <stdint.h>

#include "input/neo1_hid_keyboard.h"

typedef void (*neo1_fruitjam_usb_event_handler_t)(
    const neo1_hid_event_t* event,
    void* user_data);

// Initialize native USB-C device root port 0 before stdio_init_all().
bool neo1_fruitjam_usb_device_init(void);

// Service native USB-C while startup waits for host enumeration.
void neo1_fruitjam_usb_device_task(void);

// Enable USB-A hub power and start TinyUSB host root port 1.
bool neo1_fruitjam_usb_host_init(
    neo1_fruitjam_usb_event_handler_t handler,
    void* user_data);

// Pump native-device CDC plus host enumeration/report callbacks from the main
// loop. Keeping both TinyUSB tasks here prevents concurrent stack dispatch.
void neo1_fruitjam_usb_task(void);
