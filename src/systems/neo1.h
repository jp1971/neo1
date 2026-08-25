// neo1.h
//
// Transitional shared Neo1 machine implementation consumed by both the Pico
// and SDL targets. It owns the 64 KB backing store, selected top-ROM placement
// and write protection, Apple-1 keyboard/display register behavior, storage
// address decoding, and the display-byte callback.
//
// The CPU is still embedded through the MOS6502CPU_* macro adapter. Pico feeds
// captured cycles from a physical W65C02; SDL's software adapter calls the same
// read/write dispatch directly. Optional target devices attach through explicit
// read/write ports; the machine retains ownership of their address decode.
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
// - chips/neo1_cpu_backend.h   (selects CPU backend; default is wdc65C02cpu.h)
// - chips/mem.h
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

#ifndef MOS6502CPU_NEEDS_EXTERNAL_BUS
#define MOS6502CPU_NEEDS_EXTERNAL_BUS (1)
#endif

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

#include "devices/neo1_cffa1.h"
#include "devices/neo1_apple1_pia.h"
#include "devices/neo1_msc.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

// Version 2 stores keyboard/display registers in neo1_apple1_pia_t.
#define NEO1_SNAPSHOT_VERSION (2)
#define NEO1_FREQUENCY        (1021800)

enum {
    NEO1_MEM_SIZE     = 0x10000,
    NEO1_ROM_SIZE     = (0x10000 - NEO1_ROM_BASE),

    NEO1_TRACE_COUNT  = 64,
};

// -----------------------------------------------------------------------------
// types
// -----------------------------------------------------------------------------

typedef void (*neo1_char_out_t)(uint8_t ch, void* user_data);
typedef uint8_t (*neo1_device_read_t)(void* user_data, uint16_t addr);
typedef void (*neo1_device_write_t)(void* user_data, uint16_t addr, uint8_t data);

typedef struct {
    neo1_device_read_t read;
    neo1_device_write_t write;
    void* user_data;
} neo1_device_port_t;

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
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    // Full 64 KB backing store. The selected ROM is copied to NEO1_ROM_BASE.
    uint8_t ram[NEO1_MEM_SIZE];
    uint8_t* rom;

    // Apple-1 keyboard/display registers and pending input latch.
    neo1_apple1_pia_t pia;

    // Optional consumer for bytes written through the display data register.
    neo1_char_out_t char_out;
    void* char_out_user_data;

    // Target-provided implementations behind machine-owned optional decode.
    neo1_device_port_t msc;
    neo1_device_port_t vcffa1;

    // First physical external-bus accesses captured by _neo1_mem_rw(). The
    // soft adapter bypasses that helper, so SDL does not populate this trace.
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
// Advance one adapter step and return the number of represented CPU cycles:
// one physical bus cycle on Pico or one complete instruction on SDL.
uint32_t neo1_tick(neo1_t* sys);
// Execute until at least the requested host-time budget has been represented;
// a software instruction may overshoot the exact cycle target.
uint32_t neo1_exec(neo1_t* sys, uint32_t micro_seconds);

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

static void _neo1_init_memorymap(neo1_t* sys) {
    mem_init(&sys->mem);

    // Map the whole 64K backing store as RAM. We protect ROM writes manually.
    mem_map_ram(&sys->mem, 0, 0x0000, NEO1_MEM_SIZE, sys->ram);
}

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

static inline bool _neo1_cffa1_handles_addr(uint16_t addr) {
    return (addr == NEO1_CFFA1_ID1_ADDR) ||
           (addr == NEO1_CFFA1_ID2_ADDR) ||
           ((addr >= NEO1_CFFA1_IO_BASE) && (addr <= NEO1_CFFA1_IO_END));
}

