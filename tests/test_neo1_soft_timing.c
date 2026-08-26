#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "runners/neo1_soft_runner.h"

static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

int main(void) {
    uint8_t rom[0x100];
    memset(rom, 0xEA, sizeof(rom));  // two-cycle NOP stream at $FF00
    rom[0xFC] = 0x00;
    rom[0xFD] = 0xFF;

    const neo1_machine_desc_t desc = {
        .rom = rom,
        .rom_size = sizeof(rom),
        .rom_base = 0xFF00,
        .rom_protect_base = 0xFF00,
    };
    neo1_machine_t machine;
    neo1_soft_runner_t runner;
    neo1_soft_runner_t second_runner;
    CHECK(neo1_machine_init(&machine, &desc));
    CHECK(machine.ram[0x0000] == 0x00);
    CHECK(machine.ram[0x0001] == 0xFF);
    CHECK(machine.ram[0x0002] == 0x00);
    CHECK(neo1_soft_runner_init(&runner, &machine));
    CHECK(!neo1_soft_runner_init(&second_runner, &machine));
    CHECK(machine.ram[0x0000] == 0x4C);
    CHECK(machine.ram[0x0001] == 0x00);
    CHECK(machine.ram[0x0002] == 0xFF);

    CHECK(neo1_soft_runner_step(&runner) == 2);
    CHECK(runner.system_cycles == 2);

    machine.ram[0x0300] = 0xA5;
    neo1_soft_runner_reset(&runner);
    CHECK(runner.system_cycles == 0);
    CHECK(machine.ram[0x0300] == 0xA5);
    const uint32_t exact_cycles = neo1_soft_runner_exec_us(&runner, 100);
    CHECK(exact_cycles == 102);
    CHECK(runner.system_cycles == exact_cycles);

    neo1_soft_runner_reset(&runner);
    const uint32_t overshoot_cycles = neo1_soft_runner_exec_us(&runner, 101);
    CHECK(overshoot_cycles == 104);
    CHECK(runner.system_cycles == overshoot_cycles);

    neo1_soft_runner_discard(&runner);
    if (g_failures != 0) {
        fprintf(stderr, "neo1_soft_timing_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts("neo1_soft_timing_tests: instruction cycles govern execution budget");
    return 0;
}
