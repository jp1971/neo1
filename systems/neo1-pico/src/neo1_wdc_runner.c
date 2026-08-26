// neo1_wdc_runner.c
//
// RP2040 runner for the physical W65C02 and Neo6502 bus latches. GPIO 0-7 are
// the shared byte-wide path. Active-low OE1 and OE2 expose the low and high
// address bytes; active-low OE3 transfers data. R/W is sampled from the CPU,
// while PHI2, RESET, IRQ, and NMI are driven by the RP2040.
//
// The latch order and inline delays below are timing-sensitive,
// hardware-verified behavior. Do not reorder them without comparing the reset
// and startup bus trace on a physical Neo6502.
//
// Altered from wdc65C02cpu.h and the Neo1 transitional wrapper.
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

#include "neo1_wdc_runner.h"

#include <assert.h>
#include <string.h>

#include "pico/stdlib.h"

#define NEO1_WDC_GPIO_MASK       (0xFFu)
#define NEO1_WDC_GPIO_SHIFT_BITS (0u)
#define NEO1_WDC_OE1_PIN         (8u)
#define NEO1_WDC_OE2_PIN         (9u)
#define NEO1_WDC_OE3_PIN         (10u)
#define NEO1_WDC_RW_PIN          (11u)
#define NEO1_WDC_CLOCK_PIN       (21u)
#define NEO1_WDC_RESET_PIN       (26u)
#define NEO1_WDC_IRQ_PIN         (25u)
#define NEO1_WDC_NMI_PIN         (27u)

static void neo1_wdc_set_reset(bool asserted) {
    gpio_put(NEO1_WDC_RESET_PIN, asserted ? 0 : 1);
}

static uint16_t neo1_wdc_get_addr(void) {
    // The shared GPIO byte must be input while either address latch drives it.
    gpio_set_dir_masked(NEO1_WDC_GPIO_MASK, 0);

    gpio_put(NEO1_WDC_OE1_PIN, 0);
    // Allow the external latch output to settle before sampling GPIO.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    uint16_t addr =
        (gpio_get_all() >> NEO1_WDC_GPIO_SHIFT_BITS) & 0xFFu;
    gpio_put(NEO1_WDC_OE1_PIN, 1);

    gpio_put(NEO1_WDC_OE2_PIN, 0);
    // Preserve the independently verified high-address latch delay.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    addr |= (gpio_get_all() << (8u - NEO1_WDC_GPIO_SHIFT_BITS)) & 0xFF00u;
    gpio_put(NEO1_WDC_OE2_PIN, 1);

    return addr;
}

static uint8_t neo1_wdc_get_data(void) {
    // CPU write data is sampled with the shared GPIO byte in input mode.
    gpio_set_dir_masked(NEO1_WDC_GPIO_MASK, 0);

    gpio_put(NEO1_WDC_OE3_PIN, 0);
    // Allow the data latch output to settle before sampling GPIO.
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    const uint8_t data =
        (gpio_get_all() >> NEO1_WDC_GPIO_SHIFT_BITS) & 0xFFu;
    gpio_put(NEO1_WDC_OE3_PIN, 1);

    return data;
}

static void neo1_wdc_set_data(uint8_t data) {
    // CPU read data is driven only for the short active-low OE3 pulse.
    gpio_set_dir_masked(NEO1_WDC_GPIO_MASK, NEO1_WDC_GPIO_MASK);

    gpio_put_masked(NEO1_WDC_GPIO_MASK, data << NEO1_WDC_GPIO_SHIFT_BITS);
    gpio_put(NEO1_WDC_OE3_PIN, 0);
    __asm volatile("nop\n");
    __asm volatile("nop\n");
    gpio_put(NEO1_WDC_OE3_PIN, 1);
}

static void neo1_wdc_capture_trace(
    neo1_wdc_runner_t* runner,
    uint16_t addr,
    uint8_t data,
    bool rw) {
    if (runner->startup_trace_complete) {
        return;
    }

    neo1_wdc_trace_event_t* event =
        &runner->startup_trace[runner->startup_trace_len++];
    event->addr = addr;
    event->data = data;
    event->rw = rw;

    if (runner->startup_trace_len == NEO1_WDC_TRACE_COUNT) {
        runner->startup_trace_complete = true;
    }
}

