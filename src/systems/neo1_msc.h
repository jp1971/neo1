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
// The 6502 places a zero-terminated filename string at NEO1_MSC_FILENAME_ADDR
// before issuing the OPEN command.

// Fixed RAM location where the 6502 writes the filename to open.
#define NEO1_MSC_FILENAME_ADDR (0xC100)
#define NEO1_MSC_FILENAME_MAX  (128)

// I/O register addresses.
#define NEO1_IO_MSC_CMD        (0xD014)
#define NEO1_IO_MSC_SECTOR_LO  (0xD015)
#define NEO1_IO_MSC_SECTOR_HI  (0xD016)
#define NEO1_IO_MSC_DATA       (0xD017)
#define NEO1_IO_MSC_STATUS     (0xD018)

// Command opcodes written to NEO1_IO_MSC_CMD.
#define NEO1_MSC_CMD_OPEN      (0x01)
#define NEO1_MSC_CMD_CLOSE     (0x02)
#define NEO1_MSC_CMD_READ      (0x03)
#define NEO1_MSC_CMD_WRITE     (0x04)

// Status bits read from NEO1_IO_MSC_STATUS.
#define NEO1_MSC_STATUS_READY  (1u << 0)
#define NEO1_MSC_STATUS_ERROR  (1u << 1)

// Initialize MSC subsystem. Called once during system startup.
void neo1_msc_init(void);

// Called by the Neo1 runtime when the 6502 accesses MSC I/O registers.
uint8_t neo1_msc_io_read(uint16_t addr);
void neo1_msc_io_write(uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif
