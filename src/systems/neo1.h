// neo1.h
//
// Transitional physical-CPU wrapper consumed by the Pico target. The
// CPU-neutral 6502-visible state is owned by neo1_machine_t; this wrapper still
// embeds the WDC bus adapter, execution/reset policy, startup trace, and
// snapshot support. Host-style targets use neo1_soft_runner directly.
//
// The physical CPU is still embedded through the MOS6502CPU_* macro surface.
// Pico feeds captured W65C02 cycles to the CPU-neutral machine. Optional target
// devices attach through explicit read/write ports; the machine retains
// ownership of their address decode.
//
// Use this header the same way as the other Chips-style headers:
//
//     #define CHIPS_IMPL
//     #include "neo1.h"
//
// before including it in exactly one C/C++ translation unit.
//
// Required includes before neo1.h:
//
// - chips/chips_common.h
// - chips/wdc65C02cpu.h
// - chips/clk.h
//
// The including target defines the machine profile before including this file.
// Neo1-23 places and protects an 8 KB ROM at $E000-$FFFF. Neo1-50 places and
// protects WozMon at $FF00-$FFFF. Both profiles expose Apple-1-style I/O at
// $D010-$D013 and Replica 1 display mirrors at $D0F2-$D0F3. The Neo1 MSC
// extension at $D014-$D01C and VCFFA1 signature/register decode are separately
// compile-time optional. NEO1_ENABLE_VACI controls payload installation in a
// runner and requires MSC decode in supported build configurations.
//
// ## zlib/libpng license
//
// Copyright (c) 2023 Veselin Sladkov
// Modifications Copyright (c) 2026
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the
// use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//     1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software in a
//     product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//     2. Altered source versions must be plainly marked as such, and must not
//     be misrepresented as being the original software.
//     3. This notice may not be removed or altered from any source
//     distribution.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifndef NEO1_SLEEP_US
#if defined(PICO_ON_DEVICE) || defined(PICO_RP2040)
#define NEO1_SLEEP_US(us) sleep_us(us)
#else
#define NEO1_SLEEP_US(us) ((void)(us))
#endif
#endif

#ifndef NEO1_ROM_BASE
#define NEO1_ROM_BASE (0xE000)
#endif

#ifndef NEO1_ROM_PROTECT_BASE
#define NEO1_ROM_PROTECT_BASE (NEO1_ROM_BASE)
#endif

#ifndef NEO1_ENABLE_VCFFA1
#define NEO1_ENABLE_VCFFA1 (1)
#endif

#ifndef NEO1_ENABLE_MSC
#define NEO1_ENABLE_MSC (1)
#endif

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS (0)
#endif

#include "systems/neo1_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

// Version 3 stores CPU-neutral state in neo1_machine_t.
#define NEO1_SNAPSHOT_VERSION (3)
#define NEO1_FREQUENCY        (1021800)

enum {
    NEO1_MEM_SIZE     = NEO1_MACHINE_MEM_SIZE,
    NEO1_ROM_SIZE     = (0x10000 - NEO1_ROM_BASE),

    NEO1_TRACE_COUNT  = 64,
};

// -----------------------------------------------------------------------------
// types
// -----------------------------------------------------------------------------

typedef struct {
    uint16_t addr;
    uint8_t data;
    bool rw;   // true = read, false = write
} neo1_trace_event_t;

typedef struct {
    chips_debug_t debug;   // optional debugging hook

    struct {
        chips_range_t rom; // required: top ROM payload copied to NEO1_ROM_BASE
    } roms;

    struct {
        neo1_char_out_t func; // optional display output callback
        void* user_data;
    } char_out;

    struct {
        neo1_device_port_t msc;
        neo1_device_port_t vcffa1;
    } devices;
} neo1_desc_t;

typedef struct {
    MOS6502CPU_T cpu;
    neo1_machine_t machine;
    bool valid;
    chips_debug_t debug;

    // First physical external-bus accesses captured by _neo1_mem_rw().
    neo1_trace_event_t startup_trace[NEO1_TRACE_COUNT];
    uint32_t startup_trace_len;
    bool startup_trace_complete;

    uint32_t system_ticks;
} neo1_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

