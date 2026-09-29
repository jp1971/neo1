// Fruit Jam HSTX DVI text renderer.
//
// HSTX setup, command lists, and ping-pong DMA sequencing are adapted from
// Raspberry Pi's `pico-examples/hstx/dvi_out_hstx_encoder` example:
// Copyright (c) 2024 Raspberry Pi (Trading) Ltd.
// SPDX-License-Identifier: BSD-3-Clause
// See ../LICENSE.hstx-dvi.txt for the complete retained license.
//
// Neo1-specific pipeline:
// - core 0 publishes snapshots of the shared 40x24 terminal grid;
// - core 1 rasterizes a pending snapshot into an inactive 640x192 RGB332 text
//   surface (one row per unscaled glyph row);
// - DMA reads completed text rows directly, with no per-scanline pixel work;
// - completed rasters swap only at a frame boundary.
//
// Two bounded 122,880-byte text rasters stay in internal SRAM. This requires
// neither PSRAM nor a 640x480 framebuffer and keeps the scanout IRQ constant-
// time even while terminal content is changing.

#include "neo1_hstx_video.h"

#include <stdint.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/sync.h"
#include "pico/critical_section.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdlib.h"

#include "../../../src/roms/neo1_apple1_video_rom_image.h"

#ifndef ADAFRUIT_FRUIT_JAM
#error "Neo1 Fruit Jam HSTX video requires PICO_BOARD=adafruit_fruit_jam"
#endif

// Fruit Jam routes all four differential pairs in negative/positive order on
// the RP2350's fixed GPIO12..19 HSTX outputs. Keep this compile-time check next
// to the mapping rather than silently assuming a different board layout.
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_CKN_PIN == 12, "unexpected Fruit Jam DVI clock N pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_CKP_PIN == 13, "unexpected Fruit Jam DVI clock P pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D0N_PIN == 14, "unexpected Fruit Jam DVI D0 N pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D0P_PIN == 15, "unexpected Fruit Jam DVI D0 P pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D1N_PIN == 16, "unexpected Fruit Jam DVI D1 N pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D1P_PIN == 17, "unexpected Fruit Jam DVI D1 P pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D2N_PIN == 18, "unexpected Fruit Jam DVI D2 N pin");
_Static_assert(ADAFRUIT_FRUIT_JAM_DVI_D2P_PIN == 19, "unexpected Fruit Jam DVI D2 P pin");

enum {
    NEO1_VIDEO_SYS_CLOCK_KHZ = 126000,
    NEO1_VIDEO_H_ACTIVE = 640,
    NEO1_VIDEO_H_FRONT_PORCH = 16,
    NEO1_VIDEO_H_SYNC_WIDTH = 96,
    NEO1_VIDEO_H_BACK_PORCH = 48,
    NEO1_VIDEO_V_ACTIVE = 480,
    NEO1_VIDEO_V_FRONT_PORCH = 10,
    NEO1_VIDEO_V_SYNC_WIDTH = 2,
    NEO1_VIDEO_V_BACK_PORCH = 33,
    NEO1_VIDEO_V_TOTAL = NEO1_VIDEO_V_FRONT_PORCH +
                         NEO1_VIDEO_V_SYNC_WIDTH +
                         NEO1_VIDEO_V_BACK_PORCH +
                         NEO1_VIDEO_V_ACTIVE,
    NEO1_VIDEO_V_ACTIVE_START = NEO1_VIDEO_V_TOTAL - NEO1_VIDEO_V_ACTIVE,
    NEO1_VIDEO_FONT_WIDTH = 8,
    NEO1_VIDEO_FONT_HEIGHT = 8,
    NEO1_VIDEO_CELL_WIDTH = 16,
    NEO1_VIDEO_CELL_HEIGHT = 16,
    NEO1_VIDEO_TEXT_HEIGHT = NEO1_TERM_ROWS * NEO1_VIDEO_CELL_HEIGHT,
    NEO1_VIDEO_TEXT_TOP = (NEO1_VIDEO_V_ACTIVE - NEO1_VIDEO_TEXT_HEIGHT) / 2,
    NEO1_VIDEO_RASTER_LINES = NEO1_TERM_ROWS * NEO1_VIDEO_FONT_HEIGHT,
    NEO1_VIDEO_CURSOR_BLINK_FRAMES = 30,
};

_Static_assert(NEO1_TERM_COLS * NEO1_VIDEO_CELL_WIDTH == NEO1_VIDEO_H_ACTIVE,
               "40-column terminal must fill the active scanline");

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define HSTX_CMD_RAW_REPEAT (0x1u << 12)
#define HSTX_CMD_TMDS       (0x2u << 12)
#define HSTX_CMD_NOP        (0xfu << 12)

