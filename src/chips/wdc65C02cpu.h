#pragma once

// wdc65C02cpu.h
//
// RP2040 adapter for the physical W65C02 and Neo6502 bus latches. GPIO 0-7 are
// the shared byte-wide path. Active-low OE1 and OE2 expose the low and high
// address bytes; active-low OE3 transfers data. R/W is sampled from the CPU,
// while PHI2, RESET, IRQ, and NMI are driven by the RP2040.
//
// wdc65C02cpu_tick() lowers PHI2, captures address and R/W through the latches,
// and raises PHI2. The Neo1 machine then services that captured access: CPU
// writes are read through OE3, and CPU reads are answered by driving GPIO 0-7
// and pulsing OE3. The latch order and inline delays are timing-sensitive,
// hardware-verified behavior and must not be reordered without a reset/startup
// bus-trace comparison.
//
// RESET, IRQ, and NMI are active-low at the pins. Boolean setter state `true`
// means asserted. GPIO 26 affects the CPU only when the Neo6502 reset-control
// connection is physically present.
//
// ## zlib/libpng license
//
// Copyright (c) 2023 Veselin Sladkov
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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t addr;
    bool rw;  // sampled CPU R/W: true = read, false = write
} wdc6502cpu_t;

#define MOS6502CPU_T                 wdc6502cpu_t
#define MOS6502CPU_INIT(c, desc)     wdc65C02cpu_init()
#define MOS6502CPU_RESET(c)          wdc65C02cpu_reset()
#define MOS6502CPU_NMI(c)            wdc65C02cpu_nmi()
#define MOS6502CPU_TICK(c)           wdc65C02cpu_tick(c)
#define MOS6502CPU_GET_ADDR(c)       wdc65C02cpu_get_addr()
#define MOS6502CPU_GET_DATA(c)       wdc65C02cpu_get_data()
#define MOS6502CPU_SET_DATA(c, data) wdc65C02cpu_set_data(data)
#define MOS6502CPU_SET_IRQ(c, state) wdc65C02cpu_set_irq(state)
#define MOS6502CPU_SET_RESET(c, state) wdc65C02cpu_set_reset(state)

// Configure the bus GPIO/latches and issue a 1 ms active-low RESET pulse.
void wdc65C02cpu_init();
// Issue the same 1 ms RESET pulse after initialization.
void wdc65C02cpu_reset();
// Assert or release the active-low RESET output.
void wdc65C02cpu_set_reset(bool state);

// Pulse active-low NMI for 1 ms.
void wdc65C02cpu_nmi();

// Advance one physical clock transition pair and capture address/R/W.
void wdc65C02cpu_tick(wdc6502cpu_t* c);

// Read the address bus through the low-byte and high-byte latches, in order.
uint16_t wdc65C02cpu_get_addr();

// Sample CPU write data through the data latch.
uint8_t wdc65C02cpu_get_data();

// Drive CPU read data through the data latch.
void wdc65C02cpu_set_data(uint8_t data);

// Assert or release the active-low IRQ output.
void wdc65C02cpu_set_irq(bool state);

#ifdef __cplusplus
}  // extern "C"
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
#include <assert.h>
#define CHIPS_ASSERT(c) assert(c)
#endif

#define _GPIO_MASK       (0xFF)
#define _GPIO_SHIFT_BITS (0)
#define _OE1_PIN         (8)
#define _OE2_PIN         (9)
#define _OE3_PIN         (10)
#define _RW_PIN          (11)
#define _CLOCK_PIN       (21)
#define _RESET_PIN       (26)
#define _IRQ_PIN         (25)
#define _NMI_PIN         (27)

