#pragma once

// neo1_video.h
//
// Neo1 DVI text video module.
//
// This Pico-only module snapshots a caller-owned `neo1_terminal_t` and renders
// a centered 640x384 text area in a 640x480 monochrome DVI mode. It owns system
// clock/voltage setup, font expansion, three publication buffers, the PicoDVI
// instance, and core 1.

#include <stdint.h>
#include "neo1_terminal.h"

// Initialize clocks/DVI state, prepare font data, and bind the terminal source.
// Must be called before `neo1_video_start()`.
void neo1_video_init(neo1_terminal_t* term);

// Launch the DVI engine on core 1 and begin continuous scanline output.
void neo1_video_start(void);

// Copy a complete terminal snapshot into a core-0 producer buffer and publish
// it under a short cross-core critical section. Core 1 changes the front
// snapshot only at a frame boundary. Passing null disables terminal rendering.
void neo1_video_set_terminal(neo1_terminal_t* term);
