#pragma once

// neo1_video.h
//
// Neo1 DVI text video module.
//
// This Pico-only module consumes a caller-owned `neo1_terminal_t` and renders a
// centered 640x384 text area in a 640x480 monochrome DVI mode. It owns system
// clock/voltage setup, font expansion, snapshot buffers, the PicoDVI instance,
// and core 1. The caller must keep the terminal alive after initialization.

#include <stdint.h>
#include "neo1_terminal.h"

// Initialize clocks/DVI state, prepare font data, and bind the terminal source.
// Must be called before `neo1_video_start()`.
void neo1_video_init(neo1_terminal_t* term);

// Launch the DVI engine on core 1 and begin continuous scanline output.
void neo1_video_start(void);

// Mark the bound terminal dirty. Core 1 copies it and changes the front snapshot
// at a frame boundary. The current volatile-flag handoff has no lock, so this is
// frame-boundary publication but not a guaranteed atomic source snapshot.
void neo1_video_set_terminal(neo1_terminal_t* term);
