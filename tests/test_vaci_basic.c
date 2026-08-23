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
            return g_status;
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

int main(void) {
    memset(g_memory, 0, sizeof(g_memory));
    memset(g_file, 0, sizeof(g_file));
    memcpy(&g_memory[NEO1_VACI_V1_ADDR], neo1_vaci_v1, sizeof(neo1_vaci_v1));
    g_status = 1;
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

    if (g_failures != 0) {
        fprintf(stderr, "neo1_vaci_basic_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_vaci_basic_tests: 2230-byte round trip passed");
    return 0;
}
