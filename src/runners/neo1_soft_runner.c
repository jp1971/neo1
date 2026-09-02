#include "runners/neo1_soft_runner.h"

#include <assert.h>
#include <string.h>

static void neo1_soft_runner_service_tick(neo1_soft_runner_t* runner) {
    uint8_t input = 0;
    if (qe6502_is_write(runner->tick)) {
        neo1_machine_write(runner->machine, runner->tick.address,
                           runner->tick.bus);
    } else {
        input = neo1_machine_read(runner->machine, runner->tick.address);
    }
    runner->tick = qe6502_tick(&runner->cpu, input);
}

static void neo1_soft_runner_complete_reset(neo1_soft_runner_t* runner) {
    runner->cpu = qe6502_setup(qe6502_model_wdc);
    runner->tick = qe6502_restart(&runner->cpu);
    while (qe6502_is_reset(runner->tick) ||
           !qe6502_is_fetch(runner->tick)) {
        neo1_soft_runner_service_tick(runner);
    }

    // Preserve Neo1's deterministic software-runner reset state. Real W65C02
    // hardware does not specify these values, but SDL programs have
    // historically begun with A/X/Y=0 and S=$FD.
    qe6502_set_a(&runner->cpu, 0);
    qe6502_set_x(&runner->cpu, 0);
    qe6502_set_y(&runner->cpu, 0);
    qe6502_set_s(&runner->cpu, 0xFD);
    qe6502_set_p(&runner->cpu, qe6502_flag_I | qe6502_flag_UN);
}

bool neo1_soft_runner_init(neo1_soft_runner_t* runner, neo1_machine_t* machine) {
    if (!runner || !machine) {
        return false;
    }

    memset(runner, 0, sizeof(*runner));
    runner->machine = machine;
    runner->valid = true;
    neo1_soft_runner_complete_reset(runner);
    return true;
}

void neo1_soft_runner_discard(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid);
    runner->valid = false;
    runner->machine = NULL;
}

void neo1_soft_runner_reset(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid);
    runner->nmi_pending = false;
    runner->system_cycles = 0;
    neo1_soft_runner_complete_reset(runner);
}

uint32_t neo1_soft_runner_step(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid);
    assert(qe6502_is_fetch(runner->tick));

    bool nmi_entry = false;
    bool irq_entry = false;
    if (runner->nmi_pending) {
        runner->nmi_pending = false;
        qe6502_nmi_assert(&runner->cpu, 1);
        nmi_entry = true;
    } else if (qe6502_is_irq_asserted(&runner->cpu) &&
               !qe6502_get_flag_i(&runner->cpu)) {
        irq_entry = true;
    }

    // A newly presented interrupt is recognized after the instruction whose
    // opcode is already on the bus. Entry and the first handler instruction
    // therefore finish at the third fetch boundary.
    const unsigned fetches_to_complete = (irq_entry || nmi_entry) ? 3u : 1u;
    unsigned completed_fetches = 0;
    uint32_t cycles = 0;
    do {
        neo1_soft_runner_service_tick(runner);
        cycles++;
        if (nmi_entry && cycles == 1u) {
            qe6502_nmi_assert(&runner->cpu, 0);
        }
        if (qe6502_is_fetch(runner->tick)) {
            completed_fetches++;
        }
    } while (completed_fetches < fetches_to_complete);

    assert(cycles > 0);
    runner->system_cycles += cycles;
    return cycles;
}

uint32_t neo1_soft_runner_exec_us(neo1_soft_runner_t* runner, uint32_t microseconds) {
    assert(runner && runner->valid);
    const uint32_t requested_cycles = (uint32_t)(
        ((uint64_t)NEO1_SOFT_RUNNER_FREQUENCY_HZ * microseconds) / 1000000u);
    uint32_t executed_cycles = 0;
    while (executed_cycles < requested_cycles) {
        executed_cycles += neo1_soft_runner_step(runner);
    }
    return executed_cycles;
}

void neo1_soft_runner_set_irq(neo1_soft_runner_t* runner, bool asserted) {
    assert(runner && runner->valid);
    qe6502_irq_assert(&runner->cpu, asserted ? 1 : 0);
}

void neo1_soft_runner_nmi(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid);
    runner->nmi_pending = true;
}
