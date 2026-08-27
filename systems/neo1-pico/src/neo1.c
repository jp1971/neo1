// neo1.c
//
// Olimex Neo6502 runner for the Neo1-23 and Neo1-50 machine profiles.
//
// This file owns target assembly and lifecycle rather than the 6502-visible
// machine model. Its current responsibilities include shared-profile selection,
// Pico-only RAM-tool and Neo1-50 entry-stub installation, physical reset/startup
// sequencing, terminal publication, UART/USB input, storage initialization, and
// fixed-batch bus execution. RP2040 services are split across:
//
// - terminal/neo1_terminal.* : shared text grid and primitive mutations
// - neo1_terminal_pico.*     : Pico control-byte policy and debug helpers
// - neo1_wdc_runner.*         : physical CPU bus timing and machine access
// - neo1_video.*              : PicoDVI text rendering and scanline generation
// - neo1_usb.*                : TinyUSB HID keyboard and MSC host lifecycle
// - systems/neo1_machine.*    : shared memory, ROM, PIA, and device dispatch
//
// Notes:
// - This runner targets the Olimex Neo6502 and its physical W65C02 exclusively
// - VACI and VCFFA1 images installed here are ordinary writable 6502 RAM

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "systems/neo1_machine.h"

#ifndef NEO1_PERSONALITY
#define NEO1_PERSONALITY (50)
#endif

#include "neo1_terminal_pico.h"
#include "neo1_wdc_runner.h"
#include "neo1_video.h"
#include "neo1_msc.h"
#if NEO1_ENABLE_VCFFA1
#include "neo1_cffa1.h"
#include "ram/neo1_cffa1_m2_blockdrv.h"
#endif
#include "neo1_usb.h"

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

#define NEO1_SOFT_RESET_KEY (0x12) // Ctrl-R

// -----------------------------------------------------------------------------
// local machine/platform state
// -----------------------------------------------------------------------------

typedef struct {
    neo1_machine_t machine;
    neo1_wdc_runner_t cpu;
    neo1_terminal_t term;
#if NEO1_ENABLE_MSC
    neo1_msc_t msc;
#endif
} state_t;

static state_t __not_in_flash() state;
#if NEO1_DIAGNOSTICS
static bool msc_listed = false;
#endif

static void neo1_install_ram_tools(neo1_machine_t* machine) {
#if NEO1_ENABLE_VCFFA1
    const uint32_t m2_size = (uint32_t)sizeof(neo1_cffa1_m2_blockdrv);
    const uint32_t m2_addr = NEO1_CFFA1_M2_BLOCKDRV_ADDR;
    assert((m2_addr + m2_size) <= machine->rom_protect_base);
    memcpy(&machine->ram[m2_addr], neo1_cffa1_m2_blockdrv, m2_size);
#if NEO1_DIAGNOSTICS
    printf("[neo1] cffa1 m2 blockdrv installed at $%04X (%lu bytes), run with %04XR\n",
           (unsigned)m2_addr,
           (unsigned long)m2_size,
            (unsigned)NEO1_CFFA1_M2_TESTMAIN_ADDR);
#endif
#else
#if NEO1_DIAGNOSTICS
    printf("[neo1] vcffa1 disabled; $AFF0-$AFFF and $AFDC-$AFDD remain free\n");
#endif
    (void)machine;
#endif

#if NEO1_ENABLE_VACI
    const uint32_t vaci_size = (uint32_t)sizeof(neo1_vaci_v1);
    const uint32_t vaci_addr = NEO1_VACI_V1_ADDR;
    assert((vaci_addr + vaci_size) <= machine->rom_protect_base);
    assert(NEO1_VACI_V1_ROM_PROTECT_HI_OFFSET < vaci_size);
    memcpy(&machine->ram[vaci_addr], neo1_vaci_v1, vaci_size);
    machine->ram[vaci_addr + NEO1_VACI_V1_ROM_PROTECT_HI_OFFSET] =
        (uint8_t)(machine->rom_protect_base >> 8);
#if NEO1_DIAGNOSTICS
    printf("[neo1] vaci v1 installed at $%04X (%lu bytes), run with %04XR\n",
           (unsigned)vaci_addr,
           (unsigned long)vaci_size,
           (unsigned)vaci_addr);
#endif
#else
#if NEO1_DIAGNOSTICS
    printf("[neo1] vaci v1 disabled; $C100 remains free for monitor or hardware ACI use\n");
#endif
#endif
}

