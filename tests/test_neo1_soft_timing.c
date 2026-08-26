#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHIPS_IMPL

#include "chips/chips_common.h"
#include "chips/neo1_cpu_backend.h"
#include "chips/clk.h"
#include "systems/neo1.h"

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

    const neo1_desc_t desc = {
        .roms.rom = {
            .ptr = rom,
            .size = sizeof(rom),
        },
    };
    neo1_t machine;
    neo1_init(&machine, &desc);
    neo1_reset(&machine);

    CHECK(neo1_tick(&machine) == 2);
    CHECK(machine.system_ticks == 2);

    neo1_reset(&machine);
    const uint32_t exact_cycles = neo1_exec(&machine, 100);
    CHECK(exact_cycles == 102);
    CHECK(machine.system_ticks == exact_cycles);

    neo1_reset(&machine);
    const uint32_t overshoot_cycles = neo1_exec(&machine, 101);
    CHECK(overshoot_cycles == 104);
    CHECK(machine.system_ticks == overshoot_cycles);

    neo1_discard(&machine);
    if (g_failures != 0) {
        fprintf(stderr, "neo1_soft_timing_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts("neo1_soft_timing_tests: instruction cycles govern execution budget");
    return 0;
}