// The NOP padding keeps each command list at least as deep as the HSTX FIFO,
// avoiding a rapid DMA ping-pong race at the end of short blanking lists.
static uint32_t g_vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (NEO1_VIDEO_H_BACK_PORCH + NEO1_VIDEO_H_ACTIVE),
    SYNC_V1_H1,
    HSTX_CMD_NOP,
};

static uint32_t g_vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (NEO1_VIDEO_H_BACK_PORCH + NEO1_VIDEO_H_ACTIVE),
    SYNC_V0_H1,
    HSTX_CMD_NOP,
};

static uint32_t g_vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | NEO1_VIDEO_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS | NEO1_VIDEO_H_ACTIVE,
};

// Terminal publication mirrors the proven Pico renderer's three-buffer model:
// core 0 copies into a producer buffer without holding the publication lock,
// then publishes only an index. Core 1 accepts that index under the short lock
// and renders from an immutable front buffer. A terminal-sized memcpy must
// never run with the HSTX DMA IRQ disabled.
static critical_section_t g_terminal_publication_lock;
static semaphore_t g_video_ready;
static neo1_terminal_t g_terminal_buffers[3];
static volatile uint32_t g_front_terminal_index;
static uint32_t g_producer_terminal_index;
static volatile uint32_t g_pending_terminal_index;
static volatile bool g_has_pending_terminal;

// Each raster row is already expanded to the 640 RGB332 bytes consumed by
// HSTX. Active scanlines repeat each row twice for 16-pixel-high cells.
static uint32_t
    g_rasters[2][NEO1_VIDEO_RASTER_LINES][NEO1_VIDEO_H_ACTIVE / sizeof(uint32_t)];
static volatile uint32_t g_active_raster;
static volatile uint32_t g_pending_raster;
static volatile bool g_raster_pending;
static volatile bool g_cursor_refresh;
static volatile uint32_t g_frame_counter;

static uint32_t g_pixel_expand[256][4];
static uint32_t g_blank_line[NEO1_VIDEO_H_ACTIVE / sizeof(uint32_t)];

static int g_dma_ping;
static int g_dma_pong;
static bool g_next_completed_is_pong;
static uint32_t g_v_scanline;
static bool g_vactive_cmdlist_posted;

static void neo1_video_build_expansion_table(void) {
    // RGB332 stores green in bits 2..4 with the HSTX configuration below.
    const uint8_t foreground = 0x1Cu;
    for (uint32_t bits = 0; bits < 256; ++bits) {
        uint8_t* pixels = (uint8_t*)g_pixel_expand[bits];
        for (uint32_t pixel = 0; pixel < NEO1_VIDEO_FONT_WIDTH; ++pixel) {
            const uint8_t color = (bits & (1u << pixel)) ? foreground : 0;
            pixels[pixel * 2] = color;
            pixels[pixel * 2 + 1] = color;
        }
    }
}

static void neo1_video_render_raster(uint32_t raster_index,
                                     const neo1_terminal_t* term,
                                     bool cursor_on) {
    for (uint32_t row = 0; row < NEO1_TERM_ROWS; ++row) {
        for (uint32_t glyph_row = 0; glyph_row < NEO1_VIDEO_FONT_HEIGHT; ++glyph_row) {
            uint32_t* dst = g_rasters[raster_index]
                                     [row * NEO1_VIDEO_FONT_HEIGHT + glyph_row];
            for (uint32_t col = 0; col < NEO1_TERM_COLS; ++col) {
                uint8_t ch = term->chars[row][col] & 0x7Fu;
                if (cursor_on && term->cursor_x == col && term->cursor_y == row) {
                    ch = '@';
                }
                const uint8_t bits = apple1_vid[
                    (uint32_t)ch * NEO1_VIDEO_FONT_HEIGHT + glyph_row];
                const uint32_t* expanded = g_pixel_expand[bits];
                dst[col * 4u + 0u] = expanded[0];
                dst[col * 4u + 1u] = expanded[1];
                dst[col * 4u + 2u] = expanded[2];
                dst[col * 4u + 3u] = expanded[3];
            }
        }
    }
}

static const uint32_t* __not_in_flash_func(neo1_video_pixels_for_line)(
    uint32_t active_line) {
    if (active_line < NEO1_VIDEO_TEXT_TOP ||
        active_line >= NEO1_VIDEO_TEXT_TOP + NEO1_VIDEO_TEXT_HEIGHT) {
        return g_blank_line;
    }

    const uint32_t raster_line =
        (active_line - NEO1_VIDEO_TEXT_TOP) / 2u;
    return g_rasters[g_active_raster][raster_line];
}

