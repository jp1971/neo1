// apple1.c
//
// Minimal Apple-1 bring-up for Neo6502 using apple1.h.
//
// Current behavior:
// - uses the real WDC65C02 via Reload's hardware glue
// - loads the Neo 1 top ROM image at $E000-$FFFF via apple1.h
// - captures a buffered startup trace during boot
// - sends Apple-1 output to UART
// - accepts UART keyboard input and injects it into apple1_key_down()
//
// Notes:
// - CHIPS_IMPL must appear in exactly one C/C++ translation unit
// - OLIMEX_NEO6502 should be defined for Neo6502 builds

#define CHIPS_IMPL

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include <pico/platform.h>
#include "pico/stdlib.h"

#include "chips/chips_common.h"
#ifdef OLIMEX_NEO6502
#include "chips/wdc65C02cpu.h"
#else
#include "chips/mos6502cpu.h"
#endif
#include "chips/mem.h"
#include "chips/clk.h"

#include "systems/neo1.h"
#include "neo1_terminal.h"
#include "neo1_video.h"
#include "neo1_usb.h"
#include "roms/neo1_roms.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"

#ifndef APPLE1_ENABLE_DVI
#define APPLE1_ENABLE_DVI (1)
#endif

#ifndef APPLE1_DVI_TEST_PATTERN
#define APPLE1_DVI_TEST_PATTERN (0)
#endif

// -----------------------------------------------------------------------------
// local state
// -----------------------------------------------------------------------------

typedef struct {
    apple1_t apple1;
    neo1_terminal_t term;
} state_t;

static state_t __not_in_flash() state;

static void neo1_video_sync_terminal(void) {
   neo1_video_set_terminal(&state.term);
}

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
// output callback from apple1.h
// -----------------------------------------------------------------------------

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
// descriptor + init
// -----------------------------------------------------------------------------

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

static void app_init(void) {
    neo1_terminal_clear(&state.term);

    apple1_desc_t desc = apple1_desc();
    apple1_init(&state.apple1, &desc);
    apple1_reset(&state.apple1);

    neo1_usb_init(neo1_usb_char_in, 0);

   neo1_video_sync_terminal();
}

// -----------------------------------------------------------------------------
// keyboard input from UART/stdio
// -----------------------------------------------------------------------------

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
// startup trace printing
// -----------------------------------------------------------------------------

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
// main
// -----------------------------------------------------------------------------

int main(void) {
    stdio_init_all();
    app_init();
#if APPLE1_ENABLE_DVI
    printf("[apple1] configuring DVI...\n");
   neo1_video_init(&state.term);

    // DVI init changes the system clock; reinitialize stdio/UART so the
    // serial console stays at the expected baud rate.
    stdio_init_all();

    printf("[apple1] starting DVI core...\n");
   neo1_video_start();
#endif

    
    sleep_ms(200);

    printf("[apple1] starting...\n");
#ifdef OLIMEX_NEO6502
    printf("[apple1] OLIMEX_NEO6502 defined\n");
#else
    printf("[apple1] OLIMEX_NEO6502 NOT defined\n");
#endif

    // Helpful sanity line so we know which ROM image is being used.
    printf("[apple1] ROM size = %u bytes\n", (unsigned)sizeof(apple1_rom));

    // The Neo 1 system ROM is expected to occupy the top 8 KB of memory
    // ($E000-$FFFF). The vectors live in the last 6 bytes of that image.
    if (sizeof(apple1_rom) >= 0x2000) {
        const uint32_t vec_base = (uint32_t)sizeof(apple1_rom) - 6u;
        printf("[apple1] vectors: NMI=%02X%02X RESET=%02X%02X IRQ=%02X%02X\n",
               apple1_rom[vec_base + 1], apple1_rom[vec_base + 0],
               apple1_rom[vec_base + 3], apple1_rom[vec_base + 2],
               apple1_rom[vec_base + 5], apple1_rom[vec_base + 4]);
    }

    printf("[apple1] capturing startup trace...\n");

    // Capture the buffered startup trace without printing inside the bus loop.
    while (state.apple1.startup_trace_len < APPLE1_TRACE_COUNT) {
        apple1_tick(&state.apple1);
    }

    print_startup_trace();

    printf("[apple1] entering run loop...\n");

    while (1) {
        uint32_t start_time_us = time_us_32();

        poll_keyboard();
        neo1_usb_task();

        // Run a modest chunk of cycles. We can tune this later.
        const uint32_t num_ticks = 5000;
        for (uint32_t i = 0; i < num_ticks; i++) {
            apple1_tick(&state.apple1);
        }

        uint32_t end_time_us = time_us_32();
        uint32_t execution_time = end_time_us - start_time_us;

        // Gentle pacing keeps UART output readable while debugging.
        int32_t sleep_time = 1000 - (int32_t)execution_time;
        if (sleep_time > 0) {
            sleep_us((uint32_t)sleep_time);
        }
    }

    __builtin_unreachable();
}