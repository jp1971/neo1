#pragma once

// 6502-visible Neo1 MSC file-service contract. This is a Neo1 extension, not
// an Apple-1 or Replica 1 device. The shared machine owns address decode, this
// module owns register/transaction state, and target backends own I/O handles.
// Commands do not assert IRQ or NMI, so 6502 code polls STATUS.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NEO1_IO_MSC_CMD        (0xD014)
#define NEO1_IO_MSC_SECTOR_LO  (0xD015)
#define NEO1_IO_MSC_SECTOR_HI  (0xD016)
#define NEO1_IO_MSC_DATA       (0xD017)
#define NEO1_IO_MSC_STATUS     (0xD018)
#define NEO1_IO_MSC_INDEX      (0xD019)
#define NEO1_IO_MSC_INFO       (0xD01A)
#define NEO1_IO_MSC_SIZE_LO    (0xD01B)
#define NEO1_IO_MSC_SIZE_HI    (0xD01C)

// CMD dispatches one synchronous operation. OPEN is the exception in form:
// writing OPEN clears the filename buffer and reports BUSY; a terminating NUL
// or the filename limit on later DATA writes performs the filesystem operation.
// SECTOR is a little-endian 512-byte index. DATA streams filenames, directory
// names, or sector bytes. INDEX selects loadable root files. INFO reports a
// directory result. SIZE is the saturated file size or requested WRITE length;
// zero or more than 512 requests 512 bytes. A successful short WRITE truncates
// the file at sector * 512 + length.

#define NEO1_MSC_CMD_OPEN         (0x01)
#define NEO1_MSC_CMD_CLOSE        (0x02)
#define NEO1_MSC_CMD_READ         (0x03)
#define NEO1_MSC_CMD_WRITE        (0x04)
#define NEO1_MSC_CMD_DIR_OPEN     (0x10)
#define NEO1_MSC_CMD_DIR_NEXT     (0x11)
#define NEO1_MSC_CMD_OPEN_INDEX   (0x12)
#define NEO1_MSC_CMD_DELETE_INDEX (0x13)

#define NEO1_MSC_STATUS_BUSY  (0x00)
#define NEO1_MSC_STATUS_READY (0x01)
#define NEO1_MSC_STATUS_ERROR (0x80)

// ERROR is combined with the backend error code's low seven bits.

#define NEO1_MSC_INFO_VALID     (1u << 0)
#define NEO1_MSC_INFO_DIRECTORY (1u << 1)

#define NEO1_MSC_FILENAME_MAX (128)
#define NEO1_MSC_SECTOR_SIZE  (512u)

// Backends may allow raw sector access without a successful named-file OPEN.
// This supports SDL's existing raw image without weakening Pico file rules.
#define NEO1_MSC_BACKEND_OPEN_OPTIONAL (1u << 0)

typedef struct {
    char name[NEO1_MSC_FILENAME_MAX];
    uint32_t size;
    bool directory;
    bool valid;
} neo1_msc_dir_entry_t;

typedef struct {
    uint8_t (*open)(void* context, const char* name, uint32_t* out_size);
    void (*close)(void* context);
    uint8_t (*read_sector)(void* context,
                           uint16_t sector,
                           uint8_t* buffer,
                           size_t* out_size);
    uint8_t (*write_sector)(void* context,
                            uint16_t sector,
                            const uint8_t* buffer,
                            uint16_t size,
                            uint32_t* out_file_size);
    uint8_t (*dir_open)(void* context);
    uint8_t (*dir_next)(void* context, neo1_msc_dir_entry_t* out_entry);
    void (*dir_close)(void* context);
    uint8_t (*delete_file)(void* context, const char* name);
} neo1_msc_backend_ops_t;

typedef struct {
    const neo1_msc_backend_ops_t* ops;
    void* context;
    uint8_t flags;
    uint8_t not_found_error;
    uint8_t invalid_state_error;
} neo1_msc_backend_t;

typedef struct {
    neo1_msc_backend_t backend;
    uint16_t sector;
    uint16_t data_offset;
    uint16_t file_size;
    uint16_t open_filename_pos;
    uint8_t status;
    uint8_t index;
    uint8_t info;
    uint8_t last_dir_index;
    bool file_open;
    bool dir_open;
    bool open_pending;
    uint8_t buffer[NEO1_MSC_SECTOR_SIZE];
    char open_filename[NEO1_MSC_FILENAME_MAX];
} neo1_msc_t;

// Initialize one independent register protocol instance. Backend operations
// are synchronous and return zero for success or a low-seven-bit error code.
bool neo1_msc_init(neo1_msc_t* msc, const neo1_msc_backend_t* backend);
uint8_t neo1_msc_read(neo1_msc_t* msc, uint16_t addr);
void neo1_msc_write(neo1_msc_t* msc, uint16_t addr, uint8_t data);