void neo1_init(neo1_t* sys, const neo1_desc_t* desc);
void neo1_discard(neo1_t* sys);
void neo1_reset(neo1_t* sys);
// Advance one physical W65C02 bus cycle.
uint32_t neo1_tick(neo1_t* sys);
// Execute the requested physical-cycle budget derived from host time.
uint32_t neo1_exec(neo1_t* sys, uint32_t micro_seconds);

// Explicit CPU-facing bus surface forwarded to the CPU-neutral machine.
uint8_t neo1_bus_read(neo1_t* sys, uint16_t addr);
void neo1_bus_write(neo1_t* sys, uint16_t addr, uint8_t data);

// Mutable backing memory for runner-installed RAM payloads and diagnostics.
uint8_t* neo1_memory(neo1_t* sys);

// Inject one ASCII keyboard byte. LF is normalized to CR, bit 7 is set, and a
// pending byte is preserved until the 6502 consumes it.
void neo1_key_down(neo1_t* sys, uint8_t ascii);

// Return the captured physical startup trace and optionally its backing array.
uint32_t neo1_read_startup_trace(const neo1_t* sys, const neo1_trace_event_t** out_events);

// Snapshot helpers
uint32_t neo1_save_snapshot(neo1_t* sys, neo1_t* dst);
bool neo1_load_snapshot(neo1_t* sys, uint32_t version, neo1_t* src);

#ifdef __cplusplus
} // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL

#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

// -----------------------------------------------------------------------------
// internal helpers
// -----------------------------------------------------------------------------

static inline void _neo1_capture_trace(neo1_t* sys, uint16_t addr, uint8_t data, bool rw) {
    if (sys->startup_trace_complete) {
        return;
    }

    if (sys->startup_trace_len < NEO1_TRACE_COUNT) {
        neo1_trace_event_t* ev = &sys->startup_trace[sys->startup_trace_len++];
        ev->addr = addr;
        ev->data = data;
        ev->rw = rw;

        if (sys->startup_trace_len == NEO1_TRACE_COUNT) {
            sys->startup_trace_complete = true;
        }
    }
}

// Service one captured bus access from current CPU cycle and trace it.
static inline void _neo1_mem_rw(neo1_t* sys, uint16_t addr, bool rw) {
    if (rw) {
        uint8_t data = neo1_bus_read(sys, addr);
        MOS6502CPU_SET_DATA(&sys->cpu, data);
        _neo1_capture_trace(sys, addr, data, true);
    } else {
        uint8_t data = MOS6502CPU_GET_DATA(&sys->cpu);
        neo1_bus_write(sys, addr, data);
        _neo1_capture_trace(sys, addr, data, false);
    }
}

// -----------------------------------------------------------------------------
// public implementation
// -----------------------------------------------------------------------------

void neo1_init(neo1_t* sys, const neo1_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(*sys));
    sys->valid = true;
    sys->debug = desc->debug;

    CHIPS_ASSERT(desc->roms.rom.ptr);
    CHIPS_ASSERT(desc->roms.rom.size > 0);
    CHIPS_ASSERT(((uint32_t)NEO1_ROM_BASE + (uint32_t)desc->roms.rom.size) <= NEO1_MEM_SIZE);

#if NEO1_ENABLE_MSC
    CHIPS_ASSERT(desc->devices.msc.read && desc->devices.msc.write);
#endif
#if NEO1_ENABLE_VCFFA1
    CHIPS_ASSERT(desc->devices.vcffa1.read && desc->devices.vcffa1.write);
#endif

    const neo1_machine_desc_t machine_desc = {
        .rom = desc->roms.rom.ptr,
        .rom_size = desc->roms.rom.size,
        .rom_base = NEO1_ROM_BASE,
        .rom_protect_base = NEO1_ROM_PROTECT_BASE,
        .char_out = desc->char_out.func,
        .char_out_user_data = desc->char_out.user_data,
#if NEO1_ENABLE_MSC
        .msc = desc->devices.msc,
#endif
#if NEO1_ENABLE_VCFFA1
        .vcffa1 = desc->devices.vcffa1,
#endif
    };
    const bool machine_initialized = neo1_machine_init(&sys->machine, &machine_desc);
    CHIPS_ASSERT(machine_initialized);
    (void)machine_initialized;

    // Initialize the physical adapter only after memory and vectors exist.
    MOS6502CPU_INIT(&sys->cpu, sys);

