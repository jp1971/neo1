#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "neo1_vaci_v1.h"

#define FAKE6502_USE_STDINT

uint8_t read6502(uint16_t address);
void write6502(uint16_t address, uint8_t value);

#include "chips/fake65c02.h"

#define MSC_CMD      0xD014u
#define MSC_SECT_LO  0xD015u
#define MSC_SECT_HI  0xD016u
#define MSC_DATA     0xD017u
#define MSC_STATUS   0xD018u
#define MSC_INDEX    0xD019u
#define MSC_INFO     0xD01Au
#define MSC_SIZE_LO  0xD01Bu
#define MSC_SIZE_HI  0xD01Cu
#define KBD           0xD010u
#define KBDCR         0xD011u
#define DSP           0xD012u

#define CMD_OPEN      0x01u
#define CMD_CLOSE     0x02u
#define CMD_READ      0x03u
#define CMD_WRITE     0x04u
#define CMD_DIR_OPEN  0x10u
#define CMD_DIR_NEXT  0x11u
#define CMD_OPEN_IND  0x12u

#define BASIC_ZP_START  0x004Au
#define BASIC_ZP_SIZE   182u
#define BASIC_MEM_START 0x0800u
#define BASIC_MEM_SIZE  2048u
#define BASIC_FILE_SIZE (BASIC_ZP_SIZE + BASIC_MEM_SIZE)

static uint8_t g_memory[65536];
static uint8_t g_expected_zp[BASIC_ZP_SIZE];
static uint8_t g_expected_mem[BASIC_MEM_SIZE];
static uint8_t g_file[2560];
static size_t g_file_length;
static uint8_t g_sector_buffer[512];
static uint16_t g_sector;
static uint16_t g_size_register;
static size_t g_data_offset;
static uint8_t g_status;
static uint8_t g_info;
static bool g_open_pending;
static bool g_file_open;
static bool g_directory_returned_file;
static unsigned g_command_count[256];
static bool g_status_override_enabled;
static uint8_t g_status_override;
static unsigned g_status_read_count;
static const char* g_keys;
static size_t g_key_offset;
static int g_failures;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static void msc_command(uint8_t command) {
    g_command_count[command]++;
    switch (command) {
        case CMD_OPEN:
            g_open_pending = true;
            g_data_offset = 0;
            g_status = 0;
            break;
        case CMD_CLOSE:
            g_file_open = false;
            g_size_register = 0;
            g_status = 1;
            break;
        case CMD_READ: {
            const size_t offset = (size_t)g_sector * sizeof(g_sector_buffer);
            const size_t available = offset < g_file_length ? g_file_length - offset : 0;
            const size_t count = available < sizeof(g_sector_buffer) ? available : sizeof(g_sector_buffer);
            memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
            memcpy(g_sector_buffer, &g_file[offset], count);
            g_data_offset = 0;
            g_status = g_file_open ? 1 : 0x81;
            break;
        }
        case CMD_WRITE: {
            uint16_t count = g_size_register;
            if (count == 0 || count > sizeof(g_sector_buffer)) {
                count = sizeof(g_sector_buffer);
            }
            const size_t offset = (size_t)g_sector * sizeof(g_sector_buffer);
            memcpy(&g_file[offset], g_sector_buffer, count);
            g_file_length = offset + count;
            g_size_register = (uint16_t)g_file_length;
            g_data_offset = 0;
            g_status = g_file_open ? 1 : 0x81;
            break;
        }
        case CMD_DIR_OPEN:
            g_directory_returned_file = false;
            g_info = 0;
            g_status = 1;
            break;
        case CMD_DIR_NEXT:
            g_data_offset = 0;
            if (!g_directory_returned_file) {
                static const char filename[] = "BASIC.SAV";
                memcpy(g_sector_buffer, filename, sizeof(filename));
                g_size_register = (uint16_t)g_file_length;
                g_info = 1;
                g_directory_returned_file = true;
            } else {
                g_sector_buffer[0] = 0;
                g_size_register = 0;
                g_info = 0;
            }
            g_status = 1;
            break;
        case CMD_OPEN_IND:
            g_file_open = true;
            g_size_register = (uint16_t)g_file_length;
            g_data_offset = 0;
            g_status = 1;
            break;
        default:
            g_status = 0x81;
            break;
    }
}

uint8_t read6502(uint16_t address) {
    switch (address) {
        case KBDCR:
            return g_keys[g_key_offset] != '\0' ? 0x80 : 0;
        case KBD:
            if (g_keys[g_key_offset] == '\0') {
                return 0;
            }
            return (uint8_t)g_keys[g_key_offset++] | 0x80u;
        case MSC_DATA:
            if (g_data_offset >= sizeof(g_sector_buffer)) {
                return 0;
            }
            return g_sector_buffer[g_data_offset++];
        case MSC_STATUS:
            g_status_read_count++;
            return g_status_override_enabled ? g_status_override : g_status;
        case MSC_INFO:
            return g_info;
        case MSC_SIZE_LO:
            return (uint8_t)g_size_register;
        case MSC_SIZE_HI:
            return (uint8_t)(g_size_register >> 8);
        default:
            return g_memory[address];
    }
}

