// Fruit Jam Neo1 software-CPU runner.
//
// The RP2350 owns lifecycle, elapsed-time scheduling, HSTX DVI, and USB-CDC
// diagnostics. The shared machine owns the Apple-1 address space, terminal grid,
// and PIA; the software runner owns qe6502 execution. USB-C CDC remains a
// diagnostic/fallback console while the onboard USB-A hub supplies keyboard
// input. Storage, VACI, VCFFA1, audio, PicoDVI, and the Neo1-50 Pico entry
// stubs are deliberately absent.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "runners/neo1_soft_runner.h"
#include "terminal/neo1_terminal.h"

#include "neo1_hstx_video.h"
#include "neo1_usb_host.h"

#ifndef ADAFRUIT_FRUIT_JAM
#error "Neo1 Fruit Jam must be built for PICO_BOARD=adafruit_fruit_jam"
#endif

enum {
    NEO1_FRUITJAM_ENUMERATION_DELAY_MS = 1500,
    NEO1_FRUITJAM_MAX_CATCH_UP_US = 100000,
    NEO1_FRUITJAM_IDLE_SLEEP_US = 100,
    NEO1_FRUITJAM_TERMINAL_PUBLISH_US = 33333,
    NEO1_FRUITJAM_CTRL_L = 0x0C,
    NEO1_FRUITJAM_CTRL_R = 0x12,
    NEO1_FRUITJAM_USB_KEY_QUEUE_SIZE = 16,
};

// Keep the 64 KB machine out of the RP2350's small default C stack.
static neo1_machine_t machine;
static neo1_soft_runner_t cpu;
static neo1_terminal_t terminal;
static bool serial_previous_was_cr;
static bool terminal_dirty;
static uint64_t next_terminal_publish_us;
static uint8_t usb_key_queue[NEO1_FRUITJAM_USB_KEY_QUEUE_SIZE];
static uint8_t usb_key_queue_read;
static uint8_t usb_key_queue_write;
static bool usb_reset_requested;
static bool usb_clear_requested;

static bool neo1_fruitjam_usb_queue_empty(void) {
    return usb_key_queue_read == usb_key_queue_write;
}

static void neo1_fruitjam_usb_queue_key(uint8_t key) {
    const uint8_t next =
        (uint8_t)((usb_key_queue_write + 1) % NEO1_FRUITJAM_USB_KEY_QUEUE_SIZE);
    if (next == usb_key_queue_read) {
        return;
    }
    usb_key_queue[usb_key_queue_write] = key;
    usb_key_queue_write = next;
}

static uint8_t neo1_fruitjam_usb_queue_pop(void) {
    const uint8_t key = usb_key_queue[usb_key_queue_read];
    usb_key_queue_read =
        (uint8_t)((usb_key_queue_read + 1) % NEO1_FRUITJAM_USB_KEY_QUEUE_SIZE);
    return key;
}

static void neo1_fruitjam_usb_event(
    const neo1_hid_event_t* event,
    void* user_data)
{
    (void)user_data;
    if (event->kind == NEO1_HID_EVENT_F12) {
        const bool corrected =
            neo1_hstx_video_toggle_widescreen_correction();
        printf("[neo1-fruitjam] video=%s\n",
               corrected ? "16:9 stretch correction" : "native 4:3");
        return;
    }
    if (event->kind != NEO1_HID_EVENT_CHARACTER) {
        return;
    }

    uint8_t key = event->character;
    if (key == NEO1_FRUITJAM_CTRL_R) {
        usb_reset_requested = true;
    } else if (key == NEO1_FRUITJAM_CTRL_L) {
        usb_clear_requested = true;
    } else {
        if (key >= 'a' && key <= 'z') {
            key = (uint8_t)(key - ('a' - 'A'));
        }
        neo1_fruitjam_usb_queue_key(key);
    }
}

// Convert console input to the uppercase ASCII produced by an Apple-1
// keyboard. Treat CRLF as one Return while still accepting a bare LF.
static bool neo1_fruitjam_serial_key(int input, uint8_t* key) {
    if (input == '\n' && serial_previous_was_cr) {
        serial_previous_was_cr = false;
        return false;
    }

    serial_previous_was_cr = input == '\r';
    if (input == '\n') {
        input = '\r';
    } else if (input >= 'a' && input <= 'z') {
        input -= 'a' - 'A';
    }

    if (input == '\r' || input == '\b' ||
        (input >= 0x20 && input <= 0x7E)) {
        *key = (uint8_t)input;
        return true;
    }
    return false;
}

static void neo1_fruitjam_char_out(uint8_t ch, void* user_data) {
    (void)user_data;
    const uint8_t ascii = ch & 0x7Fu;
    neo1_terminal_apple1_putc(&terminal, ascii);
    // The CPU may produce output much faster than a display frame. Publish a
    // coalesced snapshot from the main loop instead of expanding one complete
    // text raster for every byte in a monitor dump.
    terminal_dirty = true;
    putchar((int)ascii);
    if (ascii == '\r') {
        // Apple-1 software emits CR only; USB-CDC terminals need LF to move
        // down instead of returning the cursor to column zero on the same row.
        putchar('\n');
    }
}

static void neo1_fruitjam_print_entry(const char* reason) {
    const uint16_t vector =
        (uint16_t)machine.ram[0xFFFC] |
        ((uint16_t)machine.ram[0xFFFD] << 8);

    printf("\n[neo1-fruitjam] %s personality=%u reset=$%04X entry=$%04X\n",
           reason,
           (unsigned)machine.profile->personality,
           (unsigned)vector,
           (unsigned)cpu.tick.address);
    printf("[neo1-fruitjam] HSTX DVI + USB keyboard + serial fallback enabled; Ctrl-L clears; Ctrl-R resets; storage disabled\n");
}