// Bus read dispatch order:
// 1) route the optional Replica 1 VCFFA1 signature/register addresses
// 2) route Apple-1 keyboard/display registers, including display mirrors
// 3) handle readable Neo1 MSC registers
// 4) fall back to RAM/ROM backing store
static inline uint8_t _neo1_mem_read(neo1_t* sys, uint16_t addr) {
#if NEO1_ENABLE_VCFFA1
    if (_neo1_cffa1_handles_addr(addr)) {
        return sys->vcffa1.read(sys->vcffa1.user_data, addr);
    }
#endif

    if (neo1_apple1_pia_handles_addr(addr)) {
        return neo1_apple1_pia_read(&sys->pia, addr);
    }

    switch (addr) {
#if NEO1_ENABLE_MSC
        case NEO1_IO_MSC_STATUS:
        case NEO1_IO_MSC_DATA:
        case NEO1_IO_MSC_INDEX:
        case NEO1_IO_MSC_INFO:
        case NEO1_IO_MSC_SIZE_LO:
        case NEO1_IO_MSC_SIZE_HI:
            return sys->msc.read(sys->msc.user_data, addr);
#endif

        default:
            return mem_rd(&sys->mem, addr);
    }
}

// Bus write dispatch order mirrors read side:
// 1) route the optional Replica 1 VCFFA1 signature/register addresses
// 2) route Apple-1 keyboard/display registers, including display mirrors
// 3) handle writable Neo1 MSC registers
// 4) write to backing RAM unless inside protected ROM region
static inline void _neo1_mem_write(neo1_t* sys, uint16_t addr, uint8_t data) {
#if NEO1_ENABLE_VCFFA1
    if (_neo1_cffa1_handles_addr(addr)) {
        sys->vcffa1.write(sys->vcffa1.user_data, addr, data);
        return;
    }
#endif

    if (neo1_apple1_pia_handles_addr(addr)) {
        uint8_t display_byte = 0;
        if (neo1_apple1_pia_write(&sys->pia, addr, data, &display_byte) &&
            sys->char_out)
        {
            sys->char_out(display_byte, sys->char_out_user_data);
        }
        return;
    }

    switch (addr) {
#if NEO1_ENABLE_MSC
        case NEO1_IO_MSC_CMD:
        case NEO1_IO_MSC_SECTOR_LO:
        case NEO1_IO_MSC_SECTOR_HI:
        case NEO1_IO_MSC_DATA:
        case NEO1_IO_MSC_INDEX:
        case NEO1_IO_MSC_SIZE_LO:
        case NEO1_IO_MSC_SIZE_HI:
            sys->msc.write(sys->msc.user_data, addr, data);
            break;
#endif

        default:
            // Protect the ROM region starting at NEO1_ROM_PROTECT_BASE.
            if (addr < NEO1_ROM_PROTECT_BASE) {
                mem_wr(&sys->mem, addr, data);
            }
            break;
    }
}

