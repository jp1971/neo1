// neo1.c
//
// Neo1-23 machine entry point.
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
// Architecturally, this remains compatible with Apple-1 / Replica 1 conventions
// at the machine interface, but it is now running on a distinct Neo1-23 platform
// with:
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

#ifndef NEO1_PERSONALITY
#define NEO1_PERSONALITY (23)
#endif

#define NEO1_PERSONALITY_23 (23)
#define NEO1_PERSONALITY_50 (50)

#if NEO1_PERSONALITY == NEO1_PERSONALITY_50
#define NEO1_ROM_BASE (0xFF00)
#define NEO1_ROM_PROTECT_BASE (0xFF00)
#endif

#include "systems/neo1.h"
#include "neo1_terminal.h"
#include "neo1_video.h"
#include "neo1_msc.h"
#if NEO1_ENABLE_VCFFA1
#include "neo1_cffa1.h"
#include "ram/neo1_cffa1_m2_blockdrv.h"
#endif
#include "neo1_usb.h"
#include "roms/neo1_roms.h"

#ifndef NEO1_ENABLE_VACI
#define NEO1_ENABLE_VACI (1)
#endif

#if NEO1_ENABLE_VACI
#include "ram/neo1_vaci_v1.h"
#endif

#include "hardware/vreg.h"
#include "hardware/clocks.h"

#ifndef NEO1_DVI_TEST_PATTERN
#define NEO1_DVI_TEST_PATTERN (0)
#endif

// -----------------------------------------------------------------------------
// local machine/platform state
// -----------------------------------------------------------------------------

typedef struct {
    neo1_t neo1;
    neo1_terminal_t term;
} state_t;

static state_t __not_in_flash() state;
static bool msc_listed = false;

static void neo1_install_ram_tools(neo1_t* sys) {
#if NEO1_ENABLE_VCFFA1
    const uint32_t m2_size = (uint32_t)sizeof(neo1_cffa1_m2_blockdrv);
    const uint32_t m2_addr = NEO1_CFFA1_M2_BLOCKDRV_ADDR;
    CHIPS_ASSERT((m2_addr + m2_size) <= NEO1_ROM_BASE);
    memcpy(&sys->ram[m2_addr], neo1_cffa1_m2_blockdrv, m2_size);
    printf("[neo1] cffa1 m2 blockdrv installed at $%04X (%lu bytes), run with G %04X\n",
           (unsigned)m2_addr,
           (unsigned long)m2_size,
            (unsigned)NEO1_CFFA1_M2_TESTMAIN_ADDR);
#else
    printf("[neo1] vcffa1 disabled; $AFF0-$AFFF and $AFDC-$AFDD remain free\n");
    (void)sys;
#endif

#if NEO1_ENABLE_VACI
    const uint32_t vaci_size = (uint32_t)sizeof(neo1_vaci_v1);
    const uint32_t vaci_addr = NEO1_VACI_V1_ADDR;
    CHIPS_ASSERT((vaci_addr + vaci_size) <= NEO1_ROM_BASE);
    memcpy(&sys->ram[vaci_addr], neo1_vaci_v1, vaci_size);
    printf("[neo1] vaci v1 installed at $%04X (%lu bytes), run with C %04XR\n",
           (unsigned)vaci_addr,
           (unsigned long)vaci_size,
           (unsigned)vaci_addr);
#else
    printf("[neo1] vaci v1 disabled; $C100 remains free for monitor or hardware ACI use\n");
#endif
}

//
// Push the current terminal state into the video module.
//
// The terminal owns character/cursor state.
// The video module owns how that state is rendered to DVI.
//
static void neo1_video_sync_terminal(void) {
   neo1_video_set_terminal(&state.term);
}

#if NEO1_PERSONALITY == NEO1_PERSONALITY_50
static void neo1_install_neo150_entry_stubs(neo1_t* sys) {
    // In Neo1-50, E000/F000 are writable load targets. Until user code is
    // loaded there, jumping to them would run uninitialized bytes and hang.
    // Install minimal JMP $FF00 stubs so E000R/F000R return to WozMon.
    static const uint8_t jmp_wozmon[] = { 0x4C, 0x00, 0xFF };
    memcpy(&sys->ram[0xE000], jmp_wozmon, sizeof(jmp_wozmon));
    memcpy(&sys->ram[0xF000], jmp_wozmon, sizeof(jmp_wozmon));
    printf("[neo1] neo1-50 entry stubs installed: E000/F000 -> FF00 until overwritten\n");
}
#endif

