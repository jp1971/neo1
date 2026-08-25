#pragma once

// 6502-visible Neo1 MSC file-service contract. This is a Neo1 extension, not
// an Apple-1 or Replica 1 device. The shared machine owns this address decode;
// target device implementations own protocol and backend state. Commands do
// not assert IRQ or NMI, so 6502 code polls STATUS.

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
