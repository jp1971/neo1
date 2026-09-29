#include "neo1_usb_host.h"

#include <stddef.h>
#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#ifndef ADAFRUIT_FRUIT_JAM
#error "Fruit Jam USB host requires PICO_BOARD=adafruit_fruit_jam"
#endif

enum {
    NEO1_FRUITJAM_TUH_RHPORT = 1,
    NEO1_FRUITJAM_HUB_POWER_SETTLE_MS = 100,
    // HSTX scanout claims the first two free DMA channels (0 and 1).
    NEO1_FRUITJAM_USB_TX_DMA_CHANNEL = 2,
};

_Static_assert(ADAFRUIT_FRUIT_JAM_USB_HOST_DATA_PLUS_PIN == 1,
               "unexpected Fruit Jam USB host D+ pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_USB_HOST_DATA_MINUS_PIN == 2,
               "unexpected Fruit Jam USB host D- pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_USB_HOST_5V_POWER_PIN == 11,
               "unexpected Fruit Jam USB host power pin");

static neo1_fruitjam_usb_event_handler_t g_event_handler;
static void* g_event_user_data;
static neo1_hid_keyboard_t g_keyboard_decoder;
static bool g_keyboard_mounted;
static uint8_t g_keyboard_dev_addr;
static uint8_t g_keyboard_instance;

bool neo1_fruitjam_usb_device_init(void) {
    const tusb_rhport_init_t config = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    return tusb_init(0, &config);
}

void neo1_fruitjam_usb_device_task(void) {
    tud_task();
}

static void neo1_fruitjam_usb_decoded_event(
    const neo1_hid_event_t* event,
    void* user_data)
{
    (void)user_data;
    if (g_event_handler) {
        g_event_handler(event, g_event_user_data);
    }
}

bool neo1_fruitjam_usb_host_init(
    neo1_fruitjam_usb_event_handler_t handler,
    void* user_data)
{
    g_event_handler = handler;
    g_event_user_data = user_data;
    g_keyboard_mounted = false;
    g_keyboard_dev_addr = 0;
    g_keyboard_instance = 0;
    neo1_hid_keyboard_reset(&g_keyboard_decoder);

    // GPIO 11 enables the switched 5 V rail feeding the onboard USB-A hub.
    gpio_init(ADAFRUIT_FRUIT_JAM_USB_HOST_5V_POWER_PIN);
    gpio_set_dir(ADAFRUIT_FRUIT_JAM_USB_HOST_5V_POWER_PIN, GPIO_OUT);
    gpio_put(ADAFRUIT_FRUIT_JAM_USB_HOST_5V_POWER_PIN, 1);
    sleep_ms(NEO1_FRUITJAM_HUB_POWER_SETTLE_MS);

    pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    config.pin_dp = ADAFRUIT_FRUIT_JAM_USB_HOST_DATA_PLUS_PIN;
    config.pinout = PIO_USB_PINOUT_DPDM;
    if (dma_channel_is_claimed(NEO1_FRUITJAM_USB_TX_DMA_CHANNEL)) {
        return false;
    }
    config.tx_ch = NEO1_FRUITJAM_USB_TX_DMA_CHANNEL;
    if (!tuh_configure(
            NEO1_FRUITJAM_TUH_RHPORT,
            TUH_CFGID_RPI_PIO_USB_CONFIGURATION,
            &config)) {
        return false;
    }
    return tuh_init(NEO1_FRUITJAM_TUH_RHPORT);
}

void neo1_fruitjam_usb_task(void) {
    // Service both roots from one target-owned event-loop location; TinyUSB
    // device and host tasks must not run concurrently.
    tud_task();
    tuh_task();
}

void tuh_hid_mount_cb(
    uint8_t dev_addr,
    uint8_t instance,
    const uint8_t* report_descriptor,
    uint16_t descriptor_length)
{
    (void)report_descriptor;
    (void)descriptor_length;

    if (tuh_hid_interface_protocol(dev_addr, instance) !=
        HID_ITF_PROTOCOL_KEYBOARD) {
        return;
    }

    g_keyboard_mounted = true;
    g_keyboard_dev_addr = dev_addr;
    g_keyboard_instance = instance;
    neo1_hid_keyboard_reset(&g_keyboard_decoder);
    printf("[usb] keyboard ready\n");
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("[usb] keyboard report request failed\n");
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (!g_keyboard_mounted || dev_addr != g_keyboard_dev_addr ||
        instance != g_keyboard_instance) {
        return;
    }

    g_keyboard_mounted = false;
    g_keyboard_dev_addr = 0;
    g_keyboard_instance = 0;
    neo1_hid_keyboard_reset(&g_keyboard_decoder);
    printf("[usb] keyboard removed\n");
}

void tuh_hid_report_received_cb(
    uint8_t dev_addr,
    uint8_t instance,
    const uint8_t* report,
    uint16_t length)
{
    if (tuh_hid_interface_protocol(dev_addr, instance) ==
            HID_ITF_PROTOCOL_KEYBOARD &&
        length >= sizeof(hid_keyboard_report_t)) {
        const hid_keyboard_report_t* keyboard_report =
            (const hid_keyboard_report_t*)report;
        neo1_hid_keyboard_process(
            &g_keyboard_decoder,
            keyboard_report->modifier,
            keyboard_report->keycode,
            neo1_fruitjam_usb_decoded_event,
            NULL);
    }

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("[usb] keyboard report request failed\n");
    }
}
