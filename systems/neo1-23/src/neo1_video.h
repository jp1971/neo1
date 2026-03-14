#pragma once

#include <stdint.h>
#include "neo1_terminal.h"

void neo1_video_init(neo1_terminal_t* term);
void neo1_video_start(void);
void neo1_video_set_terminal(neo1_terminal_t* term);