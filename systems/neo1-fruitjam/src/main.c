// Fruit Jam serial-only Neo1 runner.
//
// The RP2350 owns lifecycle, elapsed-time scheduling, and USB-CDC diagnostics.
// The shared machine owns the Apple-1 address space and the software runner
// owns qe6502 execution. Video, Apple-1 keyboard input, storage, VACI, VCFFA1,
// audio, and the Neo1-50 Pico entry stubs are deliberately absent.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "runners/neo1_soft_runner.h"

#ifndef ADAFRUIT_FRUIT_JAM
#error "Neo1 Fruit Jam must be built for PICO_BOARD=adafruit_fruit_jam"
#endif

enum {
    NEO1_FRUITJAM_ENUMERATION_DELAY_MS = 1500,
    NEO1_FRUITJAM_MAX_CATCH_UP_US = 100000,
    NEO1_FRUITJAM_IDLE_SLEEP_US = 100,
    NEO1_FRUITJAM_CTRL_R = 0x12,
};

// Keep the 64 KB machine out of the RP2350's small default C stack.
static neo1_machine_t machine;
static neo1_soft_runner_t cpu;

static void neo1_fruitjam_char_out(uint8_t ch, void* user_data) {
    (void)user_data;
    putchar((int)(ch & 0x7Fu));
}

static void neo1_fruitjam_print_entry(const char* reason) {
    const uint16_t vector =
        (uint16_t)machine.ram[0xFFFC] |
        ((uint16_t)machine.ram[0xFFFD] << 8);

    printf("\n[neo1-fruitjam] %s personality=%u reset=$%04X entry=$%04X\n",
           reason,
           (unsigned)machine.profile->personality,
           (unsigned)vector,
           (unsigned)cpu.tick.address);
    printf("[neo1-fruitjam] serial Ctrl-R resets; video/Apple-1 input/storage disabled\n");
}

static void neo1_fruitjam_reset(void) {
    neo1_machine_reset(&machine);
    neo1_soft_runner_reset(&cpu);
    neo1_fruitjam_print_entry("reset");
}

int main(void) {
    stdio_init_all();
    sleep_ms(NEO1_FRUITJAM_ENUMERATION_DELAY_MS);

    const neo1_profile_t* profile = neo1_profile_find(NEO1_PERSONALITY);
    if (!profile) {
        printf("[neo1-fruitjam] unsupported personality=%u\n",
               (unsigned)NEO1_PERSONALITY);
        return 1;
    }

    const neo1_machine_desc_t desc = {
        .profile = profile,
        .char_out = neo1_fruitjam_char_out,
    };
    if (!neo1_machine_init(&machine, &desc)) {
        printf("[neo1-fruitjam] machine initialization failed\n");
        return 1;
    }
    if (!neo1_soft_runner_init(&cpu, &machine)) {
        printf("[neo1-fruitjam] CPU initialization failed\n");
        return 1;
    }

    neo1_fruitjam_print_entry("ready");
    uint64_t previous_time_us = time_us_64();

    while (true) {
        const int input = getchar_timeout_us(0);
        if (input == NEO1_FRUITJAM_CTRL_R) {
            neo1_fruitjam_reset();
            previous_time_us = time_us_64();
            continue;
        }

        const uint64_t current_time_us = time_us_64();
        uint64_t elapsed_us = current_time_us - previous_time_us;
        previous_time_us = current_time_us;
        if (elapsed_us > NEO1_FRUITJAM_MAX_CATCH_UP_US) {
            elapsed_us = NEO1_FRUITJAM_MAX_CATCH_UP_US;
        }
        if (elapsed_us > 0) {
            (void)neo1_soft_runner_exec_us(&cpu, (uint32_t)elapsed_us);
        }

        sleep_us(NEO1_FRUITJAM_IDLE_SLEEP_US);
    }
}