void wdc65C02cpu_init() {
    gpio_init_mask(_GPIO_MASK);

    // Keep every latch disabled before configuring the remaining bus signals.
    gpio_init(_OE1_PIN);
    gpio_set_dir(_OE1_PIN, GPIO_OUT);
    gpio_put(_OE1_PIN, 1);

    gpio_init(_OE2_PIN);
    gpio_set_dir(_OE2_PIN, GPIO_OUT);
    gpio_put(_OE2_PIN, 1);

    gpio_init(_OE3_PIN);
    gpio_set_dir(_OE3_PIN, GPIO_OUT);
    gpio_put(_OE3_PIN, 1);

    gpio_init(_RW_PIN);
    gpio_set_dir(_RW_PIN, GPIO_IN);

    gpio_init(_CLOCK_PIN);
    gpio_set_dir(_CLOCK_PIN, GPIO_OUT);
    gpio_put(_CLOCK_PIN, 1);

    gpio_init(_RESET_PIN);
    gpio_set_dir(_RESET_PIN, GPIO_OUT);
    gpio_put(_RESET_PIN, 1);

    gpio_init(_IRQ_PIN);
    gpio_set_dir(_IRQ_PIN, GPIO_OUT);
    gpio_put(_IRQ_PIN, 1);

    gpio_init(_NMI_PIN);
    gpio_set_dir(_NMI_PIN, GPIO_OUT);
    gpio_put(_NMI_PIN, 1);

    gpio_put(_RESET_PIN, 0);
    sleep_us(1000);
    gpio_put(_RESET_PIN, 1);
}

void wdc65C02cpu_set_reset(bool state) {
    gpio_put(_RESET_PIN, state ? 0 : 1);
}

void wdc65C02cpu_reset() {
    wdc65C02cpu_set_reset(true);
    sleep_us(1000);
    wdc65C02cpu_set_reset(false);
}

void wdc65C02cpu_nmi() {
    gpio_put(_NMI_PIN, 0);
    sleep_us(1000);
    gpio_put(_NMI_PIN, 1);
}

void wdc65C02cpu_tick(wdc6502cpu_t* c) {
    // Address and R/W are sampled during the low phase. The shared machine
    // services the captured access after this function raises PHI2.
    gpio_put(_CLOCK_PIN, 0);

    c->addr = wdc65C02cpu_get_addr();
    c->rw = gpio_get(_RW_PIN);

    gpio_put(_CLOCK_PIN, 1);
}

uint16_t wdc65C02cpu_get_addr() {
    // The shared GPIO byte must be input while either address latch drives it.
    gpio_set_dir_masked(_GPIO_MASK, 0);

    gpio_put(_OE1_PIN, 0);
    // Allow the external latch output to settle before sampling GPIO.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    uint16_t addr = (gpio_get_all() >> _GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_OE1_PIN, 1);

    gpio_put(_OE2_PIN, 0);
    // Preserve the independently verified high-address latch delay.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    addr |= (gpio_get_all() << (8 - _GPIO_SHIFT_BITS)) & 0xFF00;
    gpio_put(_OE2_PIN, 1);

    return addr;
}

uint8_t wdc65C02cpu_get_data() {
    // CPU write data is sampled with the shared GPIO byte in input mode.
    gpio_set_dir_masked(_GPIO_MASK, 0);

    gpio_put(_OE3_PIN, 0);
    // Allow the data latch output to settle before sampling GPIO.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    uint8_t data = (gpio_get_all() >> _GPIO_SHIFT_BITS) & 0xFF;
    gpio_put(_OE3_PIN, 1);

    return data;
}

void wdc65C02cpu_set_data(uint8_t data) {
    // CPU read data is driven only for the short active-low OE3 pulse.
    gpio_set_dir_masked(_GPIO_MASK, _GPIO_MASK);

    gpio_put_masked(_GPIO_MASK, data << _GPIO_SHIFT_BITS);
    gpio_put(_OE3_PIN, 0);
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    gpio_put(_OE3_PIN, 1);

}

void wdc65C02cpu_set_irq(bool state) { gpio_put(_IRQ_PIN, state ? 0 : 1); }

#endif  // CHIPS_IMPL