// Service one captured bus access from current CPU cycle and trace it.
static inline void _neo1_mem_rw(neo1_t* sys, uint16_t addr, bool rw) {
    if (rw) {
        uint8_t data = _neo1_mem_read(sys, addr);
        MOS6502CPU_SET_DATA(&sys->cpu, data);
        _neo1_capture_trace(sys, addr, data, true);
    } else {
        uint8_t data = MOS6502CPU_GET_DATA(&sys->cpu);
        _neo1_mem_write(sys, addr, data);
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

    sys->rom = desc->roms.rom.ptr;
    sys->char_out = desc->char_out.func;
    sys->char_out_user_data = desc->char_out.user_data;
#if NEO1_ENABLE_MSC
    CHIPS_ASSERT(desc->devices.msc.read && desc->devices.msc.write);
    sys->msc = desc->devices.msc;
#endif
#if NEO1_ENABLE_VCFFA1
    CHIPS_ASSERT(desc->devices.vcffa1.read && desc->devices.vcffa1.write);
    sys->vcffa1 = desc->devices.vcffa1;
#endif

    // Build flat memory map and preload memory.
    _neo1_init_memorymap(sys);

    // Seed uninitialized backing memory deterministically: even addresses are
    // $00 and odd addresses are $FF.
    for (uint32_t addr = 0; addr < NEO1_MEM_SIZE; addr += 2) {
        sys->ram[addr] = 0x00;
        sys->ram[addr + 1] = 0xFF;
    }

    // Copy the selected top ROM payload to NEO1_ROM_BASE. The selected image
    // should provide NMI/RESET/IRQ vectors at $FFFA-$FFFF.
    memcpy(&sys->ram[NEO1_ROM_BASE], sys->rom, desc->roms.rom.size);

#if (NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02)
    // WozMon's IRQ/BRK vector ($FFFE-$FFFF) points to $0000, which is
    // normally provided by BASIC on a real Apple 1. Without it, any BRK
    // instruction (including $00 from uninitialized memory) causes an
    // infinite BRK loop. For the host-only soft backend, install a
    // JMP to the RESET vector at $0000 so BRK recovers to the monitor.
    {
        const uint16_t reset_vec =
            (uint16_t)sys->ram[0xFFFC] | ((uint16_t)sys->ram[0xFFFD] << 8);
        sys->ram[0x0000] = 0x4C;                       // JMP abs
        sys->ram[0x0001] = (uint8_t)(reset_vec & 0xFF);
        sys->ram[0x0002] = (uint8_t)(reset_vec >> 8);
    }
#endif

    // Initialize the selected CPU adapter only after memory and vectors exist.
    // The soft adapter fetches the reset vector during its initialization.
    MOS6502CPU_INIT(&sys->cpu, sys);

#if NEO1_DIAGNOSTICS
    printf("[neo1] mem E000=%02X E001=%02X F000=%02X F001=%02X FFFA=%02X FFFB=%02X FFFC=%02X FFFD=%02X FFFE=%02X FFFF=%02X\n",
        sys->ram[0xE000], sys->ram[0xE001],
        sys->ram[0xF000], sys->ram[0xF001],
        sys->ram[0xFFFA], sys->ram[0xFFFB],
        sys->ram[0xFFFC], sys->ram[0xFFFD],
        sys->ram[0xFFFE], sys->ram[0xFFFF]);
#endif
        
    neo1_apple1_pia_reset(&sys->pia);

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

    neo1_apple1_pia_reset(&sys->pia);
    sys->startup_trace_len = 0;
    sys->startup_trace_complete = false;
    sys->system_ticks = 0;

    MOS6502CPU_SET_IRQ(&sys->cpu, false);

    // The hardware adapter drives active-low RESET on GPIO 26; the signal
    // reaches the physical CPU only when the board's reset connection is made.
    // The soft adapter resets on assertion and treats release as a no-op.
    MOS6502CPU_SET_RESET(&sys->cpu, true);
    NEO1_SLEEP_US(1000);
    MOS6502CPU_SET_RESET(&sys->cpu, false);
}

uint32_t neo1_tick(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    // The physical adapter advances PHI2 and captures address/R/W first; the
    // machine then services that observed access. The software adapter performs
    // memory callbacks inside its complete-instruction step and therefore skips
    // the external-bus service below.
    const uint32_t cpu_cycles = MOS6502CPU_TICK(&sys->cpu);
    CHIPS_ASSERT(cpu_cycles > 0);
#if MOS6502CPU_NEEDS_EXTERNAL_BUS
    _neo1_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);
#endif

    sys->system_ticks += cpu_cycles;
    return cpu_cycles;
}

// Execute for a host-time budget measured in represented CPU cycles. A physical
// tick is exactly one cycle; a soft tick is one instruction and may overshoot.
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

    neo1_apple1_pia_key_down(&sys->pia, ascii);
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
    mem_snapshot_onsave(&dst->mem, sys);
    return NEO1_SNAPSHOT_VERSION;
}

// Restore snapshot with version check and debug/memory fixups.
bool neo1_load_snapshot(neo1_t* sys, uint32_t version, neo1_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != NEO1_SNAPSHOT_VERSION) {
        return false;
    }

    const neo1_device_port_t current_msc = sys->msc;
    const neo1_device_port_t current_vcffa1 = sys->vcffa1;
    static neo1_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    mem_snapshot_onload(&im.mem, sys);
    im.msc = current_msc;
    im.vcffa1 = current_vcffa1;
    *sys = im;
    return true;
}

#if (NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02)
uint8_t neo1_soft65c02_mem_read(void* user, uint16_t addr) {
    neo1_t* sys = (neo1_t*)user;
    return _neo1_mem_read(sys, addr);
}

void neo1_soft65c02_mem_write(void* user, uint16_t addr, uint8_t data) {
    neo1_t* sys = (neo1_t*)user;
    _neo1_mem_write(sys, addr, data);
}
#endif

#endif // CHIPS_IMPL
