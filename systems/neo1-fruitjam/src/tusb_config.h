#ifndef NEO1_FRUITJAM_TUSB_CONFIG_H
#define NEO1_FRUITJAM_TUSB_CONFIG_H

// Fruit Jam dual USB topology:
// - root port 0: RP2350 native controller in device mode for USB-C CDC stdio;
// - root port 1: Pico-PIO-USB host on the onboard USB-A hub.

#define CFG_TUSB_OS                  OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE        OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE        OPT_MODE_HOST
#define CFG_TUH_RPI_PIO_USB          1

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN           __attribute__((aligned(4)))
#endif

// Native USB-C CDC device used by pico_stdio_usb.
#define CFG_TUD_CDC                  1
#define CFG_TUD_VENDOR               0
#define CFG_TUD_CDC_RX_BUFSIZE       64
#define CFG_TUD_CDC_TX_BUFSIZE       64
#define CFG_TUD_CDC_EP_BUFSIZE       64

// PIO host classes. Storage remains disabled until checkpoint 5.
#define CFG_TUH_ENUMERATION_BUFSIZE  256
#define CFG_TUH_HUB                  1
#define CFG_TUH_DEVICE_MAX           4
#define CFG_TUH_CDC                  0
#define CFG_TUH_HID                  4
#define CFG_TUH_MIDI                 0
#define CFG_TUH_MSC                  0
#define CFG_TUH_VENDOR               0
#define CFG_TUH_HID_EPIN_BUFSIZE     64
#define CFG_TUH_HID_EPOUT_BUFSIZE    64

#endif
