// apple1.h
//
// Minimal Apple-1 runtime for Neo6502 using Reload's hardware 65C02 glue.
//
// Use this header the same way as the other Chips-style headers:
//
//     #define CHIPS_IMPL
//     #include "apple1.h"
//
// before including it in exactly one C/C++ translation unit.
//
// Required includes before apple1.h:
//
// - chips/chips_common.h
// - chips/wdc65C02cpu.h   (for Neo6502 hardware CPU path)
// - chips/mem.h
// - chips/clk.h
//
// This version is intentionally minimal:
// - real WDC65C02 on Neo6502
// - flat 64K memory backing store
// - Neo 1 top ROM image at $E000-$FFFF (8 KB, including vectors)
// - Apple-1 style I/O at $D010-$D013
// - buffered startup trace (no live printf in bus loop)
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

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

#define APPLE1_SNAPSHOT_VERSION (1)
#define APPLE1_FREQUENCY        (1021800)

enum {
    APPLE1_MEM_SIZE     = 0x10000,
    APPLE1_ROM_BASE     = 0xE000,
    APPLE1_ROM_SIZE     = 0x2000,

    APPLE1_IO_KBD       = 0xD010,
    APPLE1_IO_KBDCR     = 0xD011,
    APPLE1_IO_DSP       = 0xD012,
    APPLE1_IO_DSPCR     = 0xD013,

    APPLE1_TRACE_COUNT  = 64,
};

// -----------------------------------------------------------------------------
// types
// -----------------------------------------------------------------------------

typedef void (*apple1_char_out_t)(uint8_t ch, void* user_data);

typedef struct {
    uint16_t addr;
    uint8_t data;
    bool rw;   // true = read, false = write
} apple1_trace_event_t;

typedef struct {
    chips_debug_t debug;   // optional debugging hook

    struct {
        chips_range_t rom; // required: 8 KB Neo 1 top ROM image including vectors
    } roms;

    struct {
        apple1_char_out_t func; // optional display output callback
        void* user_data;
    } char_out;
} apple1_desc_t;

typedef struct {
    MOS6502CPU_T cpu;
    mem_t mem;
    bool valid;
    chips_debug_t debug;

    // Full 64K backing store. ROM is copied into $E000-$FFFF at init.
    uint8_t ram[APPLE1_MEM_SIZE];
    uint8_t* rom;

    // Simple Apple-1 input latch (bit 7 set means "valid"/ready convention)
    uint8_t kbd_latched;

    // Minimal Apple-1 PIA-like state. We do not emulate a full 6820/6821,
    // only the control/data-direction behavior WozMon relies on.
    uint8_t kbd_cr;
    uint8_t dsp_cr;
    uint8_t kbd_ddr;
    uint8_t dsp_ddr;
    uint8_t kbd_data;
    uint8_t dsp_data;

    // Optional display callback
    apple1_char_out_t char_out;
    void* char_out_user_data;

    // Buffered startup trace
    apple1_trace_event_t startup_trace[APPLE1_TRACE_COUNT];
    uint32_t startup_trace_len;
    bool startup_trace_complete;

    uint32_t system_ticks;
} apple1_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

void apple1_init(apple1_t* sys, const apple1_desc_t* desc);
void apple1_discard(apple1_t* sys);
void apple1_reset(apple1_t* sys);
void apple1_tick(apple1_t* sys);
uint32_t apple1_exec(apple1_t* sys, uint32_t micro_seconds);

// Inject one Apple-1 keyboard character (ASCII). Bit 7 will be set internally.
void apple1_key_down(apple1_t* sys, uint8_t ascii);

// Startup trace accessors
uint32_t apple1_read_startup_trace(const apple1_t* sys, const apple1_trace_event_t** out_events);

// Snapshot helpers
uint32_t apple1_save_snapshot(apple1_t* sys, apple1_t* dst);
bool apple1_load_snapshot(apple1_t* sys, uint32_t version, apple1_t* src);

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

static void _apple1_init_memorymap(apple1_t* sys) {
    mem_init(&sys->mem);

    // Map the whole 64K backing store as RAM. We protect ROM writes manually.
    mem_map_ram(&sys->mem, 0, 0x0000, APPLE1_MEM_SIZE, sys->ram);
}

