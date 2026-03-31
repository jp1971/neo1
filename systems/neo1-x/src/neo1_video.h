#pragma once

// neo1_video.h
//
// Neo1 DVI text video module.
//
// This module consumes terminal state (`neo1_terminal_t`) and renders it to a
// 640x480 monochrome DVI output path. Terminal ownership remains with the
// caller; the video module snapshots terminal state into internal buffers.

#include <stdint.h>
#include "neo1_terminal.h"

// Initialize clocks/DVI state, prepare font data, and bind the terminal source.
// Must be called before `neo1_video_start()`.
void neo1_video_init(neo1_terminal_t* term);

// Launch the DVI engine on core 1 and begin continuous scanline output.
void neo1_video_start(void);

// Publish latest terminal state. The copy is consumed asynchronously and is
// applied at a frame boundary to avoid tearing.
void neo1_video_set_terminal(neo1_terminal_t* term);