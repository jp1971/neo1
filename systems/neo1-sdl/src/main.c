#include "neo1_platform.h"

// SDL runner for the software-CPU Neo1 profiles. It constructs the shared
// machine, selects the profile ROM, owns the event/render loop and raw-disk
// startup probe, and connects machine display bytes to the SDL-local terminal.
// It does not install the Pico runner's VACI/VCFFA1 RAM tools or Neo1-50 entry
// stubs. `neo1_platform.h` is an SDL-local mixed service interface, not a shared
// platform abstraction.

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if NEO1_PERSONALITY == 50
#define NEO1_ROM_BASE (0xFF00)
#define NEO1_ROM_PROTECT_BASE (0xFF00)
#else
#define NEO1_ROM_BASE (0xE000)
#define NEO1_ROM_PROTECT_BASE (0xE000)
#endif

#include "runners/neo1_soft_runner.h"
#include "roms/neo1_roms.h"
#include "neo1_storage_stub.h"

static bool g_stdout_echo = false;

#if NEO1_ENABLE_MSC
static uint8_t neo1_msc_port_read(void* user_data, uint16_t addr) {
    (void)user_data;
    return neo1_msc_io_read(addr);
}

static void neo1_msc_port_write(void* user_data, uint16_t addr, uint8_t data) {
    (void)user_data;
    neo1_msc_io_write(addr, data);
}
#endif

#if NEO1_ENABLE_VCFFA1
static uint8_t neo1_cffa1_port_read(void* user_data, uint16_t addr) {
    (void)user_data;
    return neo1_cffa1_io_read(addr);
}

static void neo1_cffa1_port_write(void* user_data, uint16_t addr, uint8_t data) {
    (void)user_data;
    neo1_cffa1_io_write(addr, data);
}
#endif

enum {
    // Do not execute an unbounded backlog after a debugger stop, window drag,
    // host sleep, or other long scheduling pause.
    NEO1_SDL_MAX_CATCH_UP_US = 100000,
    NEO1_SDL_IDLE_SLEEP_MS = 1,
};

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
    neo1_machine_t machine;
    neo1_soft_runner_t cpu;
    uint8_t disk_probe[512];

    g_stdout_echo = (getenv("NEO1_SDL_STDOUT") != NULL);

    const uint8_t* rom =
#if NEO1_PERSONALITY == 50
        neo1_apple1_rom_bin;
#else
        neo1_system_rom_bin;
#endif
    const size_t rom_size =
#if NEO1_PERSONALITY == 50
        (size_t)neo1_apple1_rom_bin_len;
#else
        (size_t)neo1_system_rom_bin_len;
#endif

    const neo1_machine_desc_t desc = {
        .rom = rom,
        .rom_size = rom_size,
        .rom_base = NEO1_ROM_BASE,
        .rom_protect_base = NEO1_ROM_PROTECT_BASE,
        .char_out = neo1_host_char_out,
        .char_out_user_data = NULL,
#if NEO1_ENABLE_MSC
        .msc = {
            .read = neo1_msc_port_read,
            .write = neo1_msc_port_write,
        },
#endif
#if NEO1_ENABLE_VCFFA1
        .vcffa1 = {
            .read = neo1_cffa1_port_read,
            .write = neo1_cffa1_port_write,
        },
#endif
    };

    neo1_platform_init(window_width, window_height, "Neo1 Host (M0)");
    printf("[neo1-sdl] disk image: %s\n", neo1_platform_disk_path());
    if (neo1_platform_disk_read(0, disk_probe, 1)) {
        printf("[neo1-sdl] storage self-test: sector 0 read OK (first bytes: %02X %02X %02X %02X)\n",
               disk_probe[0], disk_probe[1], disk_probe[2], disk_probe[3]);
    } else {
        printf("[neo1-sdl] storage self-test: sector 0 read FAILED\n");
    }

#if NEO1_ENABLE_MSC
    neo1_msc_init();
#endif
#if NEO1_ENABLE_VCFFA1
    neo1_cffa1_init();
#endif
    if (!neo1_machine_init(&machine, &desc) ||
        !neo1_soft_runner_init(&cpu, &machine))
    {
        fprintf(stderr, "[neo1-sdl] machine/CPU initialization failed\n");
        neo1_platform_shutdown();
        return 1;
    }
    neo1_machine_reset(&machine);
    neo1_soft_runner_reset(&cpu);

    uint64_t previous_time_us = neo1_platform_time_us();

    while (!neo1_platform_should_quit()) {
        uint8_t keycode = 0;
        bool pressed = false;

        if (neo1_platform_poll_key(&keycode, &pressed) && pressed) {
            if ((keycode >= 'a') && (keycode <= 'z')) {
                keycode = (uint8_t)toupper((int)keycode);
            }
            neo1_machine_key_down(&machine, keycode);
        }

        if (neo1_platform_should_reset()) {
            neo1_machine_reset(&machine);
            neo1_soft_runner_reset(&cpu);
        }

        const uint64_t current_time_us = neo1_platform_time_us();
        uint64_t elapsed_us = current_time_us - previous_time_us;
        previous_time_us = current_time_us;
        if (elapsed_us > NEO1_SDL_MAX_CATCH_UP_US) {
            elapsed_us = NEO1_SDL_MAX_CATCH_UP_US;
        }
        if (elapsed_us > 0) {
            (void)neo1_soft_runner_exec_us(&cpu, (uint32_t)elapsed_us);
        }
        neo1_platform_update_display(NULL, 0, 0);
        neo1_platform_sleep_ms(NEO1_SDL_IDLE_SLEEP_MS);
    }

    neo1_soft_runner_discard(&cpu);
    neo1_platform_shutdown();
    return 0;
}
