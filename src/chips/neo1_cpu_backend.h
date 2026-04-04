#pragma once

// neo1_cpu_backend.h
//
// Single include point for selecting the CPU backend used by Neo1.
//
// Current default keeps existing behavior:
// - neo1-pico uses the hardware 65C02 glue (wdc65C02cpu.h)
//
// Future host milestones can set NEO1_CPU_BACKEND via CMake and route this
// include to a software CPU backend without touching higher-level runtime code.

#define NEO1_CPU_BACKEND_WDC65C02 (1)
#define NEO1_CPU_BACKEND_MOS6502  (2)
#define NEO1_CPU_BACKEND_SOFT65C02 (3)

#ifndef NEO1_CPU_BACKEND
#define NEO1_CPU_BACKEND NEO1_CPU_BACKEND_WDC65C02
#endif

#if NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_WDC65C02
#include "wdc65C02cpu.h"
#elif NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_MOS6502
#include "mos6502cpu.h"
#elif NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02
#include "soft65C02cpu.h"
#else
#error "Unsupported NEO1_CPU_BACKEND value"
#endif