void write6502(uint16_t address, uint8_t value) {
    switch (address) {
        case DSP:
            break;
        case MSC_CMD:
            msc_command(value);
            break;
        case MSC_SECT_LO:
            g_sector = (uint16_t)((g_sector & 0xFF00u) | value);
            break;
        case MSC_SECT_HI:
            g_sector = (uint16_t)((g_sector & 0x00FFu) | ((uint16_t)value << 8));
            break;
        case MSC_DATA:
            if (g_open_pending) {
                if (value == 0) {
                    g_open_pending = false;
                    g_file_open = true;
                    g_size_register = (uint16_t)g_file_length;
                    g_data_offset = 0;
                    g_status = 1;
                }
            } else if (g_data_offset < sizeof(g_sector_buffer)) {
                g_sector_buffer[g_data_offset++] = value;
            }
            break;
        case MSC_INDEX:
            break;
        case MSC_SIZE_LO:
            g_size_register = (uint16_t)((g_size_register & 0xFF00u) | value);
            break;
        case MSC_SIZE_HI:
            g_size_register = (uint16_t)((g_size_register & 0x00FFu) | ((uint16_t)value << 8));
            break;
        default:
            g_memory[address] = value;
            break;
    }
}

static bool run_vaci(const char* keys) {
    g_keys = keys;
    g_key_offset = 0;
    g_memory[0xFFFC] = (uint8_t)NEO1_VACI_V1_ADDR;
    g_memory[0xFFFD] = (uint8_t)(NEO1_VACI_V1_ADDR >> 8);
    reset6502();

    for (size_t count = 0; count < 2000000u; count++) {
        if (pc == 0xFF00u) {
            return g_keys[g_key_offset] == '\0';
        }
        step6502();
    }
    return false;
}

static void reset_fixture(void) {
    memset(g_memory, 0, sizeof(g_memory));
    memset(g_file, 0, sizeof(g_file));
    memset(g_sector_buffer, 0, sizeof(g_sector_buffer));
    memset(g_command_count, 0, sizeof(g_command_count));
    memcpy(&g_memory[NEO1_VACI_V1_ADDR], neo1_vaci_v1, sizeof(neo1_vaci_v1));
    g_file_length = 0;
    g_sector = 0;
    g_size_register = 0;
    g_data_offset = 0;
    g_status = 1;
    g_info = 0;
    g_open_pending = false;
    g_file_open = false;
    g_directory_returned_file = false;
    g_status_override_enabled = false;
    g_status_override = 0;
    g_status_read_count = 0;
}

static void set_rom_protect_page(uint8_t page) {
    g_memory[NEO1_VACI_V1_ADDR + NEO1_VACI_V1_ROM_PROTECT_HI_OFFSET] = page;
}

static void prepare_workspace(void) {
    for (size_t i = 0; i < BASIC_ZP_SIZE; i++) {
        g_expected_zp[i] = (uint8_t)(0x31u + i * 7u);
    }
    for (size_t i = 0; i < BASIC_MEM_SIZE; i++) {
        g_expected_mem[i] = (uint8_t)(0xA7u ^ i);
    }
    memcpy(&g_memory[BASIC_ZP_START], g_expected_zp, sizeof(g_expected_zp));
    memcpy(&g_memory[BASIC_MEM_START], g_expected_mem, sizeof(g_expected_mem));
}

static void test_basic_round_trip(void) {
    reset_fixture();
    prepare_workspace();

    CHECK(run_vaci("SBASIC.SAV\r"));
    CHECK(g_file_length == BASIC_FILE_SIZE);
    CHECK(memcmp(g_file, g_expected_zp, sizeof(g_expected_zp)) == 0);
    CHECK(memcmp(&g_file[BASIC_ZP_SIZE], g_expected_mem, sizeof(g_expected_mem)) == 0);

    memset(&g_memory[BASIC_ZP_START], 0, BASIC_ZP_SIZE);
    memset(&g_memory[BASIC_MEM_START], 0, BASIC_MEM_SIZE);

    CHECK(run_vaci("L00"));
    CHECK(memcmp(&g_memory[BASIC_ZP_START], g_expected_zp, sizeof(g_expected_zp)) == 0);
    CHECK(memcmp(&g_memory[BASIC_MEM_START], g_expected_mem, sizeof(g_expected_mem)) == 0);
}

static void test_ordinary_write(void) {
    uint8_t expected[700];
    reset_fixture();
    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = (uint8_t)(0x5Au ^ i);
    }
    memcpy(&g_memory[0x0300], expected, sizeof(expected));

    CHECK(run_vaci("WTEST.BIN\r030005BB"));
    CHECK(g_file_length == sizeof(expected));
    CHECK(memcmp(g_file, expected, sizeof(expected)) == 0);
    CHECK(!g_file_open);
    CHECK(g_command_count[CMD_OPEN] == 1);
    CHECK(g_command_count[CMD_WRITE] == 2);
    CHECK(g_command_count[CMD_CLOSE] == 1);
}