static void __not_in_flash_func(neo1_hstx_dma_irq_handler)(void) {
    const int channel = g_next_completed_is_pong ? g_dma_pong : g_dma_ping;
    dma_channel_hw_t* channel_hw = &dma_hw->ch[channel];
    dma_hw->intr = 1u << (uint32_t)channel;
    g_next_completed_is_pong = !g_next_completed_is_pong;

    if (g_v_scanline >= NEO1_VIDEO_V_FRONT_PORCH &&
        g_v_scanline < NEO1_VIDEO_V_FRONT_PORCH + NEO1_VIDEO_V_SYNC_WIDTH) {
        channel_hw->read_addr = (uintptr_t)g_vblank_line_vsync_on;
        channel_hw->transfer_count = count_of(g_vblank_line_vsync_on);
    } else if (g_v_scanline < NEO1_VIDEO_V_ACTIVE_START) {
        channel_hw->read_addr = (uintptr_t)g_vblank_line_vsync_off;
        channel_hw->transfer_count = count_of(g_vblank_line_vsync_off);
    } else if (!g_vactive_cmdlist_posted) {
        channel_hw->read_addr = (uintptr_t)g_vactive_line;
        channel_hw->transfer_count = count_of(g_vactive_line);
        g_vactive_cmdlist_posted = true;
    } else {
        const uint32_t active_line = g_v_scanline - NEO1_VIDEO_V_ACTIVE_START;
        channel_hw->read_addr =
            (uintptr_t)neo1_video_pixels_for_line(active_line);
        channel_hw->transfer_count = NEO1_VIDEO_H_ACTIVE / sizeof(uint32_t);
        g_vactive_cmdlist_posted = false;
    }

    if (!g_vactive_cmdlist_posted) {
        g_v_scanline = (g_v_scanline + 1u) % NEO1_VIDEO_V_TOTAL;
        if (g_v_scanline == 0) {
            ++g_frame_counter;
            if (g_raster_pending) {
                g_active_raster = g_pending_raster;
                g_raster_pending = false;
            }
            if ((g_frame_counter % NEO1_VIDEO_CURSOR_BLINK_FRAMES) == 0) {
                g_cursor_refresh = true;
                __sev();
            }
        }
    }
}

static void neo1_video_configure_hstx(void) {
    // Configure the HSTX hardware TMDS encoder for RGB332 pixels.
    hstx_ctrl_hw->expand_tmds =
        2u << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
        0u << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
        2u << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
        29u << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
        1u << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
        26u << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

    hstx_ctrl_hw->expand_shift =
        4u << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        8u << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1u << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0u << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // Output bit n is GPIO12+n. Fruit Jam connects each negative half first.
    hstx_ctrl_hw->bit[0] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    hstx_ctrl_hw->bit[1] = HSTX_CTRL_BIT0_CLK_BITS;
    const uint32_t lane_to_output_bit[3] = {2u, 4u, 6u};
    for (uint32_t lane = 0; lane < 3; ++lane) {
        const uint32_t bit = lane_to_output_bit[lane];
        const uint32_t select =
            (lane * 10u) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10u + 1u) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit] = select | HSTX_CTRL_BIT0_INV_BITS;
        hstx_ctrl_hw->bit[bit + 1u] = select;
    }

    for (uint32_t pin = ADAFRUIT_FRUIT_JAM_DVI_CKN_PIN;
         pin <= ADAFRUIT_FRUIT_JAM_DVI_D2P_PIN;
         ++pin) {
        gpio_set_function(pin, GPIO_FUNC_HSTX);
    }
}