static void neo1_fruitjam_reset(void) {
    neo1_terminal_clear(&terminal);
    neo1_hstx_video_set_terminal(&terminal);
    terminal_dirty = false;
    next_terminal_publish_us =
        time_us_64() + NEO1_FRUITJAM_TERMINAL_PUBLISH_US;
    neo1_machine_reset(&machine);
    neo1_soft_runner_reset(&cpu);
    serial_previous_was_cr = false;
    usb_key_queue_read = 0;
    usb_key_queue_write = 0;
    usb_reset_requested = false;
    usb_clear_requested = false;
    neo1_fruitjam_print_entry("reset");
}

int main(void) {
    // Both TinyUSB roots are enabled at compile time, so the SDK's no-argument
    // initializer would also start PIO host root 1 before its Fruit Jam pins
    // and DMA channel are configured. Initialize only native device root 0.
    if (!neo1_fruitjam_usb_device_init()) {
        return 1;
    }
    stdio_init_all();
    for (uint32_t elapsed_ms = 0;
         elapsed_ms < NEO1_FRUITJAM_ENUMERATION_DELAY_MS;
         ++elapsed_ms) {
        neo1_fruitjam_usb_device_task();
        sleep_ms(1);
    }

    const neo1_profile_t* profile = neo1_profile_find(NEO1_PERSONALITY);
    if (!profile) {
        printf("[neo1-fruitjam] unsupported personality=%u\n",
               (unsigned)NEO1_PERSONALITY);
        return 1;
    }

    const neo1_machine_desc_t desc = {
        .profile = profile,
        .char_out = neo1_fruitjam_char_out,
    };
    if (!neo1_machine_init(&machine, &desc)) {
        printf("[neo1-fruitjam] machine initialization failed\n");
        return 1;
    }
    if (!neo1_soft_runner_init(&cpu, &machine)) {
        printf("[neo1-fruitjam] CPU initialization failed\n");
        return 1;
    }

    neo1_terminal_clear(&terminal);
    if (!neo1_hstx_video_init(&terminal)) {
        printf("[neo1-fruitjam] HSTX DVI initialization failed\n");
        return 1;
    }
    if (!neo1_fruitjam_usb_host_init(neo1_fruitjam_usb_event, NULL)) {
        printf("[neo1-fruitjam] PIO USB host initialization failed\n");
        return 1;
    }
    terminal_dirty = false;
    next_terminal_publish_us =
        time_us_64() + NEO1_FRUITJAM_TERMINAL_PUBLISH_US;

    neo1_fruitjam_print_entry("ready");
    uint64_t previous_time_us = time_us_64();
    int pending_input = PICO_ERROR_TIMEOUT;

    while (true) {
        neo1_fruitjam_usb_task();

        const uint64_t current_time_us = time_us_64();
        uint64_t elapsed_us = current_time_us - previous_time_us;
        previous_time_us = current_time_us;
        if (elapsed_us > NEO1_FRUITJAM_MAX_CATCH_UP_US) {
            elapsed_us = NEO1_FRUITJAM_MAX_CATCH_UP_US;
        }
        if (elapsed_us > 0) {
            (void)neo1_soft_runner_exec_us(&cpu, (uint32_t)elapsed_us);
        }

        const uint64_t publish_time_us = time_us_64();
        if (terminal_dirty && publish_time_us >= next_terminal_publish_us) {
            neo1_hstx_video_set_terminal(&terminal);
            terminal_dirty = false;
            next_terminal_publish_us =
                publish_time_us + NEO1_FRUITJAM_TERMINAL_PUBLISH_US;
        }

        // Retain one console byte until WozMon consumes the PIA latch. Leaving
        // subsequent bytes in USB-CDC avoids dropping pasted commands at the
        // one-byte Apple-1 input boundary.
        if (pending_input == PICO_ERROR_TIMEOUT) {
            pending_input = getchar_timeout_us(0);
        }
        if (pending_input == NEO1_FRUITJAM_CTRL_R || usb_reset_requested) {
            neo1_fruitjam_reset();
            pending_input = PICO_ERROR_TIMEOUT;
            previous_time_us = time_us_64();
            continue;
        }
        if (pending_input == NEO1_FRUITJAM_CTRL_L || usb_clear_requested) {
            // Clear only the target-owned display. Ctrl-L is lifecycle UI and
            // must not enter the one-byte Apple-1 keyboard latch.
            neo1_terminal_clear(&terminal);
            neo1_hstx_video_set_terminal(&terminal);
            terminal_dirty = false;
            next_terminal_publish_us =
                time_us_64() + NEO1_FRUITJAM_TERMINAL_PUBLISH_US;
            pending_input = PICO_ERROR_TIMEOUT;
            usb_clear_requested = false;
            continue;
        }
        if (pending_input != PICO_ERROR_TIMEOUT &&
            machine.pia.keyboard_latch == 0) {
            uint8_t key = 0;
            if (neo1_fruitjam_serial_key(pending_input, &key)) {
                neo1_machine_key_down(&machine, key);
            }
            pending_input = PICO_ERROR_TIMEOUT;
        }

        if (machine.pia.keyboard_latch == 0 &&
            !neo1_fruitjam_usb_queue_empty()) {
            neo1_machine_key_down(
                &machine, neo1_fruitjam_usb_queue_pop());
        }

        sleep_us(NEO1_FRUITJAM_IDLE_SLEEP_US);
    }
}
