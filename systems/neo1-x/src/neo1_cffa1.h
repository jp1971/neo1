#pragma once

// neo1_cffa1.h
//
// Neo1 virtual CFFA1 compatibility bridge.
//
// This module exposes a minimal CFFA1-like register window and signature bytes
// so legacy monitor/tooling paths can talk to block storage semantics while the
// backend is implemented on top of host-side FatFs file I/O.

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal CFFA1 compatibility surface for Neo1.
//
// Phase M0 goals:
// - expose CFFA1 signature bytes at $AFDC/$AFDD
// - provide a safe readable/writable I/O window at $AFF0-$AFFF
// - keep behavior deterministic while backend commands are unimplemented

// Signature bytes expected by CFFA1-aware software.

#define NEO1_CFFA1_ID1_ADDR   (0xAFDC)
#define NEO1_CFFA1_ID2_ADDR   (0xAFDD)
#define NEO1_CFFA1_ID1_VALUE  (0xCF)
#define NEO1_CFFA1_ID2_VALUE  (0xFA)

#define NEO1_CFFA1_IO_BASE    (0xAFF0)
#define NEO1_CFFA1_IO_END     (0xAFFF)
#define NEO1_CFFA1_IO_SIZE    (0x10)

// Register map overview for $AFF0-$AFFF window:
// - +0x06 ALTSTATUS / DEVCTRL mirror
// - +0x08 DATA (streams block bytes)
// - +0x09 ERROR / FEATURE
// - +0x0A SECTOR COUNT (currently pass-through)
// - +0x0B..+0x0E LBA0..LBA3
// - +0x0F STATUS / COMMAND

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

// M1 bridge command subset (written to STATUS/COMMAND register).
// These are intentionally minimal command IDs for incremental bring-up.
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

// ATA-like status bits used by this bridge.
#define NEO1_CFFA1_STATUS_ERR             (1u << 0)
#define NEO1_CFFA1_STATUS_DRQ             (1u << 3)
#define NEO1_CFFA1_STATUS_DSC             (1u << 4)
#define NEO1_CFFA1_STATUS_DRDY            (1u << 6)
#define NEO1_CFFA1_STATUS_BSY             (1u << 7)

// Public bridge API used by Neo1 runtime bus hooks.
void neo1_cffa1_init(void);
bool neo1_cffa1_handles_addr(uint16_t addr);
uint8_t neo1_cffa1_io_read(uint16_t addr);
void neo1_cffa1_io_write(uint16_t addr, uint8_t data);

#ifdef __cplusplus
} // extern "C"
#endif