//
// Copy the core-0 terminal state into the video module's producer-owned
// snapshot. Core 1 accepts only completed publications at a frame boundary.
//
static void neo1_video_sync_terminal(void) {
   neo1_video_set_terminal(&state.term);
}

static void neo1_soft_reset(void) {
    neo1_machine_reset(&state.machine);
    neo1_wdc_runner_reset(&state.cpu);
}

static void neo1_install_neo150_entry_stubs(neo1_machine_t* machine) {
    // In Neo1-50, E000/F000 are writable load targets. Until user code is
    // loaded there, jumping to them would run uninitialized bytes and hang.
    // Install minimal JMP $FF00 stubs so E000R/F000R return to WozMon.
    static const uint8_t jmp_wozmon[] = { 0x4C, 0x00, 0xFF };
    memcpy(&machine->ram[0xE000], jmp_wozmon, sizeof(jmp_wozmon));
    memcpy(&machine->ram[0xF000], jmp_wozmon, sizeof(jmp_wozmon));
#if NEO1_DIAGNOSTICS
    printf("[neo1] neo1-50 entry stubs installed: E000/F000 -> FF00 until overwritten\n");
#endif
}

//
// USB keyboard input callback.
//
// TinyUSB decodes HID reports in neo1_usb.c and forwards ASCII-like characters
// here. This function normalizes them into the conventions expected by the
// Apple-1 / Replica 1-style machine interface before injecting them into the
// shared keyboard latch.
//
// Current normalization policy:
// - LF becomes CR
// - printable ASCII is uppercased
// - control characters are passed through
//
static void neo1_usb_char_in(uint8_t ch, void* user_data) {
    (void)user_data;

#if NEO1_DIAGNOSTICS
    printf("[usb] ascii=%02X", (unsigned)ch);
    if ((ch >= 32) && (ch < 127)) {
        printf(" '%c'", ch);
    } else if (ch == '\r') {
        printf(" <CR>");
    }
    printf("\n");
#endif

    if (ch == NEO1_SOFT_RESET_KEY) {
        neo1_soft_reset();
        return;
    }

    if (ch == '\n') {
        ch = '\r';
    } else if (isascii(ch)) {
        ch = (uint8_t)toupper(ch);
    }

    neo1_machine_key_down(&state.machine, ch);
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
    neo1_terminal_pico_putc(&state.term, ch);
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

#if NEO1_ENABLE_MSC
static uint8_t neo1_msc_port_read(void* user_data, uint16_t addr) {
    return neo1_msc_read((neo1_msc_t*)user_data, addr);
}

static void neo1_msc_port_write(void* user_data, uint16_t addr, uint8_t data) {
    neo1_msc_write((neo1_msc_t*)user_data, addr, data);
}
#endif

#if NEO1_ENABLE_VCFFA1
static uint8_t neo1_cffa1_port_read(void* user_data, uint16_t addr) {
    (void)user_data;
    return neo1_cffa1_io_read(addr);
}

static void neo1_cffa1_port_write(void* user_data, uint16_t addr, uint8_t data) {
    (void)user_data;
    neo1_cffa1_io_write(addr, data);
}
#endif

//
// Build the description consumed by the CPU-neutral Neo1 machine.
//
// This is where the selected machine profile is connected to the shared model:
// - which ROM image is presented at the top of memory
// - which output callback receives machine-generated characters
//
static neo1_machine_desc_t neo1_machine_desc(void) {
    const neo1_profile_t* profile = neo1_profile_find(NEO1_PERSONALITY);
    assert(profile);

    return (neo1_machine_desc_t){
        .profile = profile,
        .char_out = neo1_char_out,
        .char_out_user_data = 0,
#if NEO1_ENABLE_MSC
        .msc = {
            .read = neo1_msc_port_read,
            .write = neo1_msc_port_write,
            .user_data = &state.msc,
        },
#endif
#if NEO1_ENABLE_VCFFA1
        .vcffa1 = {
            .read = neo1_cffa1_port_read,
            .write = neo1_cffa1_port_write,
        },
#endif
    };
}

//
// Initialize the shared machine and Pico-owned support state.
//
// Order matters here:
// 1. clear the terminal state
// 2. initialize memory/ROM, then configure the runner and its first reset pulse
// 3. install Neo1-50 safety stubs, reset machine and CPU, then install RAM tools
// 4. reset optional storage protocol state and initialize TinyUSB host
// 5. DVI initialization snapshots terminal state later in main
//
static void app_init(void) {
    neo1_terminal_clear(&state.term);

    const neo1_machine_desc_t desc = neo1_machine_desc();
    const bool machine_initialized = neo1_machine_init(&state.machine, &desc);
    assert(machine_initialized);
    (void)machine_initialized;

    const bool runner_initialized =
        neo1_wdc_runner_init(&state.cpu, &state.machine);
    assert(runner_initialized);
    (void)runner_initialized;

#if NEO1_DIAGNOSTICS
    const uint8_t* ram = state.machine.ram;
    printf("[neo1] mem E000=%02X E001=%02X F000=%02X F001=%02X FFFA=%02X FFFB=%02X FFFC=%02X FFFD=%02X FFFE=%02X FFFF=%02X\n",
        ram[0xE000], ram[0xE001],
        ram[0xF000], ram[0xF001],
        ram[0xFFFA], ram[0xFFFB],
        ram[0xFFFC], ram[0xFFFD],
        ram[0xFFFE], ram[0xFFFF]);
#endif

    if (state.machine.profile->personality == NEO1_PERSONALITY_50) {
        neo1_install_neo150_entry_stubs(&state.machine);
    }

    neo1_machine_reset(&state.machine);
    neo1_wdc_runner_reset(&state.cpu);
    neo1_install_ram_tools(&state.machine);

#if NEO1_ENABLE_VCFFA1
    neo1_cffa1_init();
#endif
#if NEO1_ENABLE_MSC
    const bool msc_initialized =
        neo1_msc_init(&state.msc, neo1_msc_fatfs_backend());
    assert(msc_initialized);
    (void)msc_initialized;
#endif
    neo1_usb_init(neo1_usb_char_in, 0);

}

// -----------------------------------------------------------------------------
// UART keyboard input path
// -----------------------------------------------------------------------------

//
// Poll the UART/stdin keyboard path.
//
// The Pico runner polls two input transports in parallel:
// - UART/stdin polling here
// - USB keyboard input through neo1_usb_task()
//
// Both paths uppercase printable ASCII, normalize LF to CR, recognize Ctrl-R as
// runner reset, and inject the byte through the shared keyboard latch. The
// machine retains only the first byte while one is pending, regardless of
// which transport supplied it.
//

#ifndef NEO1_TERM_DEBUG
#define NEO1_TERM_DEBUG NEO1_DIAGNOSTICS
#endif

static void poll_keyboard(void) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) {
        return;
    }

    if (ch == NEO1_SOFT_RESET_KEY) {
        neo1_soft_reset();
        return;
    }

