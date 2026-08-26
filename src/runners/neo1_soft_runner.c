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

static void neo1_soft_runner_install_brk_recovery(neo1_machine_t* machine) {
    const uint16_t reset_vector =
        (uint16_t)machine->ram[0xFFFC] | ((uint16_t)machine->ram[0xFFFD] << 8);
    machine->ram[0x0000] = 0x4C; // JMP abs
    machine->ram[0x0001] = (uint8_t)(reset_vector & 0xFF);
    machine->ram[0x0002] = (uint8_t)(reset_vector >> 8);
}

bool neo1_soft_runner_init(neo1_soft_runner_t* runner, neo1_machine_t* machine) {
    if (!runner || !machine || (active_runner && active_runner->valid)) {
        return false;
    }

    memset(runner, 0, sizeof(*runner));
    runner->machine = machine;
    runner->valid = true;
    active_runner = runner;

    neo1_soft_runner_install_brk_recovery(machine);
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
    runner->system_cycles = 0;
    reset6502();
}

uint32_t neo1_soft_runner_step(neo1_soft_runner_t* runner) {
    assert(runner && runner->valid && (active_runner == runner));
    if (runner->irq) {
        irq6502();
    }
    const uint32_t cycles = step6502();
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
    (void)runner;
    nmi6502();
}
