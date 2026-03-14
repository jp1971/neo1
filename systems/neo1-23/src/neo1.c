// apple1.c
//
// Neo1-23 Apple-1-compatible machine entry point.
//
// This file is now less of a bring-up experiment and more of a thin machine
// orchestrator. The RP2040-side platform services live in separate Neo1 modules:
//
// - neo1_terminal.*  : text buffer, cursor state, clear-screen behavior
// - neo1_video.*     : PicoDVI text rendering and scanline generation
// - neo1_usb.*       : TinyUSB host keyboard input
// - neo1.h           : 65C02 runtime, memory map, ROM loading, and I/O model
//
// The job of this file is to:
// - construct the 65C02 machine description
// - connect machine output to the Neo1 terminal/video path
// - connect UART and USB keyboard input to the machine input path
// - initialize the system ROM environment
// - run the machine tick loop
//
// Architecturally, this is still Apple-1-compatible at the machine interface,
// but it is now running on a distinct Neo1-23 platform with:
// - an 8 KiB system ROM at $E000-$FFFF
// - DVI text output
// - USB keyboard input
// - a modern RP2040-side terminal pipeline
//
// Notes:
// - CHIPS_IMPL must appear in exactly one C/C++ translation unit
// - This build targets the Olimex Neo6502 platform exclusively.

#define CHIPS_IMPL

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include <pico/platform.h>
#include "pico/stdlib.h"

#include "chips/chips_common.h"
#include "chips/wdc65C02cpu.h"
#include "chips/mem.h"
#include "chips/clk.h"

#include "systems/neo1.h"
#include "neo1_terminal.h"
#include "neo1_video.h"
#include "neo1_usb.h"
#include "roms/neo1_roms.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"

#ifndef APPLE1_DVI_TEST_PATTERN
#define APPLE1_DVI_TEST_PATTERN (0)
#endif

// -----------------------------------------------------------------------------
// local machine/platform state
// -----------------------------------------------------------------------------

typedef struct {
    apple1_t apple1;
    neo1_terminal_t term;
} state_t;

static state_t __not_in_flash() state;

//
// Push the current terminal state into the video module.
//
// The terminal owns character/cursor state.
// The video module owns how that state is rendered to DVI.
//
static void neo1_video_sync_terminal(void) {
   neo1_video_set_terminal(&state.term);
}

//
// USB keyboard input callback.
//
// TinyUSB decodes HID reports in neo1_usb.c and forwards ASCII-like characters
// here. This function normalizes them into the conventions expected by the
// Apple-1-style machine interface before injecting them into apple1_key_down().
//
// Current normalization policy:
// - LF becomes CR
// - printable ASCII is uppercased
// - control characters are passed through
//
static void neo1_usb_char_in(uint8_t ch, void* user_data) {
    (void)user_data;

#if APPLE1_KBD_DEBUG
    printf("[usb] ascii=%02X", (unsigned)ch);
    if ((ch >= 32) && (ch < 127)) {
        printf(" '%c'", ch);
    } else if (ch == '\r') {
        printf(" <CR>");
    }
    printf("\n");
#endif

    if (ch == '\n') {
        ch = '\r';
    } else if (isascii(ch)) {
        ch = (uint8_t)toupper(ch);
    }

    apple1_key_down(&state.apple1, ch);
}

// -----------------------------------------------------------------------------
// machine output callback
// -----------------------------------------------------------------------------

//
// Character output from the 65C02-side machine.
//
// The machine writes characters through the Neo1 runtime callback interface.
// We fan that output out in two directions:
// - into the Neo1 terminal/video pipeline for on-screen display
// - into UART/stdout for debugging and serial visibility
//
// Apple-1 software commonly sets bit 7 on output characters, so we strip it
// before rendering or printing the byte.
//
static void apple1_char_out(uint8_t ch, void* user_data) {
    (void)user_data;

    // Apple-1 monitor output often has bit 7 set. Strip it for terminal display.
    ch &= 0x7F;
    neo1_terminal_putc(&state.term, ch);
    neo1_video_sync_terminal();

    // Make carriage return readable on a modern terminal.
    if (ch == '\r') {
        putchar('\r');
        putchar('\n');
    } else {
        putchar((int)ch);
    }
}

// -----------------------------------------------------------------------------
// machine description and initialization
// -----------------------------------------------------------------------------

//
// Build the machine description consumed by the Neo1 runtime.
//
// This is where the current machine personality is defined:
// - which ROM image is presented at the top of memory
// - which output callback receives machine-generated characters
// - whether optional debug hooks are enabled
//
static apple1_desc_t apple1_desc(void) {
    return (apple1_desc_t){
        .debug = {0},
        .roms = {
            .rom = {
                .ptr = apple1_rom,
                .size = sizeof(apple1_rom),
            },
        },
        .char_out = {
            .func = apple1_char_out,
            .user_data = 0,
        },
    };
}

//
// Initialize the machine and the Neo1-side support modules.
//
// Order matters here:
// 1. clear the terminal state
// 2. initialize the machine/runtime
// 3. reset the 65C02-side machine
// 4. initialize USB keyboard input
// 5. publish the terminal state to the video module
//
static void app_init(void) {
    neo1_terminal_clear(&state.term);

    apple1_desc_t desc = apple1_desc();
    apple1_init(&state.apple1, &desc);
    apple1_reset(&state.apple1);

    neo1_usb_init(neo1_usb_char_in, 0);

   neo1_video_sync_terminal();
}

