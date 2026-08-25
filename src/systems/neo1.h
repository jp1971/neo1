// neo1.h
//
// Transitional shared Neo1 machine implementation consumed by both the Pico
// and SDL targets. It owns the 64 KB backing store, selected top-ROM placement
// and write protection, Apple-1 keyboard/display register behavior, storage
// address decoding, and the display-byte callback.
//
// The CPU is still embedded through the MOS6502CPU_* macro adapter. Pico feeds
// captured cycles from a physical W65C02; SDL's software adapter calls the same
// read/write dispatch directly. This header also includes storage declarations
// from the Pico target. Those are present implementation leaks, not the desired
// portable-core boundary.
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

#if NEO1_ENABLE_MSC
#include "../../systems/neo1-pico/src/neo1_msc.h"
#endif
#if NEO1_ENABLE_VCFFA1
#include "../../systems/neo1-pico/src/neo1_cffa1.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

#define NEO1_SNAPSHOT_VERSION (1)
#define NEO1_FREQUENCY        (1021800)

enum {
    NEO1_MEM_SIZE     = 0x10000,
    NEO1_ROM_SIZE     = (0x10000 - NEO1_ROM_BASE),

    NEO1_IO_KBD       = 0xD010,
    NEO1_IO_KBDCR     = 0xD011,
    NEO1_IO_DSP       = 0xD012,
    NEO1_IO_DSPCR     = 0xD013,
    NEO1_IO_DSP_ALT   = 0xD0F2,
    NEO1_IO_DSPCR_ALT = 0xD0F3,

    NEO1_TRACE_COUNT  = 64,
};

// -----------------------------------------------------------------------------
// types
// -----------------------------------------------------------------------------

typedef void (*neo1_char_out_t)(uint8_t ch, void* user_data);

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
} neo1_desc_t;

