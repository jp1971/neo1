#pragma once

// neo1_cffa1.h
//
// Neo1 virtual VCFFA1 compatibility bridge.
//
// This optional Replica 1 compatibility device exposes signature bytes and an
// ATA-like register window to 6502 software. It is not part of the Apple-1 core
// and does not define Neo1's preferred storage architecture. The Pico backend
// maps blocks to a FatFs disk-image file; SDL reuses the register constants for
// a separate raw-image implementation with documented deviations.
//
// 6502 software observes command and 512-byte data-phase state by polling
// STATUS/DRQ. The device does not assert IRQ or NMI.

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signature bytes expected by VCFFA1-aware software.

#define NEO1_CFFA1_ID1_ADDR   (0xAFDC)
#define NEO1_CFFA1_ID2_ADDR   (0xAFDD)
#define NEO1_CFFA1_ID1_VALUE  (0xCF)
#define NEO1_CFFA1_ID2_VALUE  (0xFA)

#define NEO1_CFFA1_IO_BASE    (0xAFF0)
#define NEO1_CFFA1_IO_END     (0xAFFF)
#define NEO1_CFFA1_IO_SIZE    (0x10)

// Register contract for $AFF0-$AFFF. Unassigned offsets retain ordinary
// register bytes within the enabled device window.
// - +0x06 read: ALTSTATUS mirror; write: DEVCTRL value mirrored to STATUS by
//   the Pico implementation.
// - +0x08 DATA: streams one 512-byte block during a DRQ data phase.
// - +0x09 read: ERROR; write: FEATURE. Commands replace it with their result.
// - +0x0A SECTOR COUNT: stored pass-through value; commands ignore it.
// - +0x0B..+0x0E LBA0..LBA3: 32-bit block number, least-significant byte first.
// - +0x0F read: STATUS; write: COMMAND.

// ATA-like register offsets within $AFF0-$AFFF.
#define NEO1_CFFA1_REG_DEVCTRL_ALTSTATUS  (0x06)
#define NEO1_CFFA1_REG_DATA               (0x08)
#define NEO1_CFFA1_REG_ERROR_FEATURE      (0x09)
#define NEO1_CFFA1_REG_SECTOR_COUNT       (0x0A)
#define NEO1_CFFA1_REG_LBA0               (0x0B)
#define NEO1_CFFA1_REG_LBA1               (0x0C)
#define NEO1_CFFA1_REG_LBA2               (0x0D)
#define NEO1_CFFA1_REG_LBA3               (0x0E)
#define NEO1_CFFA1_REG_STATUS_COMMAND     (0x0F)

// Supported ProDOS-style commands written to STATUS/COMMAND:
// - STATUS validates that a backing image can be opened.
// - READ validates LBA, fills the buffer synchronously, resets DATA, and sets
//   DRQ until 512 DATA reads complete.
// - WRITE validates LBA and write permission, resets DATA, and sets DRQ; the
//   512th DATA write commits and syncs the block before DRQ clears.
#define NEO1_CFFA1_CMD_PRODOS_STATUS      (0x00)
#define NEO1_CFFA1_CMD_PRODOS_READ        (0x01)
#define NEO1_CFFA1_CMD_PRODOS_WRITE       (0x02)

// ProDOS-style low-level error codes.
#define NEO1_CFFA1_ERR_OK                 (0x00)
#define NEO1_CFFA1_ERR_BADCMD             (0x01)
#define NEO1_CFFA1_ERR_IO                 (0x27)
#define NEO1_CFFA1_ERR_NODEV              (0x28)
#define NEO1_CFFA1_ERR_WRITE_PROTECT      (0x2B)
#define NEO1_CFFA1_ERR_BADBLOCK           (0x2D)

// ATA-like status bits. Successful idle state is DRDY|DSC; READ or WRITE data
// phase adds DRQ. Errors report DRDY|DSC|ERR. No implemented path sets BSY.
#define NEO1_CFFA1_STATUS_ERR             (1u << 0)
#define NEO1_CFFA1_STATUS_DRQ             (1u << 3)
#define NEO1_CFFA1_STATUS_DSC             (1u << 4)
#define NEO1_CFFA1_STATUS_DRDY            (1u << 6)
#define NEO1_CFFA1_STATUS_BSY             (1u << 7)

// Reset protocol and lazy image-selection state.
void neo1_cffa1_init(void);
// Report ownership of the two signature addresses and the register window.
bool neo1_cffa1_handles_addr(uint16_t addr);
// Service decoded 6502 accesses; the backend owns registers, block buffer, and
// image state, while the shared machine owns conditional address decode.
uint8_t neo1_cffa1_io_read(uint16_t addr);
void neo1_cffa1_io_write(uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif
