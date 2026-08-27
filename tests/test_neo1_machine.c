#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "systems/neo1_machine.h"

typedef struct {
    unsigned reads;
    unsigned writes;
    uint16_t last_addr;
    uint8_t last_data;
} port_fixture_t;

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

static uint8_t port_read(void* user_data, uint16_t addr) {
    port_fixture_t* fixture = (port_fixture_t*)user_data;
    fixture->reads++;
    fixture->last_addr = addr;
    return (uint8_t)(0xA5u ^ (uint8_t)addr);
}

static void port_write(void* user_data, uint16_t addr, uint8_t data) {
    port_fixture_t* fixture = (port_fixture_t*)user_data;
    fixture->writes++;
    fixture->last_addr = addr;
    fixture->last_data = data;
}

static void display_out(uint8_t ch, void* user_data) {
    CHECK(user_data == &g_display_count);
    g_display_count++;
    g_display_byte = ch;
}

static void test_neo1_23_space(void) {
    static uint8_t rom[0x2000];
    for (size_t i = 0; i < sizeof(rom); i++) {
        rom[i] = (uint8_t)(i ^ 0x5A);
    }

    port_fixture_t msc = {0};
    port_fixture_t vcffa1 = {0};
    const neo1_profile_t profile = {
        .personality = NEO1_PERSONALITY_23,
        .rom = rom,
        .rom_size = sizeof(rom),
        .rom_base = 0xE000,
        .rom_protect_base = 0xE000,
    };
    const neo1_machine_desc_t desc = {
        .profile = &profile,
        .char_out = display_out,
        .char_out_user_data = &g_display_count,
        .msc = {
            .read = port_read,
            .write = port_write,
            .user_data = &msc,
        },
        .vcffa1 = {
            .read = port_read,
            .write = port_write,
            .user_data = &vcffa1,
        },
    };
    neo1_machine_t machine;
    CHECK(neo1_machine_init(&machine, &desc));

    CHECK(machine.ram[0x0000] == 0x00);
    CHECK(machine.ram[0x0001] == 0xFF);
    CHECK(machine.ram[0xDFFE] == 0x00);
    CHECK(machine.ram[0xDFFF] == 0xFF);
    CHECK(machine.ram[0xE000] == rom[0]);
    CHECK(machine.ram[0xFFFC] == rom[0x1FFC]);
    CHECK(machine.ram[0xFFFD] == rom[0x1FFD]);
    CHECK(machine.ram[0xFFFF] == rom[0x1FFF]);

    neo1_machine_write(&machine, 0xDFFF, 0x31);
    neo1_machine_write(&machine, 0xE000, 0x32);
    neo1_machine_write(&machine, 0xFFFF, 0x33);
    CHECK(neo1_machine_read(&machine, 0xDFFF) == 0x31);
    CHECK(machine.ram[0xE000] == rom[0]);
    CHECK(machine.ram[0xFFFF] == rom[0x1FFF]);

    CHECK(neo1_machine_read(&machine, NEO1_IO_MSC_STATUS) ==
          (uint8_t)(0xA5u ^ (uint8_t)NEO1_IO_MSC_STATUS));
    neo1_machine_write(&machine, NEO1_IO_MSC_CMD, 0x41);
    CHECK(msc.reads == 1);
    CHECK(msc.writes == 1);
    CHECK(msc.last_addr == NEO1_IO_MSC_CMD);
    CHECK(msc.last_data == 0x41);

    CHECK(neo1_machine_read(&machine, NEO1_CFFA1_ID1_ADDR) ==
          (uint8_t)(0xA5u ^ (uint8_t)NEO1_CFFA1_ID1_ADDR));
    neo1_machine_write(&machine, NEO1_CFFA1_IO_END, 0x42);
    CHECK(vcffa1.reads == 1);
    CHECK(vcffa1.writes == 1);
    CHECK(vcffa1.last_addr == NEO1_CFFA1_IO_END);
    CHECK(vcffa1.last_data == 0x42);

    machine.ram[0xAFDE] = 0x51;
    machine.ram[0xD01D] = 0x52;
    CHECK(neo1_machine_read(&machine, 0xAFDE) == 0x51);
    CHECK(neo1_machine_read(&machine, 0xD01D) == 0x52);
    CHECK(vcffa1.reads == 1);
    CHECK(msc.reads == 1);

    neo1_machine_write(&machine, NEO1_IO_DSPCR, 0x04);
    neo1_machine_write(&machine, NEO1_IO_DSP, 0xD4);
    CHECK(g_display_count == 1);
    CHECK(g_display_byte == 0xD4);

    neo1_machine_key_down(&machine, 'K');
    machine.ram[0x0300] = 0x77;
    neo1_machine_reset(&machine);
    CHECK(machine.pia.keyboard_latch == 0);
    CHECK(machine.pia.keyboard_control == 0);
    CHECK(machine.pia.display_control == 0);
    CHECK(machine.ram[0x0300] == 0x77);
    CHECK(machine.ram[0xE000] == rom[0]);
}

static void test_neo1_50_space_and_disabled_devices(void) {
    uint8_t rom[0x100];
    for (size_t i = 0; i < sizeof(rom); i++) {
        rom[i] = (uint8_t)(0xC0u ^ (uint8_t)i);
    }

    const neo1_profile_t profile = {
        .personality = NEO1_PERSONALITY_50,
        .rom = rom,
        .rom_size = sizeof(rom),
        .rom_base = 0xFF00,
        .rom_protect_base = 0xFF00,
    };
    const neo1_machine_desc_t desc = {
        .profile = &profile,
    };
    neo1_machine_t machine;
    CHECK(neo1_machine_init(&machine, &desc));

    neo1_machine_write(&machine, 0xE000, 0x61);
    neo1_machine_write(&machine, 0xF000, 0x62);
    neo1_machine_write(&machine, 0xFEFF, 0x63);
    neo1_machine_write(&machine, 0xFF00, 0x64);
    CHECK(neo1_machine_read(&machine, 0xE000) == 0x61);
    CHECK(neo1_machine_read(&machine, 0xF000) == 0x62);
    CHECK(neo1_machine_read(&machine, 0xFEFF) == 0x63);
    CHECK(machine.ram[0xFF00] == rom[0]);
    CHECK(machine.ram[0xFFFC] == rom[0xFC]);
    CHECK(machine.ram[0xFFFD] == rom[0xFD]);
    CHECK(machine.ram[0xFFFF] == rom[0xFF]);

    neo1_machine_write(&machine, NEO1_IO_MSC_CMD, 0x71);
    neo1_machine_write(&machine, NEO1_CFFA1_IO_END, 0x72);
    CHECK(neo1_machine_read(&machine, NEO1_IO_MSC_CMD) == 0x71);
    CHECK(neo1_machine_read(&machine, NEO1_CFFA1_IO_END) == 0x72);
}

int main(void) {
    test_neo1_23_space();
    test_neo1_50_space_and_disabled_devices();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_machine_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_machine_tests: CPU-neutral RAM, ROM, PIA, and optional decode passed");
    return 0;
}
