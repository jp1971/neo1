#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "neo1_storage_stub.h"

static uint8_t g_disk[2][NEO1_MSC_SECTOR_SIZE];
static unsigned g_reads;
static unsigned g_writes;
static bool g_media_ready;
static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

bool neo1_platform_disk_read(uint32_t lba, uint8_t* buffer, uint32_t count) {
    if (!g_media_ready || !buffer || count != 1 || lba >= 2) {
        return false;
    }
    memcpy(buffer, g_disk[lba], NEO1_MSC_SECTOR_SIZE);
    g_reads++;
    return true;
}

bool neo1_platform_disk_write(uint32_t lba,
                              const uint8_t* buffer,
                              uint32_t count) {
    if (!g_media_ready || !buffer || count != 1 || lba >= 2) {
        return false;
    }
    memcpy(g_disk[lba], buffer, NEO1_MSC_SECTOR_SIZE);
    g_writes++;
    return true;
}

static void reset_fixture(void) {
    memset(g_disk, 0, sizeof(g_disk));
    g_reads = 0;
    g_writes = 0;
    g_media_ready = true;
}

static void test_raw_sector_backend(void) {
    reset_fixture();
    for (size_t i = 0; i < NEO1_MSC_SECTOR_SIZE; i++) {
        g_disk[1][i] = (uint8_t)(i ^ 0xA5u);
    }

    neo1_msc_t msc;
    CHECK(neo1_msc_init(&msc, neo1_sdl_msc_backend()));
    neo1_msc_write(&msc, NEO1_IO_MSC_SECTOR_LO, 1);
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_READY);
    CHECK(g_reads == 1);
    for (size_t i = 0; i < NEO1_MSC_SECTOR_SIZE; i++) {
        CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_DATA) ==
              (uint8_t)(i ^ 0xA5u));
    }

    CHECK(neo1_msc_init(&msc, neo1_sdl_msc_backend()));
    neo1_msc_write(&msc, NEO1_IO_MSC_SECTOR_LO, 1);
    neo1_msc_write(&msc, NEO1_IO_MSC_SIZE_LO, 16);
    for (uint8_t value = 0; value < 16; value++) {
        neo1_msc_write(&msc, NEO1_IO_MSC_DATA, (uint8_t)(0x40u + value));
    }
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_READY);
    CHECK(g_writes == 1);
    CHECK(g_disk[1][0] == 0x40);
    CHECK(g_disk[1][15] == 0x4F);
    CHECK(g_disk[1][16] == 0);

    g_media_ready = false;
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) ==
          (NEO1_MSC_STATUS_ERROR | 1u));
}

static void test_shared_command_convergence(void) {
    reset_fixture();
    neo1_msc_t msc;
    CHECK(neo1_msc_init(&msc, neo1_sdl_msc_backend()));

    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, 0xFF);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) ==
          (NEO1_MSC_STATUS_ERROR | 1u));

    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_OPEN);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_BUSY);
    neo1_msc_write(&msc, NEO1_IO_MSC_DATA, 0);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_READY);

    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_OPEN);
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_NEXT);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_READY);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_INFO) == 0);

    neo1_msc_write(&msc, NEO1_IO_MSC_INDEX, 0);
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_OPEN_INDEX);
    CHECK(neo1_msc_read(&msc, NEO1_IO_MSC_STATUS) ==
          (NEO1_MSC_STATUS_ERROR | 1u));

    CHECK(neo1_msc_init(&msc, neo1_sdl_msc_backend()));
    neo1_msc_write(&msc, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(g_writes == 1);
    for (size_t i = 0; i < NEO1_MSC_SECTOR_SIZE; i++) {
        neo1_msc_write(&msc, NEO1_IO_MSC_DATA, 0x77);
    }
    CHECK(g_writes == 1);
}

static void test_vcffa1_remains_separate(void) {
    reset_fixture();
    neo1_cffa1_init();
    CHECK(neo1_cffa1_io_read(NEO1_CFFA1_ID1_ADDR) == NEO1_CFFA1_ID1_VALUE);
    CHECK(neo1_cffa1_io_read(NEO1_CFFA1_ID2_ADDR) == NEO1_CFFA1_ID2_VALUE);
    neo1_cffa1_io_write(NEO1_CFFA1_IO_BASE + NEO1_CFFA1_REG_STATUS_COMMAND,
                        NEO1_CFFA1_CMD_PRODOS_STATUS);
    CHECK(neo1_cffa1_io_read(NEO1_CFFA1_IO_BASE +
                             NEO1_CFFA1_REG_STATUS_COMMAND) ==
          (NEO1_CFFA1_STATUS_DRDY | NEO1_CFFA1_STATUS_DSC));
}

int main(void) {
    test_raw_sector_backend();
    test_shared_command_convergence();
    test_vcffa1_remains_separate();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_sdl_storage_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_sdl_storage_tests: raw MSC backend and VCFFA1 separation passed");
    return 0;
}
