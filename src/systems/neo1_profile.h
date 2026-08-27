#pragma once

// Shared Neo1 machine-profile definitions.
//
// A profile describes only 6502-visible top-memory policy: the ROM image,
// placement, and write-protection boundary. Runner-installed RAM tools,
// platform services, and CPU behavior are deliberately outside this contract.

#include <stddef.h>
#include <stdint.h>

#define NEO1_PERSONALITY_23 (23u)
#define NEO1_PERSONALITY_50 (50u)

typedef struct {
    uint8_t personality;
    const uint8_t* rom;
    size_t rom_size;
    uint16_t rom_base;
    uint16_t rom_protect_base;
} neo1_profile_t;

// Return the immutable shared profile, or NULL for an unsupported personality.
const neo1_profile_t* neo1_profile_find(unsigned personality);