// -----------------------------------------------------------------------------
// UART keyboard input path
// -----------------------------------------------------------------------------

//
// Poll the UART/stdin keyboard path.
//
// Neo1 currently supports two input sources in parallel:
// - UART/stdin polling here
// - USB keyboard input through neo1_usb_task()
//
// Both paths normalize characters into the same Apple-1-style machine input
// convention and then inject them via apple1_key_down().
//

#ifndef APPLE1_KBD_DEBUG
#define APPLE1_KBD_DEBUG (0)
#endif

#ifndef NEO1_TERM_DEBUG
#define NEO1_TERM_DEBUG (1)
#endif

static void poll_keyboard(void) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) {
        return;
    }

#if NEO1_TERM_DEBUG
    // Ctrl-D dumps the software terminal buffer for debugging.
    if (ch == 0x04) {
        neo1_terminal_dump(&state.term);
        return;
    }
#endif


    // Normalize terminal input slightly for WozMon.
    if (ch == '\n') {
        ch = '\r';
    } else if (isascii(ch)) {
        ch = toupper(ch);
    }

    apple1_key_down(&state.apple1, (uint8_t)ch);
}

// -----------------------------------------------------------------------------
// startup trace support
// -----------------------------------------------------------------------------

//
// Dump the buffered startup bus trace captured by the Neo1 runtime.
//
// This is useful during low-level bring-up because it shows the actual memory
// traffic seen during reset and early execution without printing directly from
// inside the memory/bus path.
//
static void print_startup_trace(void) {
    const apple1_trace_event_t* ev = 0;
    uint32_t count = apple1_read_startup_trace(&state.apple1, &ev);

    printf("[apple1] startup trace (%u events)\n", (unsigned)count);

    for (uint32_t i = 0; i < count; i++) {
        printf("%c %04X %02X\n",
               ev[i].rw ? 'R' : 'W',
               ev[i].addr,
               ev[i].data);
    }
}

// -----------------------------------------------------------------------------
// program entry point
// -----------------------------------------------------------------------------

//
// Main boot flow:
// 1. initialize stdio/UART
// 2. initialize the machine and platform services
// 3. bring up DVI if enabled
// 4. print ROM/vector sanity information
// 5. capture and print the startup trace
// 6. enter the steady-state run loop
//
// The steady-state loop interleaves:
// - UART polling
// - USB host polling
// - 65C02 execution
// - light pacing for readability while debugging
//
int main(void) {
    stdio_init_all();
    app_init();
    printf("[apple1] configuring DVI...\n");
   neo1_video_init(&state.term);

    // DVI init changes the system clock; reinitialize stdio/UART so the
    // serial console stays at the expected baud rate.
    stdio_init_all();

    printf("[apple1] starting DVI core...\n");
   neo1_video_start();
    
    sleep_ms(200);

    printf("[neo1] starting...\n");

    // Print ROM size as a quick confirmation that the expected Neo1-23 system
    // image is compiled into the build.
    printf("[neo1] ROM size = %u bytes\n", (unsigned)sizeof(apple1_rom));

    // The Neo1-23 system ROM occupies the top 8 KiB of memory ($E000-$FFFF).
    // The 65C02 vectors live in the last 6 bytes of that image, so printing
    // them here is a quick sanity check that the ROM layout is what we expect.
    if (sizeof(apple1_rom) >= 0x2000) {
        const uint32_t vec_base = (uint32_t)sizeof(apple1_rom) - 6u;
        printf("[apple1] vectors: NMI=%02X%02X RESET=%02X%02X IRQ=%02X%02X\n",
               apple1_rom[vec_base + 1], apple1_rom[vec_base + 0],
               apple1_rom[vec_base + 3], apple1_rom[vec_base + 2],
               apple1_rom[vec_base + 5], apple1_rom[vec_base + 4]);
    }

    printf("[apple1] capturing startup trace...\n");

    // Capture enough early bus activity to understand reset/startup behavior,
    // but keep printing outside the bus loop so timing stays predictable.
    while (state.apple1.startup_trace_len < APPLE1_TRACE_COUNT) {
        apple1_tick(&state.apple1);
    }

    print_startup_trace();

    printf("[apple1] entering run loop...\n");

    while (1) {
        uint32_t start_time_us = time_us_32();

        poll_keyboard();
        neo1_usb_task();

        // Run a modest chunk of machine cycles per host iteration.
        // This is intentionally simple and can be revisited later if Neo1-23
        // needs different pacing or tighter synchronization.
        const uint32_t num_ticks = 5000;
        for (uint32_t i = 0; i < num_ticks; i++) {
            apple1_tick(&state.apple1);
        }

        uint32_t end_time_us = time_us_32();
        uint32_t execution_time = end_time_us - start_time_us;

        // Light pacing keeps UART/debug output readable while preserving a
        // simple main-loop structure during this stage of the project.
        int32_t sleep_time = 1000 - (int32_t)execution_time;
        if (sleep_time > 0) {
            sleep_us((uint32_t)sleep_time);
        }
    }

    __builtin_unreachable();
}