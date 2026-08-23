#pragma once

// neo1_msc.h
//
// Neo1 memory-mapped file service used by the 6502-side VACI program.
//
// This is a Neo1 extension, not an Apple-1 or Replica 1 device. The Pico backend
// maps the protocol to files on a FatFs-mounted USB drive. The SDL target reuses
// these register and command constants but implements only a divergent raw-block
// subset; see neo1_storage_stub.c.
//
// Pico commands run synchronously in the 6502 access that completes them. OPEN
// is the exception in form: writing OPEN clears the filename buffer and reports
// BUSY, then a terminating NUL or the filename limit on subsequent DATA writes
// performs the filesystem operation. No command asserts IRQ or NMI; 6502 code
// polls STATUS.

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NEO1_DIAGNOSTICS
#define NEO1_DIAGNOSTICS 0
#endif

// Follow the firmware-wide diagnostic policy unless explicitly overridden.
#ifndef NEO1_MSC_DEBUG
#define NEO1_MSC_DEBUG NEO1_DIAGNOSTICS
#endif

// I/O register addresses.
#define NEO1_IO_MSC_CMD        (0xD014)
#define NEO1_IO_MSC_SECTOR_LO  (0xD015)
#define NEO1_IO_MSC_SECTOR_HI  (0xD016)
#define NEO1_IO_MSC_DATA       (0xD017)
#define NEO1_IO_MSC_STATUS     (0xD018)
#define NEO1_IO_MSC_INDEX      (0xD019)
#define NEO1_IO_MSC_INFO       (0xD01A)
#define NEO1_IO_MSC_SIZE_LO    (0xD01B)
#define NEO1_IO_MSC_SIZE_HI    (0xD01C)

// Register contract:
// - CMD (write): dispatch one command; OPEN begins filename collection.
// - SECTOR_LO/HI (write): 16-bit little-endian 512-byte sector index.
// - DATA (read/write): shared 512-byte sector, filename, or directory-name
//   stream. READ and DIR_NEXT reset the read offset. OPEN resets the filename
//   write offset. Sector payload bytes are written before issuing WRITE.
// - STATUS (read): BUSY, READY, or ERROR plus an error code.
// - INDEX (read/write): zero-based loadable-file selector for OPEN_INDEX and
//   DELETE_INDEX. Directory enumeration skips dot entries and directories.
// - INFO (read): DIR_NEXT result; VALID is clear at end of enumeration.
// - SIZE_LO/HI (read/write): saturating 16-bit size reported by OPEN/DIR_NEXT,
//   or requested WRITE length. A WRITE length of zero or greater than 512 means
//   512 bytes. Successful WRITE truncates at sector * 512 + write length.

// Command opcodes written to CMD:
// - OPEN: reset filename collection, then open/create on terminating DATA NUL.
// - CLOSE: close the active file and directory enumeration.
// - READ: fill DATA from the selected sector and zero-pad a short file read.
// - WRITE: commit the buffered DATA length selected by SIZE and truncate there.
#define NEO1_MSC_CMD_OPEN      (0x01)
#define NEO1_MSC_CMD_CLOSE     (0x02)
#define NEO1_MSC_CMD_READ      (0x03)
#define NEO1_MSC_CMD_WRITE     (0x04)

// - DIR_OPEN: open the root directory and reset enumeration.
// - DIR_NEXT: expose the next loadable name, INFO, and saturated SIZE.
// - OPEN_INDEX: resolve INDEX among loadable root files, then perform OPEN.
// - DELETE_INDEX: resolve INDEX, close active handles, and unlink that file.
#define NEO1_MSC_CMD_DIR_OPEN   (0x10)
#define NEO1_MSC_CMD_DIR_NEXT   (0x11)
#define NEO1_MSC_CMD_OPEN_INDEX (0x12)
#define NEO1_MSC_CMD_DELETE_INDEX (0x13)

// Status values read from NEO1_IO_MSC_STATUS.
// - 0x00 means busy (command in progress).
// - 0x01 means ready (command completed successfully).
// - 0x80 + (err & 0x7F) means error; low bits contain the error code.
#define NEO1_MSC_STATUS_BUSY   0x00
#define NEO1_MSC_STATUS_READY  0x01
#define NEO1_MSC_STATUS_ERROR  0x80

// INFO register bits set by DIR_NEXT. The Pico backend filters directories, so
// DIRECTORY remains clear for entries it returns.
#define NEO1_MSC_INFO_VALID      (1u << 0)   // 1 when a directory entry is available
#define NEO1_MSC_INFO_DIRECTORY  (1u << 1)   // 1 when entry is a directory

// Filename buffer size including its terminating NUL.
#define NEO1_MSC_FILENAME_MAX  (128)

// Reset register/protocol state. Mounting and file selection remain lazy.
void neo1_msc_init(void);

// Service one decoded 6502 register access. The shared machine owns the address
// decode; this backend owns protocol buffers and filesystem state.
uint8_t neo1_msc_io_read(uint16_t addr);
void neo1_msc_io_write(uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif
