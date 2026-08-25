#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHIPS_IMPL

#include "chips/chips_common.h"
#include "chips/neo1_cpu_backend.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "devices/neo1_msc.h"
#include "systems/neo1.h"

static unsigned g_msc_reads;
static unsigned g_msc_writes;
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

static uint8_t test_msc_read(void* user_data, uint16_t addr) {
    CHECK(user_data == &g_msc_reads);
    g_msc_reads++;
    return (uint8_t)(0xA5u ^ (uint8_t)addr);
}

static void test_msc_write(void* user_data, uint16_t addr, uint8_t data) {
    CHECK(user_data == &g_msc_reads);
    g_msc_writes++;
    g_last_write_addr = addr;
    g_last_write_data = data;
}

int main(void) {
    uint8_t rom[0x100];
    memset(rom, 0xEA, sizeof(rom));
    rom[0xFC] = 0x00;
    rom[0xFD] = 0xFF;

    const neo1_desc_t desc = {
        .roms.rom = {
            .ptr = rom,
            .size = sizeof(rom),
        },
#if NEO1_ENABLE_MSC
        .devices.msc = {
            .read = test_msc_read,
            .write = test_msc_write,
            .user_data = &g_msc_reads,
        },
#endif
    };
    neo1_t machine;
    neo1_init(&machine, &desc);

    machine.ram[NEO1_IO_MSC_STATUS] = 0x3C;
    machine.ram[0xD01D] = 0x6C;
    const uint8_t status = neo1_soft65c02_mem_read(&machine, NEO1_IO_MSC_STATUS);
    neo1_soft65c02_mem_write(&machine, NEO1_IO_MSC_CMD, 0x5A);
    const uint8_t outside = neo1_soft65c02_mem_read(&machine, 0xD01D);

#if NEO1_ENABLE_MSC
    CHECK(status == (uint8_t)(0xA5u ^ (uint8_t)NEO1_IO_MSC_STATUS));
    CHECK(g_msc_reads == 1);
    CHECK(g_msc_writes == 1);
    CHECK(g_last_write_addr == NEO1_IO_MSC_CMD);
    CHECK(g_last_write_data == 0x5A);
    CHECK(machine.ram[NEO1_IO_MSC_CMD] == 0x00);
#else
    CHECK(status == 0x3C);
    CHECK(g_msc_reads == 0);
    CHECK(g_msc_writes == 0);
    CHECK(machine.ram[NEO1_IO_MSC_CMD] == 0x5A);
#endif
    CHECK(outside == 0x6C);

    neo1_discard(&machine);
    if (g_failures != 0) {
        fprintf(stderr, "neo1_msc_decode_tests: %d failure(s)\n", g_failures);
        return 1;
    }

    puts(NEO1_ENABLE_MSC
             ? "neo1_msc_decode_tests: enabled decode routed to device"
             : "neo1_msc_decode_tests: disabled decode fell through to RAM");
    return 0;
}
