/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lib/oofatfs/ff.h"
#include "lib/oofatfs/diskio.h"
#include "sd_diskio.h"
#include "stdcmd.h"

//*****************************************************************************
// Macros
//*****************************************************************************
#define DISKIO_RETRY_TIMEOUT        0xFFFFFFFF

#define CARD_TYPE_UNKNOWN           0
#define CARD_TYPE_MMC               1
#define CARD_TYPE_SDCARD            2

#define CARD_CAP_CLASS_SDSC         0
#define CARD_CAP_CLASS_SDHC         1

#define CARD_VERSION_1              0
#define CARD_VERSION_2              1

//*****************************************************************************
// Disk Info for attached disk
//*****************************************************************************
sdmmc_card_t sdmmc_card_info;
static DSTATUS sd_card_status = STA_NOINIT;

//*****************************************************************************
//
//! Initializes physical drive
//!
//! This function initializes the physical drive
//!
//! \return Returns 0 on succeeded.
//*****************************************************************************
DSTATUS sd_disk_init (void) {
    sdmmc_host_t config =
    {
        .flags = SDMMC_HOST_FLAG_1BIT,
        .slot = SDMMC_HOST_SLOT_1,
        .max_freq_khz = SDMMC_FREQ_DEFAULT,
        .io_voltage = 3.3f,
        .init = &sdmmc_host_init,
        .set_bus_width = &sdmmc_host_set_bus_width,
        .get_bus_width = &sdmmc_host_get_slot_width,
        .set_card_clk = &sdmmc_host_set_card_clk,
        .do_transaction = &sdmmc_host_do_transaction,
        .deinit = &sdmmc_host_deinit,
    };

    // enable pull-ups on the SD card pins
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(14, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);

    if (ESP_OK == sdmmc_card_init(&config, &sdmmc_card_info)) {
        sd_card_status = 0;
    } else {
        sd_card_status = STA_NOINIT;
    }

    return sd_card_status;
}

//*****************************************************************************
//
//! De-initializes the physical drive
//!
//! This function de-initializes the physical drive
//*****************************************************************************
void sd_disk_deinit (void) {
    sdmmc_card_info.csd.capacity = 0;
    sd_card_status = STA_NOINIT;
}

//*****************************************************************************
//
//! Reads sector(s) from the disk drive.
//!
//!
//! This function reads specified number of sectors from the drive
//!
//! \return Returns RES_OK on success.
//
//*****************************************************************************
DRESULT sd_disk_read (BYTE* pBuffer, DWORD ulSectorNumber, UINT SectorCount) {
    if (SectorCount > 0) {
        if (ESP_OK == sdmmc_read_sectors(&sdmmc_card_info, pBuffer, ulSectorNumber, SectorCount)) {
            return RES_OK;
        }
    }
    return RES_ERROR;
}

//*****************************************************************************
//
//! Wrties sector(s) to the disk drive.
//!
//!
//! This function writes specified number of sectors to the drive
//!
//! \return Returns RES_OK on success.
//
//*****************************************************************************
DRESULT sd_disk_write (const BYTE* pBuffer, DWORD ulSectorNumber, UINT SectorCount) {
    if (SectorCount > 0) {
        if (ESP_OK == sdmmc_write_sectors(&sdmmc_card_info, pBuffer, ulSectorNumber, SectorCount)) {
            return RES_OK;
        }
    }
    return RES_ERROR;
}
