#include "neo1_platform.h"

#define CHIPS_IMPL

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chips/chips_common.h"
#include "chips/neo1_cpu_backend.h"
#include "chips/mem.h"
#include "chips/clk.h"

#if NEO1_PERSONALITY == 50
#define NEO1_ROM_BASE (0xFF00)
#define NEO1_ROM_PROTECT_BASE (0xFF00)
#endif

#include "systems/neo1.h"
#include "roms/neo1_roms.h"

static bool g_stdout_echo = false;

static void neo1_host_char_out(uint8_t ch, void* user_data) {
    (void)user_data;
    neo1_platform_put_char(ch);
    if (g_stdout_echo) {
        putchar((char)(ch & 0x7F));
        fflush(stdout);
    }
}

int main(void) {
    const int window_width = 560;
    const int window_height = 384;
    neo1_t neo1;
    uint8_t disk_probe[512];

    g_stdout_echo = (getenv("NEO1_SDL_STDOUT") != NULL);

    const chips_range_t rom_range =
#if NEO1_PERSONALITY == 50
        (chips_range_t){
            .ptr = neo1_apple1_rom_bin,
            .size = (size_t)neo1_apple1_rom_bin_len,
        };
#else
        (chips_range_t){
            .ptr = neo1_system_rom_bin,
            .size = (size_t)neo1_system_rom_bin_len,
        };
#endif

    const neo1_desc_t desc = {
        .roms.rom = rom_range,
        .char_out.func = neo1_host_char_out,
        .char_out.user_data = NULL,
    };

    neo1_platform_init(window_width, window_height, "Neo1 Host (M0)");
    printf("[neo1-sdl] disk image: %s\n", neo1_platform_disk_path());
    if (neo1_platform_disk_read(0, disk_probe, 1)) {
        printf("[neo1-sdl] storage self-test: sector 0 read OK (first bytes: %02X %02X %02X %02X)\n",
               disk_probe[0], disk_probe[1], disk_probe[2], disk_probe[3]);
    } else {
        printf("[neo1-sdl] storage self-test: sector 0 read FAILED\n");
    }

    neo1_msc_init();
#if NEO1_ENABLE_VCFFA1
    neo1_cffa1_init();
#endif
    neo1_init(&neo1, &desc);
    neo1_reset(&neo1);

    while (!neo1_platform_should_quit()) {
        uint8_t keycode = 0;
        bool pressed = false;

        if (neo1_platform_poll_key(&keycode, &pressed) && pressed) {
            if ((keycode >= 'a') && (keycode <= 'z')) {
                keycode = (uint8_t)toupper((int)keycode);
            }
            neo1_key_down(&neo1, keycode);
        }

        // Run a small host-time slice to keep UI responsive.
        (void)neo1_exec(&neo1, 2000);
        neo1_platform_update_display(NULL, 0, 0);
    }

    neo1_discard(&neo1);
    neo1_platform_shutdown();
    return 0;
}