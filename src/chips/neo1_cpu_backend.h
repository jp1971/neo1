#pragma once

// neo1_cpu_backend.h
//
// Transitional compile-time selector for the CPU adapter embedded by
// src/systems/neo1.h.
//
// Backend 1 connects the Pico build to the physical W65C02 bus. Backend 3
// adapts fake65c02 for the SDL runner. This macro surface reflects the present
// CHIPS_IMPL integration; it is not the intended boundary between the portable
// machine and its physical or software CPU runners.

#define NEO1_CPU_BACKEND_WDC65C02 (1)
#define NEO1_CPU_BACKEND_SOFT65C02 (3)

#ifndef NEO1_CPU_BACKEND
#define NEO1_CPU_BACKEND NEO1_CPU_BACKEND_WDC65C02
#endif

#if NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_WDC65C02
#include "wdc65C02cpu.h"
#elif NEO1_CPU_BACKEND == NEO1_CPU_BACKEND_SOFT65C02
#include "soft65C02cpu.h"
#else
#error "Unsupported NEO1_CPU_BACKEND value"
#endif
