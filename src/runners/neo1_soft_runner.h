#pragma once

// Software 65C02 execution runner for the CPU-neutral Neo1 machine.
//
// The checked-in fake65c02 core keeps its architectural state and callback
// hooks in process-global storage. This runner therefore supports exactly one
// active instance. That limitation is explicit here rather than leaking into
// neo1_machine_t, and can be removed when the software CPU dependency changes.

#include <stdbool.h>
#include <stdint.h>

#include "systems/neo1_machine.h"

#define NEO1_SOFT_RUNNER_FREQUENCY_HZ (1021800u)

typedef struct {
    neo1_machine_t* machine;
    uint32_t system_cycles;
    bool irq;
    bool valid;
} neo1_soft_runner_t;

// Attach the one active software CPU to a separately initialized machine,
// install the SDL BRK-recovery jump at $0000-$0002, and fetch RESET vector.
// Returns false when arguments are invalid or another runner is active.
bool neo1_soft_runner_init(neo1_soft_runner_t* runner, neo1_machine_t* machine);

void neo1_soft_runner_discard(neo1_soft_runner_t* runner);

// Reset CPU state and represented-cycle accounting. Machine RAM and device
// state are deliberately not reset by the CPU runner.
void neo1_soft_runner_reset(neo1_soft_runner_t* runner);

// Execute one complete instruction and return its represented cycle count.
uint32_t neo1_soft_runner_step(neo1_soft_runner_t* runner);

// Execute complete instructions until at least the requested time budget is
// represented. The final instruction may overshoot the exact cycle target.
uint32_t neo1_soft_runner_exec_us(neo1_soft_runner_t* runner, uint32_t microseconds);

void neo1_soft_runner_set_irq(neo1_soft_runner_t* runner, bool asserted);
void neo1_soft_runner_nmi(neo1_soft_runner_t* runner);