static void neo1_video_configure_dma(void) {
    g_dma_ping = dma_claim_unused_channel(true);
    g_dma_pong = dma_claim_unused_channel(true);

    dma_channel_config config = dma_channel_get_default_config((uint)g_dma_ping);
    channel_config_set_chain_to(&config, (uint)g_dma_pong);
    channel_config_set_dreq(&config, DREQ_HSTX);
    dma_channel_configure((uint)g_dma_ping,
                          &config,
                          &hstx_fifo_hw->fifo,
                          g_vblank_line_vsync_off,
                          count_of(g_vblank_line_vsync_off),
                          false);

    config = dma_channel_get_default_config((uint)g_dma_pong);
    channel_config_set_chain_to(&config, (uint)g_dma_ping);
    channel_config_set_dreq(&config, DREQ_HSTX);
    dma_channel_configure((uint)g_dma_pong,
                          &config,
                          &hstx_fifo_hw->fifo,
                          g_vblank_line_vsync_off,
                          count_of(g_vblank_line_vsync_off),
                          false);

    dma_channel_set_irq0_enabled((uint)g_dma_ping, true);
    dma_channel_set_irq0_enabled((uint)g_dma_pong, true);
    irq_set_exclusive_handler(DMA_IRQ_0, neo1_hstx_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // DVI scanout is continuous; prioritize DMA reads/writes over processors.
    hw_set_bits(&bus_ctrl_hw->priority,
                BUSCTRL_BUS_PRIORITY_DMA_R_BITS | BUSCTRL_BUS_PRIORITY_DMA_W_BITS);
    dma_channel_start((uint)g_dma_ping);
}

static bool neo1_video_accept_terminal(void) {
    bool accepted = false;
    critical_section_enter_blocking(&g_terminal_publication_lock);
    if (g_has_pending_terminal) {
        g_front_terminal_index = g_pending_terminal_index;
        g_has_pending_terminal = false;
        accepted = true;
    }
    critical_section_exit(&g_terminal_publication_lock);
    return accepted;
}

static void __not_in_flash_func(neo1_hstx_video_core1)(void) {
    neo1_video_configure_hstx();
    neo1_video_configure_dma();
    sem_release(&g_video_ready);

    while (true) {
        const bool terminal_changed = neo1_video_accept_terminal();

        const uint32_t interrupt_state = save_and_disable_interrupts();
        const bool cursor_changed = g_cursor_refresh;
        g_cursor_refresh = false;
        restore_interrupts(interrupt_state);

        if (terminal_changed || cursor_changed) {
            const uint32_t state = save_and_disable_interrupts();
            const uint32_t target = 1u - g_active_raster;
            g_raster_pending = false;
            const bool cursor_on =
                ((g_frame_counter / NEO1_VIDEO_CURSOR_BLINK_FRAMES) & 1u) == 0u;
            restore_interrupts(state);

            neo1_video_render_raster(
                target,
                &g_terminal_buffers[g_front_terminal_index],
                cursor_on);

            const uint32_t publish_state = save_and_disable_interrupts();
            g_pending_raster = target;
            g_raster_pending = true;
            restore_interrupts(publish_state);
        } else {
            __wfe();
        }
    }
}

bool neo1_hstx_video_init(const neo1_terminal_t* term) {
    if (!term || !set_sys_clock_khz(NEO1_VIDEO_SYS_CLOCK_KHZ, true)) {
        return false;
    }

    critical_section_init(&g_terminal_publication_lock);
    sem_init(&g_video_ready, 0, 1);
    memset(g_rasters, 0, sizeof(g_rasters));
    memset(g_blank_line, 0, sizeof(g_blank_line));
    memset(g_terminal_buffers, 0, sizeof(g_terminal_buffers));
    memcpy(&g_terminal_buffers[0], term, sizeof(g_terminal_buffers[0]));
    g_front_terminal_index = 0;
    g_producer_terminal_index = 1;
    g_pending_terminal_index = 0;
    g_has_pending_terminal = false;
    g_active_raster = 0;
    g_pending_raster = 0;
    g_raster_pending = false;
    g_cursor_refresh = false;
    g_frame_counter = 0;
    g_next_completed_is_pong = false;
    g_v_scanline = 2;
    g_vactive_cmdlist_posted = false;

    neo1_video_build_expansion_table();
    neo1_video_render_raster(0, term, true);

    multicore_launch_core1(neo1_hstx_video_core1);
    sem_acquire_blocking(&g_video_ready);
    return true;
}

void neo1_hstx_video_set_terminal(const neo1_terminal_t* term) {
    if (!term) {
        return;
    }

    const uint32_t published_index = g_producer_terminal_index;
    memcpy(&g_terminal_buffers[published_index],
           term,
           sizeof(g_terminal_buffers[published_index]));

    critical_section_enter_blocking(&g_terminal_publication_lock);
    uint32_t next_producer_index;
    if (g_has_pending_terminal) {
        next_producer_index = g_pending_terminal_index;
    } else {
        next_producer_index =
            3u - g_front_terminal_index - published_index;
    }
    g_pending_terminal_index = published_index;
    g_has_pending_terminal = true;
    g_producer_terminal_index = next_producer_index;
    critical_section_exit(&g_terminal_publication_lock);
    __sev();
}
