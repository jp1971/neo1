#pragma once

#include <stdint.h>

// Generated from the 8 KB Replica 1 Ten-style ROM image via:
//   xxd -i neo1_system_rom.bin > neo1_system_rom_image.h
// Keep the generated header in this same roms directory.
#include "neo1_system_rom_image.h"

// clang-format off
// For now, keep the legacy symbol name so the rest of the bring-up continues
// to compile while we switch the top-of-memory ROM mapping over to Neo 1.
#define apple1_rom neo1_system_rom_bin

// clang-format on