static chips_range_t neo1_selected_rom_range(void) {
#if NEO1_PERSONALITY == NEO1_PERSONALITY_50
    return (chips_range_t){
        .ptr = neo1_apple1_rom_bin,
        .size = (size_t)neo1_apple1_rom_bin_len,
    };
#else
    return (chips_range_t){
        .ptr = neo1_system_rom_bin,
        .size = (size_t)neo1_system_rom_bin_len,
    };
#endif
}

//
// USB keyboard input callback.
//
// TinyUSB decodes HID reports in neo1_usb.c and forwards ASCII-like characters
// here. This function normalizes them into the conventions expected by the
// Apple-1 / Replica 1-style machine interface before injecting them into neo1_key_down().
//
// Current normalization policy:
// - LF becomes CR
// - printable ASCII is uppercased
// - control characters are passed through
//
static void neo1_usb_char_in(uint8_t ch, void* user_data) {
    (void)user_data;

#if NEO1_KBD_DEBUG
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

    neo1_key_down(&state.neo1, ch);
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
// Replica 1 / Apple-1 monitor software commonly sets bit 7 on output
// characters, so we strip it before rendering or printing the byte.
//
static void neo1_char_out(uint8_t ch, void* user_data) {
    (void)user_data;

    // Replica 1 / Apple-1 monitor output often has bit 7 set. Strip it for terminal display.
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
static neo1_desc_t neo1_desc(void) {
    const chips_range_t rom = neo1_selected_rom_range();

    return (neo1_desc_t){
        .debug = {{0}},
        .roms = {
            .rom = {
                .ptr = rom.ptr,
                .size = rom.size,
            },
        },
        .char_out = {
            .func = neo1_char_out,
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

    neo1_desc_t desc = neo1_desc();
    neo1_init(&state.neo1, &desc);

#if NEO1_PERSONALITY == NEO1_PERSONALITY_50
    neo1_install_neo150_entry_stubs(&state.neo1);
#endif

    neo1_reset(&state.neo1);
    neo1_install_ram_tools(&state.neo1);

#if NEO1_ENABLE_VCFFA1
    neo1_cffa1_init();
#endif
    neo1_msc_init();
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
// Both paths normalize characters into the same Apple-1 / Replica 1-style machine
// input convention and then inject them via neo1_key_down().
//

#ifndef NEO1_KBD_DEBUG
#define NEO1_KBD_DEBUG (0)
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

    neo1_key_down(&state.neo1, (uint8_t)ch);
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
    const neo1_trace_event_t* ev = 0;
    uint32_t count = neo1_read_startup_trace(&state.neo1, &ev);

    printf("[neo1] startup trace (%u events)\n", (unsigned)count);

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
    printf("[neo1] configuring DVI...\n");
   neo1_video_init(&state.term);

    // DVI init changes the system clock; reinitialize stdio/UART so the
    // serial console stays at the expected baud rate.
    stdio_init_all();

    printf("[neo1] starting DVI core...\n");
   neo1_video_start();
    
    sleep_ms(200);

    printf("[neo1] starting...\n");

    const chips_range_t rom = neo1_selected_rom_range();
    printf("[neo1] personality=%u rom_base=$%04X rom_protect_base=$%04X rom_size=%u bytes\n",
        (unsigned)NEO1_PERSONALITY,
        (unsigned)NEO1_ROM_BASE,
        (unsigned)NEO1_ROM_PROTECT_BASE,
        (unsigned)rom.size);

    printf("[neo1] vectors: NMI=%02X%02X RESET=%02X%02X IRQ=%02X%02X\n",
        state.neo1.ram[0xFFFB], state.neo1.ram[0xFFFA],
        state.neo1.ram[0xFFFD], state.neo1.ram[0xFFFC],
        state.neo1.ram[0xFFFF], state.neo1.ram[0xFFFE]);

    printf("[neo1] capturing startup trace...\n");

    // Capture enough early bus activity to understand reset/startup behavior,
    // but keep printing outside the bus loop so timing stays predictable.
    while (state.neo1.startup_trace_len < NEO1_TRACE_COUNT) {
        neo1_tick(&state.neo1);
    }

    print_startup_trace();

    printf("[neo1] entering run loop...\n");

    while (1) {
        uint32_t start_time_us = time_us_32();

        poll_keyboard();
        neo1_usb_task();

        if (neo1_usb_msc_mounted() && !msc_listed) {
            neo1_msc_list_files();
            msc_listed = true;
        }

        // Run a modest chunk of machine cycles per host iteration.
        // This is intentionally simple and can be revisited later if Neo1-23
        // needs different pacing or tighter synchronization.
        const uint32_t num_ticks = 5000;
        for (uint32_t i = 0; i < num_ticks; i++) {
            neo1_tick(&state.neo1);
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