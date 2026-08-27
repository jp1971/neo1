#pragma once

// SDL raw-image MSC backend and local VCFFA1 compatibility implementation.

#include <stdbool.h>
#include <stdint.h>

#include "devices/neo1_cffa1.h"
#include "devices/neo1_msc.h"

const neo1_msc_backend_t* neo1_sdl_msc_backend(void);

void neo1_cffa1_init(void);
uint8_t neo1_cffa1_io_read(uint16_t addr);
void neo1_cffa1_io_write(uint16_t addr, uint8_t data);