static inline void _apple1_capture_trace(apple1_t* sys, uint16_t addr, uint8_t data, bool rw) {
    if (sys->startup_trace_complete) {
        return;
    }

    if (sys->startup_trace_len < APPLE1_TRACE_COUNT) {
        apple1_trace_event_t* ev = &sys->startup_trace[sys->startup_trace_len++];
        ev->addr = addr;
        ev->data = data;
        ev->rw = rw;

        if (sys->startup_trace_len == APPLE1_TRACE_COUNT) {
            sys->startup_trace_complete = true;
        }
    }
}

static inline uint8_t _apple1_mem_read(apple1_t* sys, uint16_t addr) {
    switch (addr) {
        case APPLE1_IO_KBD:
            // If bit 2 is clear, access the DDR. Otherwise access the peripheral register.
            if ((sys->kbd_cr & 0x04) == 0) {
                return sys->kbd_ddr;
            } else {
                uint8_t v = sys->kbd_latched;
                sys->kbd_latched = 0;   // consume the key on read
                sys->kbd_data = 0;
                return v;
            }

        case APPLE1_IO_KBDCR:
            // Preserve the programmed control bits, but report key-ready in bit 7.
            return (sys->kbd_cr & 0x7F) | ((sys->kbd_latched != 0) ? 0x80 : 0x00);

        case APPLE1_IO_DSP:
            // If bit 2 is clear, access the DDR. Otherwise access the peripheral register.
            if ((sys->dsp_cr & 0x04) == 0) {
                return sys->dsp_ddr;
            } else {
                // WozMon polls the display interface using BIT on $D012.
                // Returning 0x00 makes N=0 so BMI falls through as “display ready”.
                return 0x00;
            }

        case APPLE1_IO_DSPCR:
            // Minimal model: preserve the programmed control bits and report ready in bit 7.
            return (sys->dsp_cr & 0x7F) | 0x80;

        default:
            return mem_rd(&sys->mem, addr);
    }
}

static inline void _apple1_mem_write(apple1_t* sys, uint16_t addr, uint8_t data) {
    switch (addr) {
        case APPLE1_IO_KBD:
            if ((sys->kbd_cr & 0x04) == 0) {
                sys->kbd_ddr = data;
            } else {
                sys->kbd_data = data;
            }
            break;

        case APPLE1_IO_KBDCR:
            sys->kbd_cr = data;
            break;

        case APPLE1_IO_DSP:
            if ((sys->dsp_cr & 0x04) == 0) {
                sys->dsp_ddr = data;
            } else {
                sys->dsp_data = data;
                if (sys->char_out) {
                    sys->char_out(data, sys->char_out_user_data);
                }
            }
            break;

        case APPLE1_IO_DSPCR:
            sys->dsp_cr = data;
            break;

        default:
            // Protect the ROM region. Neo 1 ROM is copied into $E000-$FFFF.
            if (addr < APPLE1_ROM_BASE) {
                mem_wr(&sys->mem, addr, data);
            }
            break;
    }
}

static inline void _apple1_mem_rw(apple1_t* sys, uint16_t addr, bool rw) {
    if (rw) {
        uint8_t data = _apple1_mem_read(sys, addr);
        MOS6502CPU_SET_DATA(&sys->cpu, data);
        _apple1_capture_trace(sys, addr, data, true);
    } else {
        uint8_t data = MOS6502CPU_GET_DATA(&sys->cpu);
        _apple1_mem_write(sys, addr, data);
        _apple1_capture_trace(sys, addr, data, false);
    }
}

// -----------------------------------------------------------------------------
// public implementation
// -----------------------------------------------------------------------------

