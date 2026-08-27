#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "devices/neo1_cffa1.h"
#include "systems/neo1_machine.h"

static unsigned g_reads;
static unsigned g_writes;
static uint16_t g_last_write_addr;
static uint8_t g_last_write_data;
static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static uint8_t test_cffa1_read(void* user_data, uint16_t addr) {
    CHECK(user_data == &g_reads);
    g_reads++;
    return (uint8_t)(0x5Au ^ (uint8_t)addr);
}

static void test_cffa1_write(void* user_data, uint16_t addr, uint8_t data) {
    CHECK(user_data == &g_reads);
    g_writes++;
    g_last_write_addr = addr;
    g_last_write_data = data;
}

int main(void) {
    uint8_t rom[0x100];
    memset(rom, 0xEA, sizeof(rom));
    rom[0xFC] = 0x00;
    rom[0xFD] = 0xFF;

    const neo1_profile_t profile = {
        .personality = NEO1_PERSONALITY_50,
        .rom = rom,
        .rom_size = sizeof(rom),
        .rom_base = 0xFF00,
        .rom_protect_base = 0xFF00,
    };
    const neo1_machine_desc_t desc = {
        .profile = &profile,
#if NEO1_ENABLE_VCFFA1
        .vcffa1 = {
            .read = test_cffa1_read,
            .write = test_cffa1_write,
            .user_data = &g_reads,
        },
#endif
    };
    neo1_machine_t machine;
    CHECK(neo1_machine_init(&machine, &desc));

    machine.ram[NEO1_CFFA1_ID1_ADDR] = 0x31;
    machine.ram[NEO1_CFFA1_IO_BASE] = 0x32;
    machine.ram[0xAFDE] = 0x33;

    const uint8_t signature = neo1_machine_read(&machine, NEO1_CFFA1_ID1_ADDR);
    const uint8_t reg = neo1_machine_read(&machine, NEO1_CFFA1_IO_BASE);
    neo1_machine_write(&machine, NEO1_CFFA1_IO_END, 0xA6);
    const uint8_t outside = neo1_machine_read(&machine, 0xAFDE);

#if NEO1_ENABLE_VCFFA1
    CHECK(signature == (uint8_t)(0x5Au ^ (uint8_t)NEO1_CFFA1_ID1_ADDR));
    CHECK(reg == (uint8_t)(0x5Au ^ (uint8_t)NEO1_CFFA1_IO_BASE));
    CHECK(g_reads == 2);
    CHECK(g_writes == 1);
    CHECK(g_last_write_addr == NEO1_CFFA1_IO_END);
    CHECK(g_last_write_data == 0xA6);
    CHECK(machine.ram[NEO1_CFFA1_IO_END] != 0xA6);
#else
    CHECK(signature == 0x31);
    CHECK(reg == 0x32);
    CHECK(g_reads == 0);
    CHECK(g_writes == 0);
    CHECK(machine.ram[NEO1_CFFA1_IO_END] == 0xA6);
#endif
    CHECK(outside == 0x33);

    if (g_failures != 0) {
        fprintf(stderr, "neo1_cffa1_decode_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts(NEO1_ENABLE_VCFFA1
             ? "neo1_cffa1_decode_tests: enabled decode routed to device"
             : "neo1_cffa1_decode_tests: disabled decode fell through to RAM");
    return 0;
}
