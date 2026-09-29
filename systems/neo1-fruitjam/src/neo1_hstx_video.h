#pragma once

// Fruit Jam-owned RP2350 HSTX renderer. The module snapshots the shared
// terminal grid, rasterizes it on core 1, and scans it out as 640x480 DVI.
// It owns no Apple-1 memory, PIA, CPU, or terminal mutation semantics.

#include <stdbool.h>

#include "terminal/neo1_terminal.h"

// Configure 640x480 HSTX DVI, bind the initial grid, and launch the video
// worker on core 1. Returns only after the scanout DMA is running.
bool neo1_hstx_video_init(const neo1_terminal_t* term);

// Publish a complete terminal snapshot. The video worker accepts snapshots at
// raster boundaries and the DMA accepts finished rasters at frame boundaries.
void neo1_hstx_video_set_terminal(const neo1_terminal_t* term);
