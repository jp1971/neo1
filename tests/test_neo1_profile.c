#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "systems/neo1_machine.h"
#include "systems/neo1_profile.h"

static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static void test_profile(unsigned personality,
                         size_t expected_size,
                         uint16_t expected_base,
                         uint8_t first_byte) {
    const neo1_profile_t* profile = neo1_profile_find(personality);
    CHECK(profile != NULL);
    if (!profile) {
        return;
    }

    CHECK(profile->personality == personality);
    CHECK(profile->rom_size == expected_size);
    CHECK(profile->rom_base == expected_base);
    CHECK(profile->rom_protect_base == expected_base);
    CHECK(profile->rom[0] == first_byte);
    CHECK(profile->rom[profile->rom_size - 4] == 0x00);
    CHECK(profile->rom[profile->rom_size - 3] == 0xFF);

    const neo1_machine_desc_t desc = {.profile = profile};
    neo1_machine_t machine;
    CHECK(neo1_machine_init(&machine, &desc));
    CHECK(machine.profile == profile);
    CHECK(machine.ram[expected_base] == first_byte);
    CHECK(machine.ram[0xFFFC] == 0x00);
    CHECK(machine.ram[0xFFFD] == 0xFF);

    const uint16_t writable_addr = (uint16_t)(expected_base - 1u);
    const uint8_t protected_before = machine.ram[expected_base];
    neo1_machine_write(&machine, writable_addr, 0x35);
    neo1_machine_write(&machine, expected_base, 0x53);
    CHECK(machine.ram[writable_addr] == 0x35);
    CHECK(machine.ram[expected_base] == protected_before);
}

static void test_invalid_profiles(void) {
    CHECK(neo1_profile_find(0) == NULL);
    CHECK(neo1_profile_find(49) == NULL);
    CHECK(neo1_profile_find(51) == NULL);

    neo1_machine_t machine;
    memset(&machine, 0xA5, sizeof(machine));
    const neo1_machine_desc_t missing = {0};
    CHECK(!neo1_machine_init(&machine, &missing));
    CHECK(machine.ram[0] == 0xA5);

    static const uint8_t rom[] = {0xEA};
    const neo1_profile_t invalid = {
        .personality = 99,
        .rom = rom,
        .rom_size = sizeof(rom),
        .rom_base = 0xE000,
        .rom_protect_base = 0xF000,
    };
    const neo1_machine_desc_t invalid_desc = {.profile = &invalid};
    CHECK(!neo1_machine_init(&machine, &invalid_desc));
    CHECK(machine.ram[0] == 0xA5);
}

int main(void) {
    test_profile(NEO1_PERSONALITY_23, 0x2000, 0xE000, 0x4C);
    test_profile(NEO1_PERSONALITY_50, 0x0100, 0xFF00, 0xD8);
    test_invalid_profiles();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_profile_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts("neo1_profile_tests: shared Neo1-23/Neo1-50 layouts passed");
    return 0;
}
