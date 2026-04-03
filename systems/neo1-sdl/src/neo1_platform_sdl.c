#include "neo1_platform.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

static SDL_Window* window_handle;
static SDL_Renderer* renderer_handle;
static bool should_quit;

void neo1_platform_init(int width, int height, const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    window_handle = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN);
    if (!window_handle) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }

    renderer_handle = SDL_CreateRenderer(window_handle, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_handle) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return;
    }

    SDL_SetRenderDrawColor(renderer_handle, 16, 16, 16, 255);
    SDL_RenderClear(renderer_handle);
    SDL_RenderPresent(renderer_handle);
}

void neo1_platform_shutdown(void) {
    if (renderer_handle) {
        SDL_DestroyRenderer(renderer_handle);
        renderer_handle = NULL;
    }
    if (window_handle) {
        SDL_DestroyWindow(window_handle);
        window_handle = NULL;
    }
    SDL_Quit();
}

void neo1_platform_update_display(const uint32_t* pixels, int width, int height) {
    (void)pixels;
    (void)width;
    (void)height;

    if (!renderer_handle) {
        return;
    }
    SDL_RenderPresent(renderer_handle);
}

bool neo1_platform_poll_key(uint8_t* out_apple1_keycode, bool* out_pressed) {
    SDL_Event event;

    if (out_apple1_keycode) {
        *out_apple1_keycode = 0;
    }
    if (out_pressed) {
        *out_pressed = false;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            should_quit = true;
            return false;
        }
    }
    return false;
}

uint64_t neo1_platform_time_us(void) {
    return (uint64_t)SDL_GetTicks64() * 1000ULL;
}

bool neo1_platform_disk_read(uint32_t lba, uint8_t* buf, uint32_t count) {
    (void)lba;
    (void)buf;
    (void)count;
    return false;
}

bool neo1_platform_disk_write(uint32_t lba, const uint8_t* buf, uint32_t count) {
    (void)lba;
    (void)buf;
    (void)count;
    return false;
}

bool neo1_platform_should_quit(void) {
    return should_quit;
}