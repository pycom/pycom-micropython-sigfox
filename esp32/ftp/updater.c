/*
 * Copyright (c) 2018, Pycom Limited.
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

#include "ff.h"

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

    if (ESP_OK != updater_spi_flash_read(ESP_PARTITION_TABLE_ADDR, (void *)partition_info, sizeof(partition_info), true)) {
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

#define FILE_READ_SIZE                      256

STATIC char *updater_read_file (const char *file_path, vstr_t *vstr) {
    vstr_init(vstr, FILE_READ_SIZE);
    char *filebuf = vstr->buf;
    mp_uint_t actualsize;
    mp_uint_t totalsize = 0;

    FIL fp;
    FRESULT res = f_open(&fp, file_path, FA_READ);
    if (res != FR_OK) {
        return NULL;
    }

    while (true) {
        FRESULT res = f_read(&fp, filebuf, FILE_READ_SIZE, (UINT *)&actualsize);
        if (res != FR_OK) {
            f_close(&fp);
            return NULL;
        }
        totalsize += actualsize;
        if (actualsize < FILE_READ_SIZE) {
            break;
        } else {
            filebuf = vstr_extend(vstr, FILE_READ_SIZE);
        }
    }
    f_close(&fp);

    vstr->len = totalsize;
    vstr_null_terminated_str(vstr);
    return vstr->buf;
}

bool updater_start (void) {

    //Reading OTA_DATA
    printf("Reading boot info..\n");
    boot_info_t boot_info_local;
    uint32_t boot_info_offset_local;
    if(true == updater_read_boot_info(&boot_info_local, &boot_info_offset_local)) {
        printf("Boot info read successfully...\n");
    }

    printf("Checking OLD partition table:\n");
    esp_partition_info_t partition_info_OLD[PARTITIONS_COUNT];
    if (ESP_OK != updater_spi_flash_read(ESP_PARTITION_TABLE_ADDR, (void *)partition_info_OLD, sizeof(partition_info_OLD), true)) {
            ESP_LOGE(TAG, "err1\n");
            return false;
    }
    int i = 0;
    for(; i < PARTITIONS_COUNT; i++) {
        printf("%d offset: 0x%X\n", i, partition_info_OLD[i].pos.offset);
        printf("%d size: %d\n",   i, partition_info_OLD[i].pos.size);
        printf("%d magic: 0x%X\n",   i, partition_info_OLD[i].magic);
        printf("%d type: 0x%X\n",   i, partition_info_OLD[i].type);
        printf("%d subtype: 0x%X\n",   i, partition_info_OLD[i].subtype);
    }

    printf("Erasing partition table...\n");
//     Update partition table
    if (ESP_OK != spi_flash_erase_sector(ESP_PARTITION_TABLE_ADDR / SPI_FLASH_SEC_SIZE)) {
            ESP_LOGE(TAG, "Erasing partition table sector failed!\n");
            printf("Erasing partition table sector failed!\n");
            return false;
    }


    // Read the new partition table
    vstr_t vstr;
    char* buf = updater_read_file("partitions.bin", &vstr);
    printf("Partition table file has been read...\n");
    if(buf != NULL) {
        printf("Writing partition table...\n");
        // Writing the new partition table
        if (ESP_OK != spi_flash_write(0x8000, (void *)buf, vstr.len)) {
            ESP_LOGE(TAG, "SPI flash write failed\n");
            printf("SPI flash write failed\n");
            return false;
        }
        printf("Partition table has been written successfully!\n");
    }
    else {
        ESP_LOGE(TAG, "Reading new partition table file failed...\n");
        printf("Reading new partition table file failed...\n");
        return false;
    }

    printf("Checking new partition table:\n");
    esp_partition_info_t partition_info_NEW[PARTITIONS_COUNT];
    if (ESP_OK != updater_spi_flash_read(ESP_PARTITION_TABLE_ADDR, (void *)partition_info_NEW, sizeof(partition_info_NEW), true)) {
            ESP_LOGE(TAG, "err1\n");
            return false;
    }
    for(i = 0; i < PARTITIONS_COUNT; i++) {
        printf("%d offset: 0x%X\n", i, partition_info_NEW[i].pos.offset);
        printf("%d size: %d\n",   i, partition_info_NEW[i].pos.size);
        printf("%d magic: 0x%X\n",   i, partition_info_NEW[i].magic);
        printf("%d type: 0x%X\n",   i, partition_info_NEW[i].type);
        printf("%d subtype: 0x%X\n",   i, partition_info_NEW[i].subtype);
    }

    printf("Erasing the new sector of boot info...\n");
    //     Update partition table
    if (ESP_OK != spi_flash_erase_sector(partition_info_NEW[OTA_DATA_INDEX].pos.offset / SPI_FLASH_SEC_SIZE)) {
            ESP_LOGE(TAG, "Erasing new sector of boot info failed!\n");
            printf("Erasing new sector of boot info failed!\n");
            return false;
    }
    printf("Writing back boot info...\n");
    if (ESP_OK != spi_flash_write(partition_info_NEW[OTA_DATA_INDEX].pos.offset, (void *)&boot_info_local, sizeof(boot_info_t))) {
        ESP_LOGE(TAG, "Writing back boot info failed\n");
        printf("Writing back boot info failed\n");
        return false;
    }
    printf("Boot info has been written successfully!\n");

    printf("Checking the new boot info:\n");
    boot_info_t boot_info_local_NEW;
    uint32_t boot_info_offset_local_NEW;
    if(true == updater_read_boot_info(&boot_info_local_NEW, &boot_info_offset_local_NEW)) {
           printf("Boot info read successfully...\n");
   }
    printf("Writing out informations...\n");

    printf("OLD offset: 0x%X\n", boot_info_offset_local);
    printf("NEW offset: 0x%X\n", boot_info_offset_local_NEW);

    printf("OLD ActiveImg: %d\n", boot_info_local.ActiveImg);
    printf("NEW ActiveImg: %d\n", boot_info_local_NEW.ActiveImg);

    printf("OLD status: %d\n", boot_info_local.Status);
    printf("NEW status: %d\n", boot_info_local_NEW.Status);

    printf("OLD PrevImg: %d\n", boot_info_local.PrevImg);
    printf("NEW PrevImg: %d\n", boot_info_local_NEW.PrevImg);

    printf("OLD Crc: %u\n", boot_info_local.crc);
    printf("NEW Crc: %u\n", boot_info_local_NEW.crc);

    printf("OLD safeboot: %u\n", boot_info_local.safeboot);
    printf("NEW safeboot: %u\n", boot_info_local_NEW.safeboot);

    printf("OLD size: %u\n", boot_info_local.size);
    printf("NEW size: %u\n", boot_info_local_NEW.size);

    printf("Writing out informations FINISHED\n");


    updater_data.size = IMG_SIZE;
    // check which one should be the next active image
    updater_data.offset = updater_ota_next_slot_address();

    printf("updater_data.offset: 0x%x\n", updater_data.offset);

    ESP_LOGD(TAG, "Updating image at offset = 0x%6X\n", updater_data.offset);
    printf("Updating image at offset = 0x%6X\n", updater_data.offset);
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
        printf("updater_write: SPI flash write failed\n");
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
            printf("updater_write: Erasing next sector failed!\n");
            return false;
        }
    }
//    sl_LockObjUnlock (&wlan_LockObj);
    return true;
}

bool updater_finish (void) {
    if (updater_data.offset > 0) {
        ESP_LOGI(TAG, "Updater finished, boot status: %d\n", boot_info.Status);
        printf("Updater finished, boot status: %d\n", boot_info.Status);
//        sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER);
        // if we still have an image pending for verification, leave the boot info as it is
        if (boot_info.Status != IMG_STATUS_CHECK) {
            ESP_LOGI(TAG, "Saving new boot info\n");
            printf("Saving new boot info\n");
            // save the new boot info
            boot_info.PrevImg = boot_info.ActiveImg;
            if (boot_info.ActiveImg == IMG_ACT_UPDATE1) {
                boot_info.ActiveImg = IMG_ACT_UPDATE2;
            } else {
                boot_info.ActiveImg = IMG_ACT_UPDATE1;
            }
            boot_info.Status = IMG_STATUS_CHECK;

            printf("Calling updater_write_boot_info with address: 0x%X\n", boot_info_offset);
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

    printf("Verifying: 0x%X\n", updater_data.offset_start_upd);

    ret = esp_image_load(ESP_IMAGE_VERIFY, &part_pos, &data);

    ESP_LOGI(TAG, "esp_image_load: %d\n", ret);
    printf("esp_image_load: %d\n", ret);

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
            printf("Boot info saved OK\n");
    }

    return (ESP_OK == ret);
}

int updater_ota_next_slot_address() {

    int ota_offset = IMG_UPDATE1_OFFSET;

    // check which one should be the next active image
    if (updater_read_boot_info (&boot_info, &boot_info_offset)) {
        // if we still have an image pending for verification, keep overwriting it
        if ((boot_info.Status == IMG_STATUS_CHECK && boot_info.ActiveImg == IMG_ACT_UPDATE2) ||
            (boot_info.ActiveImg == IMG_ACT_UPDATE1 && boot_info.Status != IMG_STATUS_CHECK)) {
                ota_offset = IMG_UPDATE2_OFFSET;
        }
    }

    ESP_LOGI(TAG, "Next slot address: 0x%6X\n", ota_offset);
    printf("Next slot address: 0x%6X\n", ota_offset);

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