#if NEO1_TERM_DEBUG
    // Ctrl-D dumps the software terminal buffer for debugging.
    if (ch == 0x04) {
        neo1_terminal_pico_dump(&state.term);
        return;
    }
#endif


    // Normalize terminal input slightly for WozMon.
    if (ch == '\n') {
        ch = '\r';
    } else if (isascii(ch)) {
        ch = toupper(ch);
    }

    neo1_machine_key_down(&state.machine, (uint8_t)ch);
}

// -----------------------------------------------------------------------------
// startup trace support
// -----------------------------------------------------------------------------

//
// Dump the buffered startup bus trace captured by the physical runner.
//
// This exposes the first physical memory transactions after reset without
// printing from the timing-sensitive bus-service path.
//
#if NEO1_DIAGNOSTICS
static void print_startup_trace(void) {
    const neo1_wdc_trace_event_t* ev = 0;
    uint32_t count = neo1_wdc_runner_read_startup_trace(&state.cpu, &ev);

    printf("[neo1] startup trace (%u events)\n", (unsigned)count);

    for (uint32_t i = 0; i < count; i++) {
        printf("%c %04X %02X\n",
               ev[i].rw ? 'R' : 'W',
               ev[i].addr,
               ev[i].data);
    }
}
#endif

// -----------------------------------------------------------------------------
// program entry point
// -----------------------------------------------------------------------------

