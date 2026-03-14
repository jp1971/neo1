#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sem.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"

#include "common_dvi_pin_configs.h"
#include "dvi.h"
#include "dvi_serialiser.h"
#include "tmds_encode.h"
#include "console_font_8x8.h"

#include "neo1_terminal.h"
#include "neo1_video.h"

#ifndef NEO1_ENABLE_DVI
#define NEO1_ENABLE_DVI (1)
#endif

#define FONT_CHAR_WIDTH 8
#define FONT_CHAR_HEIGHT 8

#if NEO1_ENABLE_DVI

#ifdef OLIMEX_NEO6502
#define VREG_VSEL VREG_VOLTAGE_1_10
#endif

#define FRAME_WIDTH   640
#define FRAME_HEIGHT  480
#define DVI_TIMING    dvi_timing_640x480p_60hz

#define NEO1_SCALED_CHAR_WIDTH   (FONT_CHAR_WIDTH * 2)
#define NEO1_SCALED_CHAR_HEIGHT  (FONT_CHAR_HEIGHT * 2)
#define NEO1_ACTIVE_HEIGHT       (NEO1_TERM_ROWS * NEO1_SCALED_CHAR_HEIGHT)
#define NEO1_EMPTY_LINES         (((FRAME_HEIGHT - NEO1_ACTIVE_HEIGHT) / 2))
#define NEO1_SCANLINE_BYTES      (FRAME_WIDTH / 8)
#define NEO1_TEXT_BYTES          (NEO1_TERM_COLS * 2)
#define NEO1_LEFT_PAD_BYTES      ((NEO1_SCANLINE_BYTES - NEO1_TEXT_BYTES) / 2)

static struct dvi_inst dvi0;
static struct semaphore dvi_start_sem;
static volatile uint32_t dvi_frame_counter = 0;
static volatile uint32_t dvi_line_counter = 0;

static neo1_terminal_t* g_term = 0;
static neo1_terminal_t g_term_buffers[2];
static volatile uint32_t g_front_buffer_index = 0;
static volatile uint32_t g_pending_buffer_index = 0;
static volatile bool g_has_pending_buffer = false;
static volatile bool g_term_dirty = false;
static volatile uint32_t g_set_terminal_calls = 0;
static volatile uint32_t g_terminal_buffer_swaps = 0;
#define NEO1_CURSOR_BLINK_FRAMES 30
static uint16_t __not_in_flash("neo1_video_font") g_font_16x8_ram[256 * FONT_CHAR_HEIGHT];

static inline uint8_t neo1_reverse_bits8(uint8_t v) {
    v = (uint8_t)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (uint8_t)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (uint8_t)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

static inline uint16_t neo1_expand_row_2x(uint8_t bits) {
    uint16_t out = 0;
    for (uint32_t i = 0; i < 8; i++) {
        if (bits & (1u << i)) {
            out |= (uint16_t)(3u << (i * 2));
        }
    }
    return out;
}

static void __not_in_flash_func(neo1_video_prepare_scanline)(uint32_t line) {
    static uint8_t scanbuf[FRAME_WIDTH / 8];
    memset(scanbuf, 0, sizeof(scanbuf));

    if (!g_term) {
        goto encode_line;
    }

    if (line >= NEO1_EMPTY_LINES) {
        const uint32_t active_height = NEO1_ACTIVE_HEIGHT;
        if (line < (NEO1_EMPTY_LINES + active_height)) {
            const uint32_t local_y = line - NEO1_EMPTY_LINES;
            const uint32_t row = local_y / NEO1_SCALED_CHAR_HEIGHT;
            const uint32_t gy  = (local_y / 2) % FONT_CHAR_HEIGHT;
            const uint32_t cell_y = local_y % NEO1_SCALED_CHAR_HEIGHT;

            if (row < NEO1_TERM_ROWS) {
                for (uint32_t col = 0; col < NEO1_TERM_COLS; col++) {
                    uint8_t ch = g_term_buffers[g_front_buffer_index].chars[row][col];
                    ch &= 0x7F;

                    uint32_t dst_byte = NEO1_LEFT_PAD_BYTES + (col * 2);
                    if ((dst_byte + 1) < NEO1_SCANLINE_BYTES) {
                        uint16_t bits16 = g_font_16x8_ram[((uint32_t)ch * FONT_CHAR_HEIGHT) + gy];
                        scanbuf[dst_byte + 0] = (uint8_t)(bits16 & 0xFFu);
                        scanbuf[dst_byte + 1] = (uint8_t)(bits16 >> 8);
                    }
                }
            }
            if (row < NEO1_TERM_ROWS) {
                const bool cursor_blink_on = (((dvi_frame_counter / NEO1_CURSOR_BLINK_FRAMES) & 1u) == 0u);
                const uint32_t cursor_x = g_term_buffers[g_front_buffer_index].cursor_x;
                const uint32_t cursor_y = g_term_buffers[g_front_buffer_index].cursor_y;

                if (cursor_blink_on &&
                    (cursor_y < NEO1_TERM_ROWS) &&
                    (cursor_x < NEO1_TERM_COLS) &&
                    (row == cursor_y) &&
                    (cell_y >= (NEO1_SCALED_CHAR_HEIGHT - 2))) {
                    const uint32_t cursor_byte = NEO1_LEFT_PAD_BYTES + (cursor_x * 2);
                    if ((cursor_byte + 1) < NEO1_SCANLINE_BYTES) {
                        scanbuf[cursor_byte + 0] = 0xFFu;
                        scanbuf[cursor_byte + 1] = 0xFFu;
                    }
                }
            }
        }
    }

encode_line:
    uint32_t* tmdsbuf;
    queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmdsbuf);
    tmds_encode_1bpp((const uint32_t*)scanbuf, tmdsbuf, FRAME_WIDTH);
    queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmdsbuf);
}

