#pragma once

// Physical W65C02 runner for the Olimex Neo6502.
//
// The runner owns RP2040 GPIO/latch timing and reset/interrupt pins. Each
// captured W65C02 bus cycle is forwarded to an explicitly attached
// CPU-neutral Neo1 machine.

#include <stdbool.h>
#include <stdint.h>

#include "systems/neo1_machine.h"

#define NEO1_WDC_TRACE_COUNT (64u)

typedef struct {
    uint16_t addr;
    uint8_t data;
    bool rw;  // sampled CPU R/W: true = read, false = write
} neo1_wdc_trace_event_t;

typedef struct {
    neo1_machine_t* machine;
    bool valid;
    neo1_wdc_trace_event_t startup_trace[NEO1_WDC_TRACE_COUNT];
    uint32_t startup_trace_len;
    bool startup_trace_complete;
    uint32_t system_cycles;
} neo1_wdc_runner_t;

// Configure the physical bus and issue its initial clock-qualified RESET pulse.
bool neo1_wdc_runner_init(neo1_wdc_runner_t* runner, neo1_machine_t* machine);

// Clear runner counters/trace, release IRQ, and issue a clock-qualified RESET.
// Machine-visible device reset remains an explicit caller responsibility.
void neo1_wdc_runner_reset(neo1_wdc_runner_t* runner);

// Advance PHI2 through one captured bus cycle and service it through machine.
uint32_t neo1_wdc_runner_tick(neo1_wdc_runner_t* runner);

void neo1_wdc_runner_set_irq(neo1_wdc_runner_t* runner, bool asserted);
void neo1_wdc_runner_nmi(neo1_wdc_runner_t* runner);

uint32_t neo1_wdc_runner_read_startup_trace(
    const neo1_wdc_runner_t* runner,
    const neo1_wdc_trace_event_t** out_events);