#if NEO1_DIAGNOSTICS
    const uint8_t* ram = sys->machine.ram;
    printf("[neo1] mem E000=%02X E001=%02X F000=%02X F001=%02X FFFA=%02X FFFB=%02X FFFC=%02X FFFD=%02X FFFE=%02X FFFF=%02X\n",
        ram[0xE000], ram[0xE001],
        ram[0xF000], ram[0xF001],
        ram[0xFFFA], ram[0xFFFB],
        ram[0xFFFC], ram[0xFFFD],
        ram[0xFFFE], ram[0xFFFF]);
#endif

    // Clear startup trace.
    sys->startup_trace_len = 0;
    sys->startup_trace_complete = false;

    sys->system_ticks = 0;
}

void neo1_discard(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void neo1_reset(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    neo1_machine_reset(&sys->machine);
    sys->startup_trace_len = 0;
    sys->startup_trace_complete = false;
    sys->system_ticks = 0;

    MOS6502CPU_SET_IRQ(&sys->cpu, false);

    // The hardware adapter drives active-low RESET on GPIO 26; the signal
    // reaches the physical CPU only when the board's reset connection is made.
    MOS6502CPU_SET_RESET(&sys->cpu, true);
    NEO1_SLEEP_US(1000);
    MOS6502CPU_SET_RESET(&sys->cpu, false);
}

uint8_t neo1_bus_read(neo1_t* sys, uint16_t addr) {
    CHIPS_ASSERT(sys && sys->valid);
    return neo1_machine_read(&sys->machine, addr);
}

void neo1_bus_write(neo1_t* sys, uint16_t addr, uint8_t data) {
    CHIPS_ASSERT(sys && sys->valid);
    neo1_machine_write(&sys->machine, addr, data);
}

uint8_t* neo1_memory(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->machine.ram;
}

uint32_t neo1_tick(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    // The physical adapter advances PHI2 and captures address/R/W first; the
    // machine then services that observed access.
    const uint32_t cpu_cycles = MOS6502CPU_TICK(&sys->cpu);
    CHIPS_ASSERT(cpu_cycles > 0);
    _neo1_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);

    sys->system_ticks += cpu_cycles;
    return cpu_cycles;
}

// Execute for a host-time budget measured in physical CPU cycles.
uint32_t neo1_exec(neo1_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);

    const uint32_t requested_cycles = clk_us_to_ticks(NEO1_FREQUENCY, micro_seconds);
    uint32_t executed_cycles = 0;

    if (0 == sys->debug.callback.func) {
        while (executed_cycles < requested_cycles) {
            executed_cycles += neo1_tick(sys);
        }
    } else {
        // Debug callback mode allows cooperative stop conditions.
        while ((executed_cycles < requested_cycles) && !(*sys->debug.stopped)) {
            executed_cycles += neo1_tick(sys);
            sys->debug.callback.func(sys->debug.callback.user_data, 0);
        }
    }

    return executed_cycles;
}

void neo1_key_down(neo1_t* sys, uint8_t ascii) {
    CHIPS_ASSERT(sys && sys->valid);

    neo1_machine_key_down(&sys->machine, ascii);
}

uint32_t neo1_read_startup_trace(const neo1_t* sys, const neo1_trace_event_t** out_events) {
    CHIPS_ASSERT(sys);
    if (out_events) {
        *out_events = sys->startup_trace;
    }
    return sys->startup_trace_len;
}

uint32_t neo1_save_snapshot(neo1_t* sys, neo1_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    return NEO1_SNAPSHOT_VERSION;
}

// Restore snapshot with version check and current callback/port reconnection.
bool neo1_load_snapshot(neo1_t* sys, uint32_t version, neo1_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != NEO1_SNAPSHOT_VERSION) {
        return false;
    }

    const neo1_char_out_t current_char_out = sys->machine.char_out;
    void* const current_char_out_user_data = sys->machine.char_out_user_data;
    const neo1_device_port_t current_msc = sys->machine.msc;
    const neo1_device_port_t current_vcffa1 = sys->machine.vcffa1;
    static neo1_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    im.machine.char_out = current_char_out;
    im.machine.char_out_user_data = current_char_out_user_data;
    im.machine.msc = current_msc;
    im.machine.vcffa1 = current_vcffa1;
    *sys = im;
    return true;
}

#endif // CHIPS_IMPL
