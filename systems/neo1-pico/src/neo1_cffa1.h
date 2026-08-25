#pragma once

// Pico FatFs image implementation of the shared VCFFA1 register contract.

#include <stdbool.h>
#include <stdint.h>

#include "devices/neo1_cffa1.h"

void neo1_cffa1_init(void);
uint8_t neo1_cffa1_io_read(uint16_t addr);
void neo1_cffa1_io_write(uint16_t addr, uint8_t data);
