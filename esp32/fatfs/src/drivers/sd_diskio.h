/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef SD_DISKIO_H_
#define SD_DISKIO_H_

#define SD_SECTOR_SIZE                          512

//*****************************************************************************
// Disk Info Structure definition
//*****************************************************************************

extern sdmmc_card_t sdmmc_card_info;

DSTATUS sd_disk_init (void);
void sd_disk_deinit (void);
DRESULT sd_disk_read (BYTE* pBuffer, DWORD ulSectorNumber, UINT bSectorCount);
DRESULT sd_disk_write (const BYTE* pBuffer, DWORD ulSectorNumber, UINT bSectorCount);

#endif /* SD_DISKIO_H_ */
