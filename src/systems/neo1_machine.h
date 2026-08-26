#pragma once

// CPU-neutral Neo1 machine and 6502-visible address space.
//
// The machine owns the 64 KB backing store, ROM placement/write protection,
// Apple-1 keyboard/display state, and optional-device decode. A physical or
// software CPU supplies explicit reads and writes; CPU execution, reset pins,
// timing, tracing, and platform lifecycle remain outside this state.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "devices/neo1_apple1_pia.h"
#include "devices/neo1_cffa1.h"
#include "devices/neo1_msc.h"

#define NEO1_MACHINE_MEM_SIZE (0x10000u)

typedef void (*neo1_char_out_t)(uint8_t ch, void* user_data);
typedef uint8_t (*neo1_device_read_t)(void* user_data, uint16_t addr);
typedef void (*neo1_device_write_t)(void* user_data, uint16_t addr, uint8_t data);

typedef struct {
    neo1_device_read_t read;
    neo1_device_write_t write;
    void* user_data;
} neo1_device_port_t;

typedef struct {
    const uint8_t* rom;
    size_t rom_size;
    uint16_t rom_base;
    uint16_t rom_protect_base;
    neo1_char_out_t char_out;
    void* char_out_user_data;
    neo1_device_port_t msc;
    neo1_device_port_t vcffa1;
} neo1_machine_desc_t;

typedef struct {
    uint8_t ram[NEO1_MACHINE_MEM_SIZE];
    uint16_t rom_base;
    uint16_t rom_protect_base;
    neo1_apple1_pia_t pia;
    neo1_char_out_t char_out;
    void* char_out_user_data;
    neo1_device_port_t msc;
    neo1_device_port_t vcffa1;
} neo1_machine_t;

// Initialize the deterministic backing store and copy the selected top ROM.
// Returns false for a missing/empty ROM or an image that crosses $FFFF.
bool neo1_machine_init(neo1_machine_t* machine, const neo1_machine_desc_t* desc);

// Reset 6502-visible device state without changing RAM or ROM contents.
void neo1_machine_reset(neo1_machine_t* machine);

// Service one explicit 6502 bus access.
uint8_t neo1_machine_read(neo1_machine_t* machine, uint16_t addr);
void neo1_machine_write(neo1_machine_t* machine, uint16_t addr, uint8_t data);

// Inject one host input byte through the shared Apple-1 keyboard latch.
void neo1_machine_key_down(neo1_machine_t* machine, uint8_t ascii);
