#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHIPS_IMPL

#include "chips/chips_common.h"
#include "chips/neo1_cpu_backend.h"
#include "chips/clk.h"
#include "devices/neo1_apple1_pia.h"
#include "systems/neo1.h"

static unsigned g_display_count;
static uint8_t g_display_byte;
static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static void capture_display(uint8_t ch, void* user_data) {
    CHECK(user_data == &g_display_count);
    g_display_count++;
    g_display_byte = ch;
}

static void test_register_contract(void) {
    neo1_apple1_pia_t pia;
    memset(&pia, 0xA5, sizeof(pia));
    neo1_apple1_pia_reset(&pia);

    CHECK(pia.keyboard_latch == 0);
    CHECK(pia.keyboard_control == 0);
    CHECK(pia.keyboard_ddr == 0);
    CHECK(pia.keyboard_data == 0);
    CHECK(pia.display_control == 0);
    CHECK(pia.display_ddr == 0);
    CHECK(pia.display_data == 0);
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBDCR) == 0x00);
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_DSPCR) == 0x80);

    uint8_t emitted = 0;
    CHECK(!neo1_apple1_pia_write(&pia, NEO1_IO_KBD, 0x5A, &emitted));
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBD) == 0x5A);

    neo1_apple1_pia_key_down(&pia, '\n');
    neo1_apple1_pia_key_down(&pia, 'X');
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBDCR) == 0x80);
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBD) == 0x5A);
    CHECK(pia.keyboard_latch == (uint8_t)('\r' | 0x80));

    CHECK(!neo1_apple1_pia_write(&pia, NEO1_IO_KBDCR, 0x04, &emitted));
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBD) == (uint8_t)('\r' | 0x80));
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBDCR) == 0x04);

    neo1_apple1_pia_key_down(&pia, 'A');
    neo1_apple1_pia_key_down(&pia, 'B');
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_KBD) == (uint8_t)('A' | 0x80));
    CHECK(!neo1_apple1_pia_write(&pia, NEO1_IO_KBD, 0x37, &emitted));
    CHECK(pia.keyboard_data == 0x37);

    CHECK(!neo1_apple1_pia_write(&pia, NEO1_IO_DSP, 0x3C, &emitted));
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_DSP) == 0x3C);
    CHECK(!neo1_apple1_pia_write(&pia, NEO1_IO_DSPCR_ALT, 0x04, &emitted));
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_DSPCR) == 0x84);
    CHECK(neo1_apple1_pia_read(&pia, NEO1_IO_DSP_ALT) == 0x00);
    CHECK(neo1_apple1_pia_write(&pia, NEO1_IO_DSP_ALT, 0xA5, &emitted));
    CHECK(emitted == 0xA5);
    CHECK(pia.display_data == 0xA5);

    CHECK(neo1_apple1_pia_handles_addr(NEO1_IO_KBD));
    CHECK(neo1_apple1_pia_handles_addr(NEO1_IO_DSPCR_ALT));
    CHECK(!neo1_apple1_pia_handles_addr(0xD00F));
    CHECK(!neo1_apple1_pia_handles_addr(0xD014));
    CHECK(!neo1_apple1_pia_handles_addr(0xD0F1));
    CHECK(!neo1_apple1_pia_handles_addr(0xD0F4));
}

static void test_machine_routing(void) {
    uint8_t rom[0x100];
    memset(rom, 0xEA, sizeof(rom));
    rom[0xFC] = 0x00;
    rom[0xFD] = 0xFF;

    const neo1_desc_t desc = {
        .roms.rom = {
            .ptr = rom,
            .size = sizeof(rom),
        },
        .char_out = {
            .func = capture_display,
            .user_data = &g_display_count,
        },
    };
    neo1_t machine;
    neo1_init(&machine, &desc);

    neo1_memory(&machine)[0xD00F] = 0x21;
    neo1_memory(&machine)[0xD014] = 0x22;
    neo1_memory(&machine)[0xD0F1] = 0x23;
    neo1_memory(&machine)[0xD0F4] = 0x24;
    CHECK(neo1_bus_read(&machine, 0xD00F) == 0x21);
    CHECK(neo1_bus_read(&machine, 0xD014) == 0x22);
    CHECK(neo1_bus_read(&machine, 0xD0F1) == 0x23);
    CHECK(neo1_bus_read(&machine, 0xD0F4) == 0x24);

    neo1_bus_write(&machine, NEO1_IO_DSPCR, 0x04);
    neo1_bus_write(&machine, NEO1_IO_DSP_ALT, 0xD1);
    CHECK(g_display_count == 1);
    CHECK(g_display_byte == 0xD1);

    neo1_key_down(&machine, 'C');
    neo1_key_down(&machine, 'D');
    neo1_bus_write(&machine, NEO1_IO_KBDCR, 0x04);
    CHECK(neo1_bus_read(&machine, NEO1_IO_KBD) == (uint8_t)('C' | 0x80));
    CHECK(neo1_bus_read(&machine, NEO1_IO_KBDCR) == 0x04);

    neo1_bus_write(&machine, NEO1_IO_DSPCR, 0x7F);
    neo1_key_down(&machine, 'E');
    neo1_reset(&machine);
    CHECK(machine.machine.pia.keyboard_latch == 0);
    CHECK(machine.machine.pia.keyboard_control == 0);
    CHECK(machine.machine.pia.display_control == 0);

    neo1_discard(&machine);
}

int main(void) {
    test_register_contract();
    test_machine_routing();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_apple1_pia_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_apple1_pia_tests: register, latch, mirror, and routing contracts passed");
    return 0;
}
