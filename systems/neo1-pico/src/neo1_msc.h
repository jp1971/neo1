#pragma once

// Pico FatFs backend for the shared Neo1 MSC register protocol.

#include <stdbool.h>
#include <stdint.h>

#include "devices/neo1_msc.h"

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS 0
#endif
#ifndef NEO1_MSC_DEBUG
#define NEO1_MSC_DEBUG NEO1_DIAGNOSTICS
#endif

const neo1_msc_backend_t* neo1_msc_fatfs_backend(void);
