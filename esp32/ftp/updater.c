/*
 * Copyright (c) 2019, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "bootloader.h"
#include "updater.h"
#include "esp_spi_flash.h"
#include "esp_flash_encrypt.h"
#include "esp_image_format.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "rom/crc.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
static const char *TAG = "updater";
#define UPDATER_IMG_PATH                                "/flash/sys/appimg.bin"

/* if flash is encrypted, it requires the flash_write operation to be done in 16 Bytes chunks */
#define ENCRYP_FLASH_MIN_CHUNK                            16

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    uint32_t size;
    uint32_t offset;
    uint32_t offset_start_upd;
    uint32_t chunk_size;
    uint32_t current_chunk;
} updater_data_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static updater_data_t updater_data = {
    .size = 0,
    .offset = 0,
    .offset_start_upd = 0,
    .chunk_size = 0,
    .current_chunk = 0 };

//static OsiLockObj_t updater_LockObj;
static boot_info_t boot_info;
static uint32_t boot_info_offset;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static esp_err_t updater_spi_flash_read(size_t src, void *dest, size_t size, bool allow_decrypt);
static esp_err_t updater_spi_flash_write(size_t dest_addr, void *src, size_t size, bool write_encrypted);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

bool updater_read_boot_info (boot_info_t *boot_info, uint32_t *boot_info_offset) {
    esp_partition_info_t partition_info[PARTITIONS_COUNT];

    ESP_LOGV(TAG, "Reading boot info\n");

    if (ESP_OK != updater_spi_flash_read(CONFIG_PARTITION_TABLE_OFFSET, (void *)partition_info, sizeof(partition_info), true)) {
            ESP_LOGE(TAG, "err1\n");
            return false;
    }
    // get the data from the boot info partition
    ESP_LOGI(TAG, "read data from: 0x%X\n", partition_info[OTA_DATA_INDEX].pos.offset);
    if (ESP_OK != updater_spi_flash_read(partition_info[OTA_DATA_INDEX].pos.offset, (void *)boot_info, sizeof(boot_info_t), true)) {
            ESP_LOGE(TAG, "err2\n");
            return false;
    }
    *boot_info_offset = partition_info[OTA_DATA_INDEX].pos.offset;
    ESP_LOGD(TAG, "off: %d, status:%d, %d\n", *boot_info_offset, boot_info->Status,  boot_info->ActiveImg);
    return true;
}

bool updater_check_path (void *path) {
//    sl_LockObjLock (&updater_LockObj, SL_OS_WAIT_FOREVER);
    if (!strcmp(UPDATER_IMG_PATH, path)) {
        return true;
    }
//        sl_LockObjUnlock (&updater_LockObj);
    return false;
}

bool updater_start (void) {
    updater_data.size = IMG_SIZE;
    // check which one should be the next active image
    updater_data.offset = updater_ota_next_slot_address();

    ESP_LOGD(TAG, "Updating image at offset = 0x%6X\n", updater_data.offset);
    updater_data.offset_start_upd = updater_data.offset;

    // erase the first 2 sectors
    if (ESP_OK != spi_flash_erase_sector(updater_data.offset / SPI_FLASH_SEC_SIZE)) {
        ESP_LOGE(TAG, "Erasing first sector failed!\n");
        return false;
    }
    if (ESP_OK != spi_flash_erase_sector((updater_data.offset + SPI_FLASH_SEC_SIZE) / SPI_FLASH_SEC_SIZE)) {
        ESP_LOGE(TAG, "Erasing second sector failed!\n");
        return false;
    }

    boot_info.size = 0;
    updater_data.current_chunk = 0;

    return true;
}

bool updater_write (uint8_t *buf, uint32_t len) {

    // the actual writing into flash, not-encrypted,
    // because it already came encrypted from OTA server
    if (ESP_OK != updater_spi_flash_write(updater_data.offset, (void *)buf, len, false)) {
        ESP_LOGE(TAG, "SPI flash write failed\n");
        return false;
    }

    updater_data.offset += len;
    updater_data.current_chunk += len;
    boot_info.size += len;

    if (updater_data.current_chunk >= SPI_FLASH_SEC_SIZE) {
        updater_data.current_chunk -= SPI_FLASH_SEC_SIZE;
        // erase the next sector
        if (ESP_OK != spi_flash_erase_sector((updater_data.offset + SPI_FLASH_SEC_SIZE) / SPI_FLASH_SEC_SIZE)) {
            ESP_LOGE(TAG, "Erasing next sector failed!\n");
            return false;
        }
    }
//    sl_LockObjUnlock (&wlan_LockObj);
    return true;
}

