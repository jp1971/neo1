#include "neo1_platform.h"

// SDL implementation of the target-local mixed service surface. Display bytes
// feed a private 40x24 grid rendered on the main thread; this is a second
// terminal state machine, not an adapter around the Pico terminal. Input is
// sourced from SDL events, and storage is one seekable raw host image.
//
// Observable terminal differences from Pico are intentional preservation of
// current behavior: SDL erases for Backspace, ignores LF and form feed, uses six
// font bits in 14x16 cells, and draws its cursor directly. Rendering is
// unconditional each runner iteration and ignores update_display pixel inputs.

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/roms/neo1_apple1_video_rom_image.h"

static SDL_Window* window_handle;
static SDL_Renderer* renderer_handle;
static bool should_quit;
static FILE* disk_handle;
static char disk_path[512];

enum {
    NEO1_TERM_COLS = 40,
    NEO1_TERM_ROWS = 24,
    NEO1_CELL_W = 14,
    NEO1_CELL_H = 16,
};

static uint8_t term_cells[NEO1_TERM_ROWS][NEO1_TERM_COLS];
static int term_cursor_x;
static int term_cursor_y;
static bool term_dirty;
static uint64_t frame_count;

static bool should_reset;

#define NEO1_SDL_SECTOR_SIZE (512u)
#define NEO1_SDL_DEFAULT_DISK_MB (32u)

static bool neo1_platform_open_disk_image(void) {
    const char* env_path = getenv("NEO1_SDL_DISK");
    const char* path = (env_path && env_path[0]) ? env_path : "neo1_sdl_disk.img";

    disk_path[0] = '\0';
    (void)snprintf(disk_path, sizeof(disk_path), "%s", path);

    disk_handle = fopen(path, "r+b");
    if (!disk_handle) {
        disk_handle = fopen(path, "w+b");
        if (!disk_handle) {
            SDL_Log("disk image open/create failed: %s", path);
            return false;
        }

        // A missing image is created as a sparse 32 MiB raw block device.
        const long long bytes = (long long)NEO1_SDL_DEFAULT_DISK_MB * 1024ll * 1024ll;
        if ((bytes > 0) && (fseek(disk_handle, (long)(bytes - 1), SEEK_SET) == 0)) {
            (void)fputc(0, disk_handle);
            (void)fflush(disk_handle);
        }
    }

    SDL_Log("neo1-sdl disk image: %s", path);
    return true;
}

static void neo1_term_scroll_up(void) {
    memmove(&term_cells[0][0], &term_cells[1][0], (NEO1_TERM_ROWS - 1) * NEO1_TERM_COLS);
    memset(&term_cells[NEO1_TERM_ROWS - 1][0], ' ', NEO1_TERM_COLS);
}

static void neo1_term_newline(void) {
    term_cursor_x = 0;
    term_cursor_y++;
    if (term_cursor_y >= NEO1_TERM_ROWS) {
        neo1_term_scroll_up();
        term_cursor_y = NEO1_TERM_ROWS - 1;
    }
}

static void neo1_term_put_glyph(uint8_t ch) {
    if (term_cursor_x < 0 || term_cursor_x >= NEO1_TERM_COLS ||
        term_cursor_y < 0 || term_cursor_y >= NEO1_TERM_ROWS) {
        return;
    }
    term_cells[term_cursor_y][term_cursor_x] = ch;
    term_cursor_x++;
    if (term_cursor_x >= NEO1_TERM_COLS) {
        neo1_term_newline();
    }
}

static void neo1_term_draw(void) {
    if (!renderer_handle) {
        return;
    }

    SDL_SetRenderDrawColor(renderer_handle, 10, 12, 10, 255);
    SDL_RenderClear(renderer_handle);
    SDL_SetRenderDrawColor(renderer_handle, 120, 255, 140, 255);

    for (int row = 0; row < NEO1_TERM_ROWS; row++) {
        for (int col = 0; col < NEO1_TERM_COLS; col++) {
            const uint8_t ch = term_cells[row][col] & 0x7F;
            const int glyph_base = ((int)ch) * 8;
            if ((glyph_base + 7) >= (int)sizeof(apple1_vid)) {
                continue;
            }

            const int dst_x = col * NEO1_CELL_W;
            const int dst_y = row * NEO1_CELL_H;

            for (int gy = 0; gy < 8; gy++) {
                const uint8_t bits = apple1_vid[glyph_base + gy];
                for (int gx = 0; gx < 6; gx++) {
                    if (bits & (1u << gx)) {
                        SDL_Rect p = {
                            dst_x + gx * 2 + 1,
                            dst_y + gy * 2,
                            2,
                            2,
                        };
                        SDL_RenderFillRect(renderer_handle, &p);
                    }
                }
            }
        }
    }

    // Overlay a blinking '@' cursor at about 1.3 Hz using SDL wall-clock time.
    const uint64_t ms = SDL_GetTicks64();
    if (((ms / 375) & 1u) == 0u &&
        term_cursor_x >= 0 && term_cursor_x < NEO1_TERM_COLS &&
        term_cursor_y >= 0 && term_cursor_y < NEO1_TERM_ROWS) {
        const int cbx = ((int)'@') * 8;
        const int cdx = term_cursor_x * NEO1_CELL_W;
        const int cdy = term_cursor_y * NEO1_CELL_H;
        for (int gy = 0; gy < 8; gy++) {
            const uint8_t bits = apple1_vid[cbx + gy];
            for (int gx = 0; gx < 6; gx++) {
                if (bits & (1u << gx)) {
                    SDL_Rect p = {cdx + gx * 2 + 1, cdy + gy * 2, 2, 2};
                    SDL_RenderFillRect(renderer_handle, &p);
                }
            }
        }
    }
}

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

    SDL_RaiseWindow(window_handle);
    SDL_StartTextInput();

    memset(term_cells, ' ', sizeof(term_cells));
    term_cursor_x = 0;
    term_cursor_y = 0;
    term_dirty = true;
    frame_count = 0;
    should_reset = false;

    (void)neo1_platform_open_disk_image();
}

