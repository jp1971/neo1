#pragma once

// SDL-local divergent storage accommodations attached through the shared
// machine's optional-device ports.

#include <stdbool.h>
#include <stdint.h>

#include "devices/neo1_cffa1.h"
#include "devices/neo1_msc.h"

void neo1_msc_init(void);
uint8_t neo1_msc_io_read(uint16_t addr);
void neo1_msc_io_write(uint16_t addr, uint8_t data);

void neo1_cffa1_init(void);
uint8_t neo1_cffa1_io_read(uint16_t addr);
void neo1_cffa1_io_write(uint16_t addr, uint8_t data);
