/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "tusb.h"
#include "class/msc/msc.h"

/* Definitions of physical drive number for each drive */
#define DEV_USB		0	/* Map USB MSD to physical drive 0 */

static bool disk_inited[1] = {false};
static volatile bool msc_complete = false;
static bool msc_success = false;

bool msc_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const* cb_data) {
    (void)dev_addr; (void)cb_data;
    msc_complete = true;
    msc_success = cb_data->csw->status == 0;
    return true;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	if (pdrv != DEV_USB) return STA_NOINIT;

	if (!disk_inited[pdrv]) return STA_NOINIT;

	// For MSC, assume always ready if mounted
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	if (pdrv != DEV_USB) return STA_NOINIT;

	disk_inited[pdrv] = true;
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if (pdrv != DEV_USB) return RES_PARERR;
	if (!disk_inited[pdrv]) return RES_NOTRDY;

	// Find the MSC device
	uint8_t dev_addr = 0;
	for (uint8_t addr = 1; addr <= CFG_TUH_DEVICE_MAX; addr++) {
		if (tuh_msc_mounted(addr)) {
			dev_addr = addr;
			break;
		}
	}
	if (dev_addr == 0) return RES_NOTRDY;

	// Read sectors
	for (UINT i = 0; i < count; i++) {
		msc_complete = false;
		msc_success = false;
		if (!tuh_msc_read10(dev_addr, 0, buff + i * 512, sector + i, 1, msc_complete_cb, 0)) {
			return RES_ERROR;
		}
		// Wait for completion
		while (!msc_complete) {
			tuh_task();
		}
		if (!msc_success) return RES_ERROR;
	}

	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	if (pdrv != DEV_USB) return RES_PARERR;
	if (!disk_inited[pdrv]) return RES_NOTRDY;

	// Find the MSC device
	uint8_t dev_addr = 0;
	for (uint8_t addr = 1; addr <= CFG_TUH_DEVICE_MAX; addr++) {
		if (tuh_msc_mounted(addr)) {
			dev_addr = addr;
			break;
		}
	}
	if (dev_addr == 0) return RES_NOTRDY;

	// Write sectors
	for (UINT i = 0; i < count; i++) {
		msc_complete = false;
		msc_success = false;
		if (!tuh_msc_write10(dev_addr, 0, buff + i * 512, sector + i, 1, msc_complete_cb, 0)) {
			return RES_ERROR;
		}
		// Wait for completion
		while (!msc_complete) {
			tuh_task();
		}
		if (!msc_success) return RES_ERROR;
	}

	return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if (pdrv != DEV_USB) return RES_PARERR;
	if (!disk_inited[pdrv]) return RES_NOTRDY;

	// Find the MSC device
	uint8_t dev_addr = 0;
	for (uint8_t addr = 1; addr <= CFG_TUH_DEVICE_MAX; addr++) {
		if (tuh_msc_mounted(addr)) {
			dev_addr = addr;
			break;
		}
	}
	if (dev_addr == 0) return RES_NOTRDY;

	switch (cmd) {
		case CTRL_SYNC:
			// Sync write cache (not needed for MSC)
			return RES_OK;

		case GET_SECTOR_COUNT: {
			// For now, return a large number
			*(LBA_t*)buff = 1000000;
			return RES_OK;
		}

		case GET_SECTOR_SIZE:
			*(WORD*)buff = 512;
			return RES_OK;

		case GET_BLOCK_SIZE:
			*(DWORD*)buff = 1;
			return RES_OK;

		default:
			return RES_PARERR;
	}
}

DWORD get_fattime(void) {
	// Return a fixed time for now
	return ((2023 - 1980) << 25) | (1 << 21) | (1 << 16) | (0 << 11) | (0 << 5) | (0 >> 1);
}