bool updater_finish (void) {
    if (updater_data.offset > 0) {
        ESP_LOGI(TAG, "Updater finished, boot status: %d\n", boot_info.Status);
//        sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER);
        // if we still have an image pending for verification, leave the boot info as it is
        if (boot_info.Status != IMG_STATUS_CHECK) {
            ESP_LOGI(TAG, "Saving new boot info\n");
            // save the new boot info
            boot_info.PrevImg = boot_info.ActiveImg;
            if (boot_info.ActiveImg == IMG_ACT_UPDATE1) {
                boot_info.ActiveImg = IMG_ACT_UPDATE2;
            } else {
                boot_info.ActiveImg = IMG_ACT_UPDATE1;
            }
            boot_info.Status = IMG_STATUS_CHECK;

            // save the actual boot_info structure to otadata partition
            updater_write_boot_info(&boot_info, boot_info_offset);
        }
//        sl_LockObjUnlock (&wlan_LockObj);
        updater_data.offset = 0;
    }
//    sl_LockObjUnlock (&updater_LockObj);
    return true;
}

bool updater_verify (void) {
    // bootloader verifies anyway the image, but the user can check himself
    // so, the next code is adapted from bootloader/bootloader.c,

    // the last image written stats at updater_data.offset_start_upd and
    // has the lenght boot_info.size

    esp_err_t ret;
    esp_image_metadata_t data;
    const esp_partition_pos_t part_pos = {
      .offset = updater_data.offset_start_upd,
      .size = boot_info.size,
    };

    ret = esp_image_verify(ESP_IMAGE_VERIFY, &part_pos, &data);

    ESP_LOGI(TAG, "esp_image_verify: %d\n", ret);

    return (ret == ESP_OK);
}


bool updater_write_boot_info(boot_info_t *boot_info, uint32_t boot_info_offset) {

    boot_info->crc = crc32_le(UINT32_MAX, (uint8_t *)boot_info, sizeof(boot_info_t) - sizeof(boot_info->crc));
    ESP_LOGI(TAG, "Wr crc=0x%x\n", boot_info->crc);

    if (ESP_OK != spi_flash_erase_sector(boot_info_offset / SPI_FLASH_SEC_SIZE)) {
        printf("Erasing boot info failed\n");
        return false;
    }

    // saving boot info, encrypted
    esp_err_t ret; // return code of the flash_write operation
    if (esp_flash_encryption_enabled()) {
        // sizeof(boot_info_t) is 40 bytes, and we have to write multiple of 16
        // so read next 48-40 bytes from flash, and write back 48 B

        uint32_t len_aligned_16 = ((sizeof(boot_info_t) + 15) / 16) * 16;
        uint8_t *buff; // buffer used for filling boot_info data
        buff = (uint8_t *)malloc(len_aligned_16);

        if (!buff) {
            ESP_LOGE(TAG, "Can't allocate %d\n", len_aligned_16);
            return false;
        }

        // put the first sizeof(boot_info_t)
        memcpy(buff, (void *)boot_info, sizeof(boot_info_t));

        // read the next bytes
        spi_flash_read_encrypted(boot_info_offset + sizeof(boot_info_t),
                                (void *)(buff + sizeof(boot_info_t)),
                                len_aligned_16 - sizeof(boot_info_t) );

        ret = spi_flash_write_encrypted(boot_info_offset, (void *)buff, len_aligned_16);
    } else { // not-encrypted flash, just write directly boot_info
            ret = spi_flash_write(boot_info_offset, (void *)boot_info, sizeof(boot_info_t));
    }

    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Saving boot info failed\n");
    } else {
            ESP_LOGI(TAG, "Boot info saved OK\n");
    }

    return (ESP_OK == ret);
}

int updater_ota_next_slot_address() {

    int ota_offset = IMG_UPDATE1_OFFSET;

    // check which one should be the next active image
    if (updater_read_boot_info (&boot_info, &boot_info_offset)) {
        // if we still have an image pending for verification, keep overwriting it
        if (boot_info.Status == IMG_STATUS_CHECK) {
            if(boot_info.ActiveImg == IMG_ACT_FACTORY)

            {
                ota_offset = IMG_FACTORY_OFFSET;
            }
            else
            {
                ota_offset = IMG_UPDATE1_OFFSET;
            }
        }
        else
        {
            if(boot_info.ActiveImg == IMG_ACT_FACTORY)

            {
                ota_offset = IMG_UPDATE1_OFFSET;
            }
            else
            {
                ota_offset = IMG_FACTORY_OFFSET;
            }
        }
    }

    ESP_LOGI(TAG, "Next slot address: 0x%6X\n", ota_offset);

    return ota_offset;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static esp_err_t updater_spi_flash_read(size_t src, void *dest, size_t size, bool allow_decrypt)
{
    if (allow_decrypt && esp_flash_encryption_enabled()) {
        return spi_flash_read_encrypted(src, dest, size);
    } else {
        return spi_flash_read(src, dest, size);
    }
}

/* @note Both dest_addr and size must be multiples of 16 bytes. For
 * absolute best performance, both dest_addr and size arguments should
 * be multiples of 32 bytes.
*/
static esp_err_t updater_spi_flash_write(size_t dest_addr, void *src, size_t size,
                                        bool write_encrypted)
{
    if (write_encrypted && esp_flash_encryption_enabled()) {
        return spi_flash_write_encrypted(dest_addr, src, size);
    } else {
        return spi_flash_write(dest_addr, src, size);
    }
}
