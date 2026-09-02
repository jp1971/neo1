#include "runners/neo1_soft_runner.h"

#include <assert.h>
#include <string.h>

static neo1_soft_runner_t* active_runner;

// Global callback names required by the checked-in fake65c02 core.
unsigned char read6502(unsigned short address) {
    assert(active_runner && active_runner->valid && active_runner->machine);
    return neo1_machine_read(active_runner->machine, (uint16_t)address);
}

void write6502(unsigned short address, unsigned char value) {
    assert(active_runner && active_runner->valid && active_runner->machine);
    neo1_machine_write(active_runner->machine, (uint16_t)address, (uint8_t)value);
}

#include "chips/fake65c02.h"

bool neo1_soft_runner_init(neo1_soft_runner_t* runner, neo1_machine_t* machine) {
    if (!runner || !machine || (active_runner && active_runner->valid)) {
        return false;
    }

    memset(runner, 0, sizeof(*runner));
    runner->machine = machine;
    runner->valid = true;
    active_runner = runner;

    reset6502();
    return true;
}

void neo1_soft_runner_discard(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid && (active_runner == runner));
    runner->valid = false;
    runner->machine = NULL;
    active_runner = NULL;
}

void neo1_soft_runner_reset(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid && (active_runner == runner));
    runner->irq = false;
    runner->nmi_pending = false;
    runner->system_cycles = 0;
    reset6502();
}

uint32_t neo1_soft_runner_step(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid && (active_runner == runner));

    uint32_t interrupt_cycles = 0;
    if (runner->nmi_pending) {
        runner->nmi_pending = false;
        nmi6502();
        interrupt_cycles = 7;
    } else if (runner->irq && ((status & FLAG_INTERRUPT) == 0)) {
        // fake65c02 exposes no accepted/not-accepted result, so inspect its
        // internal I flag before presenting the level. Keep that dependency
        // isolated here until the provisional CPU core is replaced.
        irq6502();
        interrupt_cycles = 7;
    }

    const uint32_t cycles = interrupt_cycles + step6502();
    assert(cycles > 0);
    runner->system_cycles += cycles;
    return cycles;
}

uint32_t neo1_soft_runner_exec_us(neo1_soft_runner_t* runner, uint32_t microseconds) {
    assert(runner && runner->valid && (active_runner == runner));
    const uint32_t requested_cycles = (uint32_t)(
        ((uint64_t)NEO1_SOFT_RUNNER_FREQUENCY_HZ * microseconds) / 1000000u);
    uint32_t executed_cycles = 0;
    while (executed_cycles < requested_cycles) {
        executed_cycles += neo1_soft_runner_step(runner);
    }
    return executed_cycles;
}

void neo1_soft_runner_set_irq(neo1_soft_runner_t* runner, bool asserted) {
    assert(runner && runner->valid && (active_runner == runner));
    runner->irq = asserted;
}

void neo1_soft_runner_nmi(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid && (active_runner == runner));
    runner->nmi_pending = true;
}
