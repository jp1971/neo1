#pragma once

// Pico FatFs implementation of the shared Neo1 MSC register contract.

#include <stdbool.h>
#include <stdint.h>

#include "devices/neo1_msc.h"

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS 0
#endif
#ifndef NEO1_MSC_DEBUG
#define NEO1_MSC_DEBUG NEO1_DIAGNOSTICS
#endif

void neo1_msc_init(void);
uint8_t neo1_msc_io_read(uint16_t addr);
void neo1_msc_io_write(uint16_t addr, uint8_t data);