static void test_ordinary_read_closes_file(void) {
    uint8_t expected[700];
    reset_fixture();
    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = (uint8_t)(0xC3u + i * 3u);
    }
    memcpy(g_file, expected, sizeof(expected));
    g_file_length = sizeof(expected);

    CHECK(run_vaci("R000300"));
    CHECK(memcmp(&g_memory[0x0300], expected, sizeof(expected)) == 0);
    CHECK(!g_file_open);
    CHECK(g_command_count[CMD_OPEN_IND] == 1);
    CHECK(g_command_count[CMD_READ] == 2);
    CHECK(g_command_count[CMD_CLOSE] == 1);
}

static void check_write_rejected(const char* keys) {
    CHECK(run_vaci(keys));
    CHECK(g_command_count[CMD_OPEN] == 0);
    CHECK(g_command_count[CMD_WRITE] == 0);
    CHECK(g_command_count[CMD_CLOSE] == 0);
}

static void test_ordinary_write_rejects_unsafe_ranges(void) {
    reset_fixture();
    check_write_rejected("WFULL.BIN\r0000FFFFQ");

    reset_fixture();
    check_write_rejected("WPAGE2.BIN\r02000300Q");

    reset_fixture();
    check_write_rejected("WVCFFA.BIN\rAF00B000Q");

    reset_fixture();
    check_write_rejected("WVACI.BIN\rC000C100Q");

    reset_fixture();
    check_write_rejected("WIO.BIN\rCFFFD100Q");

    reset_fixture();
    set_rom_protect_page(0xE0);
    check_write_rejected("WROM23.BIN\rE000E00FQ");
}

static void test_neo1_50_upper_ram_remains_writable(void) {
    uint8_t expected[16];
    reset_fixture();
    set_rom_protect_page(0xFF);
    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = (uint8_t)(0x90u + i);
    }
    memcpy(&g_memory[0xE000], expected, sizeof(expected));

    CHECK(run_vaci("WUPPER.BIN\rE000E00F"));
    CHECK(g_file_length == sizeof(expected));
    CHECK(memcmp(g_file, expected, sizeof(expected)) == 0);
    CHECK(g_command_count[CMD_CLOSE] == 1);
}

static void test_read_rejects_wrap_and_closes(void) {
    reset_fixture();
    set_rom_protect_page(0xFF);
    g_file_length = 512;

    CHECK(run_vaci("R00FF00Q"));
    CHECK(g_command_count[CMD_OPEN_IND] == 1);
    CHECK(g_command_count[CMD_READ] == 0);
    CHECK(g_command_count[CMD_CLOSE] == 1);
    CHECK(!g_file_open);
}

static void test_read_uses_profile_rom_boundary(void) {
    uint8_t expected[16];
    for (size_t i = 0; i < sizeof(expected); i++) {
        expected[i] = (uint8_t)(0x42u + i * 5u);
    }

    reset_fixture();
    set_rom_protect_page(0xE0);
    memcpy(g_file, expected, sizeof(expected));
    g_file_length = sizeof(expected);
    CHECK(run_vaci("R00E000Q"));
    CHECK(g_command_count[CMD_READ] == 0);
    CHECK(g_command_count[CMD_CLOSE] == 1);

    reset_fixture();
    set_rom_protect_page(0xFF);
    memcpy(g_file, expected, sizeof(expected));
    g_file_length = sizeof(expected);
    CHECK(run_vaci("R00E000"));
    CHECK(memcmp(&g_memory[0xE000], expected, sizeof(expected)) == 0);
    CHECK(g_command_count[CMD_READ] == 1);
    CHECK(g_command_count[CMD_CLOSE] == 1);
}

static void test_status_contract_and_timeout(void) {
    reset_fixture();
    g_status_override_enabled = true;
    g_status_override = 0x02;
    CHECK(run_vaci("RQ"));
    CHECK(g_command_count[CMD_DIR_OPEN] == 1);
    CHECK(g_command_count[CMD_DIR_NEXT] == 0);
    CHECK(g_status_read_count == 1);

    reset_fixture();
    g_status_override_enabled = true;
    g_status_override = 0x00;
    CHECK(run_vaci("RQ"));
    CHECK(g_command_count[CMD_DIR_OPEN] == 1);
    CHECK(g_command_count[CMD_DIR_NEXT] == 0);
    CHECK(g_status_read_count == 65536u);
}

int main(void) {
    test_basic_round_trip();
    test_ordinary_write();
    test_ordinary_read_closes_file();
    test_ordinary_write_rejects_unsafe_ranges();
    test_neo1_50_upper_ram_remains_writable();
    test_read_rejects_wrap_and_closes();
    test_read_uses_profile_rom_boundary();
    test_status_contract_and_timeout();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_vaci_payload_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_vaci_payload_tests: BASIC and ordinary transfer contracts passed");
    return 0;
}