bool neo1_wdc_runner_init(
    neo1_wdc_runner_t* runner,
    neo1_machine_t* machine) {
    if (!runner || !machine) {
        return false;
    }

    memset(runner, 0, sizeof(*runner));
    runner->machine = machine;
    runner->valid = true;

    gpio_init_mask(NEO1_WDC_GPIO_MASK);

    // Keep every latch disabled before configuring the remaining bus signals.
    gpio_init(NEO1_WDC_OE1_PIN);
    gpio_set_dir(NEO1_WDC_OE1_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_OE1_PIN, 1);

    gpio_init(NEO1_WDC_OE2_PIN);
    gpio_set_dir(NEO1_WDC_OE2_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_OE2_PIN, 1);

    gpio_init(NEO1_WDC_OE3_PIN);
    gpio_set_dir(NEO1_WDC_OE3_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_OE3_PIN, 1);

    gpio_init(NEO1_WDC_RW_PIN);
    gpio_set_dir(NEO1_WDC_RW_PIN, GPIO_IN);

    gpio_init(NEO1_WDC_CLOCK_PIN);
    gpio_set_dir(NEO1_WDC_CLOCK_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_CLOCK_PIN, 1);

    gpio_init(NEO1_WDC_RESET_PIN);
    gpio_set_dir(NEO1_WDC_RESET_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_RESET_PIN, 1);

    gpio_init(NEO1_WDC_IRQ_PIN);
    gpio_set_dir(NEO1_WDC_IRQ_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_IRQ_PIN, 1);

    gpio_init(NEO1_WDC_NMI_PIN);
    gpio_set_dir(NEO1_WDC_NMI_PIN, GPIO_OUT);
    gpio_put(NEO1_WDC_NMI_PIN, 1);

    neo1_wdc_set_reset(true);
    sleep_us(1000);
    neo1_wdc_set_reset(false);
    return true;
}

void neo1_wdc_runner_reset(neo1_wdc_runner_t* runner) {
    assert(runner && runner->valid);

    runner->startup_trace_len = 0;
    runner->startup_trace_complete = false;
    runner->system_cycles = 0;

    neo1_wdc_runner_set_irq(runner, false);
    neo1_wdc_set_reset(true);
    sleep_us(1000);
    neo1_wdc_set_reset(false);
}

uint32_t neo1_wdc_runner_tick(neo1_wdc_runner_t* runner) {
    assert(runner && runner->valid && runner->machine);

    // Address and R/W are sampled during the low phase. The machine services
    // the captured access only after PHI2 returns high.
    gpio_put(NEO1_WDC_CLOCK_PIN, 0);
    const uint16_t addr = neo1_wdc_get_addr();
    const bool rw = gpio_get(NEO1_WDC_RW_PIN);
    gpio_put(NEO1_WDC_CLOCK_PIN, 1);

    uint8_t data;
    if (rw) {
        data = neo1_machine_read(runner->machine, addr);
        neo1_wdc_set_data(data);
    } else {
        data = neo1_wdc_get_data();
        neo1_machine_write(runner->machine, addr, data);
    }
    neo1_wdc_capture_trace(runner, addr, data, rw);

    runner->system_cycles++;
    return 1;
}

void neo1_wdc_runner_set_irq(
    neo1_wdc_runner_t* runner,
    bool asserted) {
    assert(runner && runner->valid);
    gpio_put(NEO1_WDC_IRQ_PIN, asserted ? 0 : 1);
}

void neo1_wdc_runner_nmi(neo1_wdc_runner_t* runner) {
    assert(runner && runner->valid);
    gpio_put(NEO1_WDC_NMI_PIN, 0);
    sleep_us(1000);
    gpio_put(NEO1_WDC_NMI_PIN, 1);
}

uint32_t neo1_wdc_runner_read_startup_trace(
    const neo1_wdc_runner_t* runner,
    const neo1_wdc_trace_event_t** out_events) {
    assert(runner && runner->valid);
    if (out_events) {
        *out_events = runner->startup_trace;
    }
    return runner->startup_trace_len;
}