void apple1_init(apple1_t* sys, const apple1_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(*sys));
    sys->valid = true;
    sys->debug = desc->debug;

    CHIPS_ASSERT(desc->roms.rom.ptr);
    CHIPS_ASSERT(desc->roms.rom.size >= APPLE1_ROM_SIZE);

    sys->rom = desc->roms.rom.ptr;
    sys->char_out = desc->char_out.func;
    sys->char_out_user_data = desc->char_out.user_data;

    // Initialize hardware CPU glue / CPU abstraction.
    // On Neo6502, the second argument is ignored by the macro in wdc65C02cpu.h.
    MOS6502CPU_INIT(&sys->cpu, 0);

    // Build flat memory map and preload memory.
    _apple1_init_memorymap(sys);

    // Fill memory with a predictable Apple-like pattern:
    // even bytes 00, odd bytes FF. This mirrors how apple2.h seeds RAM.
    for (uint32_t addr = 0; addr < APPLE1_MEM_SIZE; addr += 2) {
        sys->ram[addr] = 0x00;
        sys->ram[addr + 1] = 0xFF;
    }

    // Copy the 8 KB Neo 1 top ROM image to $E000-$FFFF. This ROM should
    // already contain NMI/RESET/IRQ vectors in its last 6 bytes.
    memcpy(&sys->ram[APPLE1_ROM_BASE], sys->rom, APPLE1_ROM_SIZE);

    printf("[apple1] mem E000=%02X E001=%02X F000=%02X F001=%02X FFFA=%02X FFFB=%02X FFFC=%02X FFFD=%02X FFFE=%02X FFFF=%02X\n",
        sys->ram[0xE000], sys->ram[0xE001],
        sys->ram[0xF000], sys->ram[0xF001],
        sys->ram[0xFFFA], sys->ram[0xFFFB],
        sys->ram[0xFFFC], sys->ram[0xFFFD],
        sys->ram[0xFFFE], sys->ram[0xFFFF]);
        
    // Default no key pending and reset the minimal PIA-like state.
    sys->kbd_latched = 0;
    sys->kbd_cr = 0x00;
    sys->dsp_cr = 0x00;
    sys->kbd_ddr = 0x00;
    sys->dsp_ddr = 0x00;
    sys->kbd_data = 0x00;
    sys->dsp_data = 0x00;

    // Clear startup trace.
    sys->startup_trace_len = 0;
    sys->startup_trace_complete = false;

    sys->system_ticks = 0;
}

void apple1_discard(apple1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void apple1_reset(apple1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    sys->kbd_latched = 0;
    sys->kbd_cr = 0x00;
    sys->dsp_cr = 0x00;
    sys->kbd_ddr = 0x00;
    sys->dsp_ddr = 0x00;
    sys->kbd_data = 0x00;
    sys->dsp_data = 0x00;
    sys->startup_trace_len = 0;
    sys->startup_trace_complete = false;
    sys->system_ticks = 0;

    MOS6502CPU_SET_IRQ(&sys->cpu, false);

    // Experimental explicit reset control. This only affects the real 65C02
    // if GPIO 26 is mechanically connected to the CPU RESET line.
    MOS6502CPU_SET_RESET(&sys->cpu, true);
    sleep_us(1000);
    MOS6502CPU_SET_RESET(&sys->cpu, false);
}

void apple1_tick(apple1_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);

    // This ordering intentionally matches the working Apple II implementation:
    //   1) tick the real CPU / hardware glue
    //   2) service memory using captured addr/rw
    MOS6502CPU_TICK(&sys->cpu);
    _apple1_mem_rw(sys, sys->cpu.addr, sys->cpu.rw);

    sys->system_ticks++;
}

uint32_t apple1_exec(apple1_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);

    uint32_t num_ticks = clk_us_to_ticks(APPLE1_FREQUENCY, micro_seconds);

    if (0 == sys->debug.callback.func) {
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            apple1_tick(sys);
        }
    } else {
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            apple1_tick(sys);
            sys->debug.callback.func(sys->debug.callback.user_data, 0);
        }
    }

    return num_ticks;
}

void apple1_key_down(apple1_t* sys, uint8_t ascii) {
    CHIPS_ASSERT(sys && sys->valid);

    // Apple-1 monitor convention: set bit 7 on incoming key.
    // Translate LF to CR for convenience.
    if (ascii == '\n') {
        ascii = '\r';
    }

    // Only latch if no key is pending.
    if (sys->kbd_latched == 0) {
        sys->kbd_latched = ascii | 0x80;
    }
}

uint32_t apple1_read_startup_trace(const apple1_t* sys, const apple1_trace_event_t** out_events) {
    CHIPS_ASSERT(sys);
    if (out_events) {
        *out_events = sys->startup_trace;
    }
    return sys->startup_trace_len;
}

uint32_t apple1_save_snapshot(apple1_t* sys, apple1_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    mem_snapshot_onsave(&dst->mem, sys);
    return APPLE1_SNAPSHOT_VERSION;
}

bool apple1_load_snapshot(apple1_t* sys, uint32_t version, apple1_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != APPLE1_SNAPSHOT_VERSION) {
        return false;
    }

    static apple1_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

#endif // CHIPS_IMPL