void neo1_platform_shutdown(void) {
    SDL_StopTextInput();

    if (disk_handle) {
        fclose(disk_handle);
        disk_handle = NULL;
    }
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

    // Redraw unconditionally; term_dirty and frame_count do not gate rendering.
    neo1_term_draw();
    term_dirty = false;
    frame_count++;
    SDL_RenderPresent(renderer_handle);
}

void neo1_platform_put_char(uint8_t ch) {
    ch &= 0x7F;

    switch (ch) {
        case '\r':
            neo1_term_newline();
            term_dirty = true;
            return;
        case '\n':
            term_dirty = true;
            return;
        case 0x08:
            if (term_cursor_x > 0) {
                term_cursor_x--;
                term_cells[term_cursor_y][term_cursor_x] = ' ';
                term_dirty = true;
            }
            return;
        default:
            break;
    }

    if (ch < 0x20) {
        return;
    }

    neo1_term_put_glyph(ch);
    term_dirty = true;
}

bool neo1_platform_should_reset(void) {
    bool r = should_reset;
    should_reset = false;
    return r;
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

        if (event.type == SDL_MOUSEBUTTONDOWN) {
            SDL_RaiseWindow(window_handle);
            continue;
        }

        if (event.type == SDL_TEXTINPUT) {
            const unsigned char ch = (unsigned char)event.text.text[0];
            // SDL_TEXTINPUT supplies printable bytes; KEYDOWN owns controls so
            // Return cannot also arrive as a text-input CR.
            if ((ch >= 0x20) && (ch != 0x7F)) {
                if (out_apple1_keycode) {
                    *out_apple1_keycode = (uint8_t)ch;
                }
                if (out_pressed) {
                    *out_pressed = true;
                }
                return true;
            }
            continue;
        }

        if (event.type == SDL_KEYDOWN) {
            const SDL_Keycode key = event.key.keysym.sym;
            const SDL_Keymod mod = (SDL_Keymod)event.key.keysym.mod;
            const bool ctrl = (mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;

            if (ctrl && key == SDLK_l) {
                memset(term_cells, ' ', sizeof(term_cells));
                term_cursor_x = 0;
                term_cursor_y = 0;
                term_dirty = true;
                continue;
            }
            if (ctrl && key == SDLK_r) {
                should_reset = true;
                continue;
            }

            // Printable input is handled above; ignore repeated control-key
            // events so Return and Backspace are injected once per press.
            if (event.key.repeat != 0) {
                continue;
            }

            if (out_pressed) {
                *out_pressed = true;
            }

            if (out_apple1_keycode) {
                if (key == SDLK_RETURN) {
                    *out_apple1_keycode = '\r';
                } else if (key == SDLK_BACKSPACE) {
                    *out_apple1_keycode = 0x08;
                } else {
                    if (out_pressed) {
                        *out_pressed = false;
                    }
                    continue;
                }
            }
            return true;
        }
    }
    return false;
}

uint64_t neo1_platform_time_us(void) {
    return (uint64_t)SDL_GetTicks64() * 1000ULL;
}

bool neo1_platform_disk_read(uint32_t lba, uint8_t* buf, uint32_t count) {
    if (!disk_handle || !buf || (count == 0)) {
        return false;
    }

    const uint64_t offset = (uint64_t)lba * (uint64_t)NEO1_SDL_SECTOR_SIZE;
    const size_t bytes = (size_t)count * (size_t)NEO1_SDL_SECTOR_SIZE;

    if (fseek(disk_handle, (long)offset, SEEK_SET) != 0) {
        return false;
    }

    return fread(buf, 1, bytes, disk_handle) == bytes;
}

bool neo1_platform_disk_write(uint32_t lba, const uint8_t* buf, uint32_t count) {
    if (!disk_handle || !buf || (count == 0)) {
        return false;
    }

    const uint64_t offset = (uint64_t)lba * (uint64_t)NEO1_SDL_SECTOR_SIZE;
    const size_t bytes = (size_t)count * (size_t)NEO1_SDL_SECTOR_SIZE;

    if (fseek(disk_handle, (long)offset, SEEK_SET) != 0) {
        return false;
    }

    if (fwrite(buf, 1, bytes, disk_handle) != bytes) {
        return false;
    }

    return fflush(disk_handle) == 0;
}

const char* neo1_platform_disk_path(void) {
    return disk_path;
}

bool neo1_platform_should_quit(void) {
    return should_quit;
}