//
// Main boot flow:
// 1. initialize stdio/UART
// 2. initialize the machine and platform services
// 3. bring up DVI if enabled
// 4. capture the startup trace and print it when diagnostics are enabled
// 5. print one readiness summary and enter the steady-state run loop
//
// The steady-state loop interleaves:
// - UART polling
// - USB host polling
// - 65C02 execution
// - a fixed 5,000-cycle batch and a minimum one-millisecond loop duration
//
int main(void) {
    stdio_init_all();
    app_init();
#if NEO1_DIAGNOSTICS
    printf("[neo1] configuring DVI...\n");
#endif
   neo1_video_init(&state.term);

    // DVI init changes the system clock; reinitialize stdio/UART so the
    // serial console stays at the expected baud rate.
    stdio_init_all();

#if NEO1_DIAGNOSTICS
    printf("[neo1] starting DVI core...\n");
#endif
   neo1_video_start();
    
    sleep_ms(200);

#if NEO1_DIAGNOSTICS
    printf("[neo1] personality=%u rom_base=$%04X rom_protect_base=$%04X rom_size=%u bytes\n",
        (unsigned)state.machine.profile->personality,
        (unsigned)state.machine.profile->rom_base,
        (unsigned)state.machine.profile->rom_protect_base,
        (unsigned)state.machine.profile->rom_size);

    printf("[neo1] vectors: NMI=%02X%02X RESET=%02X%02X IRQ=%02X%02X\n",
        state.machine.ram[0xFFFB], state.machine.ram[0xFFFA],
        state.machine.ram[0xFFFD], state.machine.ram[0xFFFC],
        state.machine.ram[0xFFFF], state.machine.ram[0xFFFE]);

    printf("[neo1] capturing startup trace...\n");
#endif

    // Capture enough early bus activity to understand reset/startup behavior,
    // preserving the verified startup sequence. Printing remains outside the
    // bus loop and is enabled only for a diagnostic build.
    while (state.cpu.startup_trace_len < NEO1_WDC_TRACE_COUNT) {
        neo1_wdc_runner_tick(&state.cpu);
    }

#if NEO1_DIAGNOSTICS
    print_startup_trace();
    printf("[neo1] entering run loop...\n");
#endif

    printf("[neo1] ready personality=%u vaci=%u vcffa1=%u diagnostics=%u\n",
           (unsigned)state.machine.profile->personality,
           (unsigned)NEO1_ENABLE_VACI,
           (unsigned)NEO1_ENABLE_VCFFA1,
           (unsigned)NEO1_DIAGNOSTICS);

    while (1) {
        uint32_t start_time_us = time_us_32();

        poll_keyboard();
        neo1_usb_task();

#if NEO1_DIAGNOSTICS
        if (neo1_usb_msc_mounted() && !msc_listed) {
            neo1_msc_list_files();
            msc_listed = true;
        }
#endif

        // Service exactly 5,000 physical bus cycles before polling transports
        // again. This batching policy belongs to the Pico runner, not the
        // shared machine contract.
        const uint32_t num_ticks = 5000;
        for (uint32_t i = 0; i < num_ticks; i++) {
            neo1_wdc_runner_tick(&state.cpu);
        }

        uint32_t end_time_us = time_us_32();
        uint32_t execution_time = end_time_us - start_time_us;

        // Enforce only a minimum host-loop duration; this is not a cycle-rate
        // governor when the bus batch itself takes longer than one millisecond.
        int32_t sleep_time = 1000 - (int32_t)execution_time;
        if (sleep_time > 0) {
            sleep_us((uint32_t)sleep_time);
        }
    }

    __builtin_unreachable();
}