static void __not_in_flash_func(_scanline_callback)(void) {
    if (dvi_line_counter == FRAME_HEIGHT) {
        dvi_frame_counter++;
        dvi_line_counter = 0;

        if (g_term && g_term_dirty) {
            uint32_t target_index = 1u - g_front_buffer_index;
            memcpy(&g_term_buffers[target_index], g_term, sizeof(g_term_buffers[target_index]));
            g_pending_buffer_index = target_index;
            g_has_pending_buffer = true;
            g_term_dirty = false;
        }

        if (g_has_pending_buffer) {
            g_front_buffer_index = g_pending_buffer_index;
            g_has_pending_buffer = false;
            g_terminal_buffer_swaps++;
        }
    }

    neo1_video_prepare_scanline(dvi_line_counter);
    dvi_line_counter++;
}

static void __not_in_flash_func(core1_main)(void) {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    sem_acquire_blocking(&dvi_start_sem);
    dvi_start(&dvi0);
    while (1) {
        __wfi();
    }
}

void neo1_video_set_terminal(neo1_terminal_t* term) {
    g_term = term;
    g_set_terminal_calls++;

    if (g_term) {
        g_term_dirty = true;
    }
}

void neo1_video_init(neo1_terminal_t* term) {
#ifdef OLIMEX_NEO6502
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);
#endif

    g_front_buffer_index = 0;
    g_pending_buffer_index = 0;
    g_has_pending_buffer = false;
    g_term_dirty = false;
    g_set_terminal_calls = 0;
    g_terminal_buffer_swaps = 0;

    neo1_video_set_terminal(term);

    if (g_term) {
        memcpy(&g_term_buffers[0], g_term, sizeof(g_term_buffers[0]));
        g_pending_buffer_index = 0;
        g_has_pending_buffer = false;
        g_term_dirty = false;
    }
    for (uint32_t ch = 0; ch < 256; ch++) {
        for (uint32_t row = 0; row < FONT_CHAR_HEIGHT; row++) {
            uint8_t bits = console_font_8x8[(ch * FONT_CHAR_HEIGHT) + row];
            bits = neo1_reverse_bits8(bits);
            g_font_16x8_ram[(ch * FONT_CHAR_HEIGHT) + row] = neo1_expand_row_2x(bits);
        }
    }

    sem_init(&dvi_start_sem, 0, 1);
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi0.scanline_callback = _scanline_callback;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    dvi_line_counter = 1;
    neo1_video_prepare_scanline(0);
}

void neo1_video_start(void) {
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    multicore_launch_core1(core1_main);
    sem_release(&dvi_start_sem);
}

#else

void neo1_video_set_terminal(neo1_terminal_t* term) {
    (void)term;
}

void neo1_video_init(neo1_terminal_t* term) {
    (void)term;
}

void neo1_video_start(void) {
}

#endif