#pragma once

// neo1_video.h
//
// Neo1 DVI text video module.
//
// This Pico-only module snapshots a caller-owned `neo1_terminal_t` and renders
// it in a 640x480 monochrome DVI mode. Native 4:3 rendering uses a 640x384 text
// area; optional widescreen-stretch compensation uses a centered 480x384 text
// area. The module owns system clock/voltage setup, font expansion, three
// publication buffers, the PicoDVI instance, and core 1.

#include <stdbool.h>
#include <stdint.h>
#include "terminal/neo1_terminal.h"

// Initialize clocks/DVI state, prepare font data, and bind the terminal source.
// Must be called before `neo1_video_start()`.
void neo1_video_init(neo1_terminal_t* term);

// Launch the DVI engine on core 1 and begin continuous scanline output.
void neo1_video_start(void);

// Toggle between full-width native 4:3 rendering and a centered view that
// compensates for 16:9 displays which stretch a 4:3 input. Returns true when
// widescreen-stretch compensation is enabled. The DVI timing remains 640x480.
bool neo1_video_toggle_widescreen_correction(void);

// Copy a complete terminal snapshot into a core-0 producer buffer and publish
// it under a short cross-core critical section. Core 1 changes the front
// snapshot only at a frame boundary. Passing null disables terminal rendering.
void neo1_video_set_terminal(neo1_terminal_t* term);
