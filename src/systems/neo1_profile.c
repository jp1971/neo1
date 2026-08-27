#include "systems/neo1_profile.h"

#include "roms/neo1_roms.h"

static const neo1_profile_t neo1_profile_23 = {
    .personality = NEO1_PERSONALITY_23,
    .rom = neo1_system_rom_bin,
    .rom_size = sizeof(neo1_system_rom_bin),
    .rom_base = 0xE000,
    .rom_protect_base = 0xE000,
};

static const neo1_profile_t neo1_profile_50 = {
    .personality = NEO1_PERSONALITY_50,
    .rom = neo1_apple1_rom_bin,
    .rom_size = sizeof(neo1_apple1_rom_bin),
    .rom_base = 0xFF00,
    .rom_protect_base = 0xFF00,
};

const neo1_profile_t* neo1_profile_find(unsigned personality) {
    switch (personality) {
        case NEO1_PERSONALITY_23:
            return &neo1_profile_23;
        case NEO1_PERSONALITY_50:
            return &neo1_profile_50;
        default:
            return NULL;
    }
}
