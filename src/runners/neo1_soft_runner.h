#pragma once

// Software 65C02 execution runner for the CPU-neutral Neo1 machine.
//
// Each runner owns one qe6502 WDC65C02 value and services its explicit bus
// requests through a separately owned neo1_machine. CPU state remains outside
// neo1_machine_t and multiple runners may execute independently.

#include <stdbool.h>
#include <stdint.h>

#include <qe6502/qe6502.h>

#include "systems/neo1_machine.h"

#define NEO1_SOFT_RUNNER_FREQUENCY_HZ (1021800u)

typedef struct {
    neo1_machine_t* machine;
    qe6502_t cpu;
    qe6502_tick_t tick;
    uint32_t system_cycles;
    bool nmi_pending;
    bool valid;
} neo1_soft_runner_t;

// Attach an instance-owned software CPU to a separately initialized machine
// and complete its RESET sequence through explicit machine reads. The runner
// never initializes or patches machine RAM.
bool neo1_soft_runner_init(neo1_soft_runner_t* runner, neo1_machine_t* machine);

void neo1_soft_runner_discard(neo1_soft_runner_t* runner);

// Reset CPU state and represented-cycle accounting. Machine RAM and device
// state are deliberately not reset by the CPU runner.
void neo1_soft_runner_reset(neo1_soft_runner_t* runner);

// Execute one complete instruction and return its represented bus-cycle count.
// For compatibility with the established runner contract, an accepted IRQ or
// pending NMI includes its entry sequence and the first handler instruction.
uint32_t neo1_soft_runner_step(neo1_soft_runner_t* runner);

// Execute complete instructions until at least the requested time budget is
// represented. The final instruction may overshoot the exact cycle target.
uint32_t neo1_soft_runner_exec_us(neo1_soft_runner_t* runner, uint32_t microseconds);

void neo1_soft_runner_set_irq(neo1_soft_runner_t* runner, bool asserted);

// Latch one NMI edge for delivery before the next instruction. Multiple edges
// before that boundary collapse into one pending NMI, matching a CPU input
// latch rather than an unbounded host-event queue.
void neo1_soft_runner_nmi(neo1_soft_runner_t* runner);