typedef struct {
    MOS6502CPU_T cpu;
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    // Full 64 KB backing store. The selected ROM is copied to NEO1_ROM_BASE.
    uint8_t ram[NEO1_MEM_SIZE];
    uint8_t* rom;

    // Apple-1 keyboard latch; bit 7 is set on injected input bytes.
    uint8_t kbd_latched;

    // Minimal PIA-like state for the control/data-direction behavior WozMon
    // uses. This is not a complete 6820/6821 implementation.
    uint8_t kbd_cr;
    uint8_t dsp_cr;
    uint8_t kbd_ddr;
    uint8_t dsp_ddr;
    uint8_t kbd_data;
    uint8_t dsp_data;
    // Bookkeeping only: DSP/DSPCR reads report ready unconditionally.
    bool dsp_ready;

    // Optional consumer for bytes written through the display data register.
    neo1_char_out_t char_out;
    void* char_out_user_data;

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
void neo1_tick(neo1_t* sys);
uint32_t neo1_exec(neo1_t* sys, uint32_t micro_seconds);

// Inject one ASCII keyboard byte. LF is normalized to CR and bit 7 is set.
// Pico preserves a pending byte; the soft backend replaces it.
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

static inline uint16_t _neo1_normalize_io_addr(uint16_t addr) {
    switch (addr) {
        case NEO1_IO_DSP_ALT:
            return NEO1_IO_DSP;
        case NEO1_IO_DSPCR_ALT:
            return NEO1_IO_DSPCR;
        default:
            return addr;
    }
}

// Bus read dispatch order:
// 1) normalize mirrored addresses
// 2) route the optional Replica 1 VCFFA1 signature/register addresses
// 3) handle Apple-1 keyboard/display and readable Neo1 MSC registers
// 4) fall back to RAM/ROM backing store
static inline uint8_t _neo1_mem_read(neo1_t* sys, uint16_t addr) {
    addr = _neo1_normalize_io_addr(addr);

#if NEO1_ENABLE_VCFFA1
    if (neo1_cffa1_handles_addr(addr)) {
        return neo1_cffa1_io_read(addr);
    }
#endif

    switch (addr) {
        case NEO1_IO_KBD:
#if (NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02)
            // Host robustness: always treat KBD reads as data-port reads so a
            // pending key cannot get stuck behind control-register mode bits.
            {
                uint8_t v = sys->kbd_latched;
                sys->kbd_latched = 0;
                sys->kbd_data = 0;
                return v;
            }
#else
            // If bit 2 is clear, access the DDR. Otherwise access the peripheral register.
            if ((sys->kbd_cr & 0x04) == 0) {
                return sys->kbd_ddr;
            } else {
                uint8_t v = sys->kbd_latched;
                sys->kbd_latched = 0;   // consume the key on read
                sys->kbd_data = 0;
                return v;
            }
#endif

        case NEO1_IO_KBDCR:
            // Preserve the programmed control bits, but report key-ready in bit 7.
            return (sys->kbd_cr & 0x7F) | ((sys->kbd_latched != 0) ? 0x80 : 0x00);

        case NEO1_IO_DSP:
            // If bit 2 is clear, access the DDR. Otherwise access the peripheral register.
            if ((sys->dsp_cr & 0x04) == 0) {
                return sys->dsp_ddr;
            } else {
                // Keep host display non-blocking: always report ready semantics.
                return 0x00;
            }

        case NEO1_IO_DSPCR:
            // Preserve programmed control bits and report ready in bit 7.
            return (sys->dsp_cr & 0x7F) | 0x80;

#if NEO1_ENABLE_MSC
        case NEO1_IO_MSC_STATUS:
        case NEO1_IO_MSC_DATA:
        case NEO1_IO_MSC_INDEX:
        case NEO1_IO_MSC_INFO:
        case NEO1_IO_MSC_SIZE_LO:
        case NEO1_IO_MSC_SIZE_HI:
            return neo1_msc_io_read(addr);
#endif

        default:
            return mem_rd(&sys->mem, addr);
    }
}

// Bus write dispatch order mirrors read side:
// 1) normalize mirrored addresses
// 2) route the optional Replica 1 VCFFA1 signature/register addresses
// 3) handle Apple-1 keyboard/display and writable Neo1 MSC registers
// 4) write to backing RAM unless inside protected ROM region
static inline void _neo1_mem_write(neo1_t* sys, uint16_t addr, uint8_t data) {
    addr = _neo1_normalize_io_addr(addr);

#if NEO1_ENABLE_VCFFA1
    if (neo1_cffa1_handles_addr(addr)) {
        neo1_cffa1_io_write(addr, data);
        return;
    }
#endif

    switch (addr) {
        case NEO1_IO_KBD:
            if ((sys->kbd_cr & 0x04) == 0) {
                sys->kbd_ddr = data;
            } else {
                sys->kbd_data = data;
            }
            break;

        case NEO1_IO_KBDCR:
            sys->kbd_cr = data;
            break;

        case NEO1_IO_DSP:
            if ((sys->dsp_cr & 0x04) == 0) {
                sys->dsp_ddr = data;
            } else {
                sys->dsp_data = data;
                sys->dsp_ready = true;
                if (sys->char_out) {
                    sys->char_out(data, sys->char_out_user_data);
                }
            }
            break;

        case NEO1_IO_DSPCR:
            sys->dsp_cr = data;
            break;

#if NEO1_ENABLE_MSC
        case NEO1_IO_MSC_CMD:
        case NEO1_IO_MSC_SECTOR_LO:
        case NEO1_IO_MSC_SECTOR_HI:
        case NEO1_IO_MSC_DATA:
        case NEO1_IO_MSC_INDEX:
        case NEO1_IO_MSC_SIZE_LO:
        case NEO1_IO_MSC_SIZE_HI:
            neo1_msc_io_write(addr, data);
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
        
    // Default no key pending and reset the minimal PIA-like state.
    sys->kbd_latched = 0;
    sys->kbd_cr = 0x00;
    sys->dsp_cr = 0x00;
    sys->kbd_ddr = 0x00;
    sys->dsp_ddr = 0x00;
    sys->kbd_data = 0x00;
    sys->dsp_data = 0x00;
    sys->dsp_ready = true;

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

    sys->kbd_latched = 0;
    sys->kbd_cr = 0x00;
    sys->dsp_cr = 0x00;
    sys->kbd_ddr = 0x00;
    sys->dsp_ddr = 0x00;
    sys->kbd_data = 0x00;
    sys->dsp_data = 0x00;
    sys->dsp_ready = true;
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

void neo1_tick(neo1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    // The physical adapter advances PHI2 and captures address/R/W first; the
    // machine then services that observed access. The software adapter performs
    // memory callbacks inside its complete-instruction step and therefore skips
    // the external-bus service below.
    MOS6502CPU_TICK(&sys->cpu);
#if MOS6502CPU_NEEDS_EXTERNAL_BUS
    _neo1_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);
#endif

    sys->system_ticks++;
}

// Execute for a host-time budget by converting microseconds to machine ticks.
uint32_t neo1_exec(neo1_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);

    uint32_t num_ticks = clk_us_to_ticks(NEO1_FREQUENCY, micro_seconds);

    if (0 == sys->debug.callback.func) {
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            neo1_tick(sys);
        }
    } else {
        // Debug callback mode allows cooperative stop conditions.
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            neo1_tick(sys);
            sys->debug.callback.func(sys->debug.callback.user_data, 0);
        }
    }

    return num_ticks;
}

void neo1_key_down(neo1_t* sys, uint8_t ascii) {
    CHIPS_ASSERT(sys && sys->valid);

    // Replica 1 / Apple-1 monitor convention: set bit 7 on incoming key.
    // Translate LF to CR for convenience.
    if (ascii == '\n') {
        ascii = '\r';
    }

    // SDL accommodation: replace a pending byte so host input cannot remain
    // blocked behind a stale latch value.
#if (NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02)
    sys->kbd_latched = ascii | 0x80;
#else
    // Physical-machine behavior: retain the first byte until the 6502 reads it.
    if (sys->kbd_latched == 0) {
        sys->kbd_latched = ascii | 0x80;
    }
#endif
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

    static neo1_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    mem_snapshot_onload(&im.mem, sys);
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
