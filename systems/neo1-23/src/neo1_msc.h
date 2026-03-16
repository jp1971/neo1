#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory-mapped I/O for Neo1 MSC (Mass Storage) access.
//
// This is a minimal command-based interface where the 6502 writes a command
// to a register and then polls a status register until the command completes.
//
// The 6502 writes the filename bytes (including terminating NUL) to the DATA
// port after issuing an OPEN command.
//
// Debug output:
// Set NEO1_MSC_DEBUG to 0 to disable all debug output, or 1 (default) to enable.
// Debug messages are printed to the host console (stdio).
#define NEO1_MSC_DEBUG 0

// I/O register addresses.
#define NEO1_IO_MSC_CMD        (0xD014)
#define NEO1_IO_MSC_SECTOR_LO  (0xD015)
#define NEO1_IO_MSC_SECTOR_HI  (0xD016)
#define NEO1_IO_MSC_DATA       (0xD017)
#define NEO1_IO_MSC_STATUS     (0xD018)
#define NEO1_IO_MSC_INDEX      (0xD019)
#define NEO1_IO_MSC_INFO       (0xD01A)

// Command opcodes written to NEO1_IO_MSC_CMD.
#define NEO1_MSC_CMD_OPEN      (0x01)
#define NEO1_MSC_CMD_CLOSE     (0x02)
#define NEO1_MSC_CMD_READ      (0x03)
#define NEO1_MSC_CMD_WRITE     (0x04)

// Phase 2 directory/navigation commands.
// DIR_OPEN: open root directory and reset enumeration cursor.
// DIR_NEXT: find next loadable file entry and stream its NUL-terminated name via DATA reads.
// OPEN_INDEX: open the Nth loadable file in root (index written to INDEX register).
#define NEO1_MSC_CMD_DIR_OPEN   (0x10)
#define NEO1_MSC_CMD_DIR_NEXT   (0x11)
#define NEO1_MSC_CMD_OPEN_INDEX (0x12)

// Status values read from NEO1_IO_MSC_STATUS.
// - 0x00 means busy (command in progress).
// - 0x01 means ready (command completed successfully).
// - 0x80 + (err & 0x7F) means error; low bits contain the error code.
#define NEO1_MSC_STATUS_BUSY   0x00
#define NEO1_MSC_STATUS_READY  0x01
#define NEO1_MSC_STATUS_ERROR  0x80

// INFO register bits (NEO1_IO_MSC_INFO).
// Written by firmware after DIR_NEXT.
#define NEO1_MSC_INFO_VALID      (1u << 0)   // 1 when a directory entry is available
#define NEO1_MSC_INFO_DIRECTORY  (1u << 1)   // 1 when entry is a directory

// Maximum filename length accepted via the DATA port.
#define NEO1_MSC_FILENAME_MAX  (128)

// Initialize MSC subsystem. Called once during system startup.
void neo1_msc_init(void);

// Called by the Neo1 runtime when the 6502 accesses MSC I/O registers.
uint8_t neo1_msc_io_read(uint16_t addr);
void neo1_msc_io_write(uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif
