#pragma once

// 6502-visible VCFFA1 compatibility contract. This optional Replica 1 device is
// not part of the Apple-1 core or Neo1's preferred storage architecture. The
// shared machine owns its signature/register decode; target devices own
// protocol/backend state. Software polls STATUS/DRQ; no IRQ or NMI is raised.

#define NEO1_CFFA1_ID1_ADDR  (0xAFDC)
#define NEO1_CFFA1_ID2_ADDR  (0xAFDD)
#define NEO1_CFFA1_ID1_VALUE (0xCF)
#define NEO1_CFFA1_ID2_VALUE (0xFA)

#define NEO1_CFFA1_IO_BASE (0xAFF0)
#define NEO1_CFFA1_IO_END  (0xAFFF)
#define NEO1_CFFA1_IO_SIZE (0x10)

// The ATA-like window stores pass-through register bytes. +$06 is ALTSTATUS on
// read and DEVCTRL on write. DATA at +$08 streams a 512-byte block while DRQ is
// set. +$09 is ERROR on read and FEATURE on write. LBA0..3 at +$0B..+$0E form a
// little-endian block number. STATUS is read at +$0F and commands are written
// there. Unassigned offsets retain ordinary register bytes while enabled.

#define NEO1_CFFA1_REG_DEVCTRL_ALTSTATUS (0x06)
#define NEO1_CFFA1_REG_DATA               (0x08)
#define NEO1_CFFA1_REG_ERROR_FEATURE      (0x09)
#define NEO1_CFFA1_REG_SECTOR_COUNT       (0x0A)
#define NEO1_CFFA1_REG_LBA0               (0x0B)
#define NEO1_CFFA1_REG_LBA1               (0x0C)
#define NEO1_CFFA1_REG_LBA2               (0x0D)
#define NEO1_CFFA1_REG_LBA3               (0x0E)
#define NEO1_CFFA1_REG_STATUS_COMMAND     (0x0F)

#define NEO1_CFFA1_CMD_PRODOS_STATUS (0x00)
#define NEO1_CFFA1_CMD_PRODOS_READ   (0x01)
#define NEO1_CFFA1_CMD_PRODOS_WRITE  (0x02)

// STATUS validates the backing image. READ fills the buffer and sets DRQ until
// 512 DATA reads complete. WRITE sets DRQ and commits on the 512th DATA write.

#define NEO1_CFFA1_ERR_OK            (0x00)
#define NEO1_CFFA1_ERR_BADCMD        (0x01)
#define NEO1_CFFA1_ERR_IO            (0x27)
#define NEO1_CFFA1_ERR_NODEV         (0x28)
#define NEO1_CFFA1_ERR_WRITE_PROTECT (0x2B)
#define NEO1_CFFA1_ERR_BADBLOCK      (0x2D)

#define NEO1_CFFA1_STATUS_ERR  (1u << 0)
#define NEO1_CFFA1_STATUS_DRQ  (1u << 3)
#define NEO1_CFFA1_STATUS_DSC  (1u << 4)
#define NEO1_CFFA1_STATUS_DRDY (1u << 6)
#define NEO1_CFFA1_STATUS_BSY  (1u << 7)

// Successful idle state is DRDY|DSC. A data phase adds DRQ; errors report
// DRDY|DSC|ERR. Current implementations do not expose a BSY phase.
