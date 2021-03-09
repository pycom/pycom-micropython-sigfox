/*
 * Copyright (c) 2021, Pycom Limited.
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
#include "esp32chipinfo.h"

#ifdef DIFF_UPDATE_ENABLED
#include "bzlib.h"
#include "bsdiff_api.h"
#endif

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
static bool updater_is_delta_file(void);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

bool updater_read_boot_info (boot_info_t *boot_info, uint32_t *boot_info_offset) {
    esp_partition_info_t partition_info[PARTITIONS_COUNT_4MB];

    uint8_t part_count = (esp32_get_chip_rev() > 0 ? PARTITIONS_COUNT_8MB : PARTITIONS_COUNT_4MB);
    ESP_LOGV(TAG, "Reading boot info\n");

    if (ESP_OK != updater_spi_flash_read(CONFIG_PARTITION_TABLE_OFFSET, (void *)partition_info, (sizeof(esp_partition_info_t) * part_count), true)) {
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

    updater_data.size = (esp32_get_chip_rev() > 0 ? IMG_SIZE_8MB : IMG_SIZE_4MB);
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

#ifdef DIFF_UPDATE_ENABLED
bool updater_patch(void) {

    const int HEADER_LEN            = 32;
    const int MAGIC_BYTES_LEN       = 8;
    const int CTRLEN_OFFSET         = 8;
    const int DATALEN_OFFSET        = 16;
    const int NEWFILE_SIZE_OFFSET   = 24;

    bool status = false;                    // Status to be returned (true for success, false otherwise)
    unsigned char header[HEADER_LEN], buf[8];

    uint32_t patch_offset;                  // Offset of the patch file in the flash
    uint32_t patch_size;                    // Size of the patch file
    uint32_t old_bin_offset;                // Offset of the old/current binary image in the flash
    uint32_t newsize;
    uint32_t bzctrllen, bzdatalen, xtralen; // Lengths of various blocks in the patch file

    printf("Patching the binary...\n");
    // Since we haven't switched the active partition, the next partition
    // returned by this function will be the one containing the downloaded patch
    // file NOTE: This also reads the BOOT INFO so we don't have to explicitly
    // read it
    patch_offset = updater_ota_next_slot_address();
    patch_size = boot_info.size;            // boot_info.patch_size;

    // Getting the offset of the current image in the flash
    if (boot_info.ActiveImg == IMG_ACT_FACTORY) {
        old_bin_offset = IMG_FACTORY_OFFSET;
    } else {
        old_bin_offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);
    }

    ESP_LOGI(TAG, "Old_Offset: %d, Offset: %d, Size: %d, ChunkSize: %d, Chunk: %d\n",
             old_bin_offset, updater_data.offset, updater_data.size, updater_data.chunk_size, updater_data.current_chunk);
    ESP_LOGI(TAG, "BootInfoSize: %d, BootInfoActiveImg: %d\n",
             boot_info.size, boot_info.ActiveImg);

    // File format:
    //     0   8   "BSDIFF40"
    //     8   8   X
    //     16  8   Y
    //     24  8   sizeof(newfile)
    //     32  X   bzip2(control block)
    //     32+X    Y   bzip2(diff block)
    //     32+X+Y  ??? bzip2(extra block)
    // with control block a set of triples (x,y,z) meaning "add x bytes
    // from oldfile to x bytes from the diff block; copy y bytes from the
    // extra block; seek forwards in oldfile by z bytes".

    // Reading header of the patch file

    if (ESP_OK != updater_spi_flash_read(patch_offset, header, HEADER_LEN, false)) {
        printf("Error while reading patch file header\n");
        goto return_status;
    }

    // Check for the appropriate magic
    if (memcmp(header, "BSDIFF40", MAGIC_BYTES_LEN) != 0) {
        printf("Invalid header\n");
        goto return_status;
    }

    ESP_LOGI(TAG,"Header Verified\n");

    // Reading lengths from header
    bzctrllen = offtin(header + CTRLEN_OFFSET);
    bzdatalen = offtin(header + DATALEN_OFFSET);
    newsize = offtin(header + NEWFILE_SIZE_OFFSET);

    xtralen = patch_size - (HEADER_LEN + bzctrllen + bzdatalen);

    ESP_LOGD(TAG, "CtrlLen: %d, DataLen: %d, NewSize: %d, ExtraLen: %d\n", bzctrllen, bzdatalen, newsize, xtralen);

    if ((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0) || (xtralen < 0)) {
        printf("Invalid Block Sizes CtrlLen: %d, DataLen: %d, NewSize: %d, ExtraLen: %d\n", bzctrllen, bzdatalen, newsize, xtralen);
        goto return_status;
    }

    // Header is valid, reading the contents of the patch file
    {
        const int FLASH_READ_WRITE_SIZE = 512;              // Number of bytes read/written to flash at a time

        uint16_t i = 0;
        unsigned char *out_buf = NULL;
        unsigned char *diff_ptr = NULL;
        unsigned char *xtra_ptr = NULL;
        unsigned char old_bin_buf[FLASH_READ_WRITE_SIZE];   // Buffer to read parts of old binary

        unsigned char *patch_buf;

        void *ctrl_strm = NULL;
        void *diff_strm = NULL;
        void *xtra_strm = NULL;

        int ctrl[3];                                        // Buffer to read control block values from the patch file (NOTE: Its value can be negative)
                                                            // Used to read (x,y,z) tuples from the Control Block
        int oldpos = 0;                                     // Read pointer for old binary
        int newpos = 0;                                     // Read pointer for the patched binary

        unsigned int ctrl_len, diff_len, xtra_len;
        int ret = 0;

        // Reading the complete patch file
        patch_buf = heap_caps_malloc(patch_size, MALLOC_CAP_SPIRAM);
        if (patch_buf == NULL) {
            printf("Failed to allocate %d bytes for the Patch File\n", patch_size);
            goto free_mem_and_ret;
        }

        updater_spi_flash_read(patch_offset, patch_buf, patch_size, false);

        // Creating BZLIB streams for decompression
        ret = BZ2_bzDecompressStreamInit(&ctrl_strm, (char *)patch_buf + 32, bzctrllen);
        if (ret != BZ_OK) {
            printf("Failed to init stream for CTRL block. Error code: %d\n", ret);
            goto free_mem_and_ret;
        }
        ret = BZ2_bzDecompressStreamInit(&diff_strm, (char *)patch_buf + 32 + bzctrllen, bzdatalen);
        if (ret != BZ_OK) {
            printf("Failed to init stream for DIFF block. Error code: %d\n", ret);
            goto free_mem_and_ret;
        }
        ret = BZ2_bzDecompressStreamInit(&xtra_strm, (char *)patch_buf + 32 + bzctrllen + bzdatalen, xtralen);
        if (ret != BZ_OK) {
            printf("Failed to init stream for EXTRA block. Error code: %d\n", ret);
            goto free_mem_and_ret;
        }

        // Memory for the buffer to store the decompressed blocks
        const int AVAIL_MEM = 100 * 1024;

        ESP_LOGD(TAG, "Going to allocate memory for decompressed blocks buffer: %d\n", AVAIL_MEM);
        out_buf = heap_caps_malloc(AVAIL_MEM, MALLOC_CAP_SPIRAM);

        if (out_buf == NULL) {
            printf("Failed to allocate the buffer memory of size %d\n", AVAIL_MEM);
            goto free_mem_and_ret;
        }

        // Starting the patching

        // Initializing the parameters of the updater so that the next write is
        // done from the start of the partition
        if (!updater_start()) {
            printf("Failed to START UPDATER\n");
            goto free_mem_and_ret;
        }

        while (newpos < newsize) {
            // Reading the control data
            for (i = 0; i <= 2; i++) {
                ctrl_len = 8;
                ret = BZ2_bzDecompressRead(ctrl_strm, (char *)buf, &ctrl_len);
                if (ret != BZ_OK || ctrl_len != 8) {
                    printf("PATCHING: Unable to decompress required bytes. ret: %d, ctrl_len: %d, i: %d\n", ret, ctrl_len, i);
                    goto free_mem_and_ret;
                }

                ctrl[i] = offtin(buf);
            }

            // Sanity-check
            if (newpos + ctrl[0] > newsize) {
                printf("PATCHING: Corrupt Patch. Violated newsize: %d, ctrl[0]: %d\n", newsize, ctrl[0]);
                goto free_mem_and_ret;
            }

            // Decompressing ctrl[0] bytes of diff block
            while (ctrl[0] > 0) {
                unsigned int byte_count = 0;

                if (ctrl[0] > AVAIL_MEM) {
                    ctrl[0] -= AVAIL_MEM;
                    diff_len = AVAIL_MEM;
                }
                else {
                    diff_len = ctrl[0];
                    ctrl[0] = 0;
                }

                ret = BZ2_bzDecompressRead(diff_strm, (char *)out_buf, &diff_len);

                if (ret != BZ_OK) {
                    printf("PATCHING: Unable to decompress required bytes. ret: %d, ctrl[0]: %d, diff_len: %d\n", ret, ctrl[0], diff_len);
                    goto free_mem_and_ret;
                }
                diff_ptr = out_buf;

                // Reading old binary file in chunks combining it with the Diff
                // bytes
                do {
                    int bytes_to_read = 0;

                    if ((diff_len - byte_count) > FLASH_READ_WRITE_SIZE) {
                        bytes_to_read = FLASH_READ_WRITE_SIZE;
                    } else {
                        bytes_to_read = diff_len - byte_count;
                    }

                    if (ESP_OK != updater_spi_flash_read(old_bin_offset + oldpos, old_bin_buf, bytes_to_read, false)) {
                        printf("Error while reading old bin block. old_bin_offset: %u, oldpos: %u, bytes_to_read: %d\n", old_bin_offset, oldpos, bytes_to_read);
                        goto free_mem_and_ret;
                    }

                    for (i = 0; i < bytes_to_read; i++) {
                        *(diff_ptr + i) += old_bin_buf[i];
                    }

                    if (!updater_write(diff_ptr, bytes_to_read)) {
                        printf("Failed to write %d bytes to the Flash\n", bytes_to_read);
                        goto free_mem_and_ret;
                    }

                    diff_ptr += bytes_to_read;
                    oldpos += bytes_to_read;
                    newpos += bytes_to_read;
                    byte_count += bytes_to_read;

                } while (byte_count < diff_len);
            }

            // Sanity-check
            if (newpos + ctrl[1] > newsize) {
                printf("PATCHING: Corrupt patch, ctrl[1]: %d violated newsize: %d\n", (int)ctrl[1], (int)newsize);
                goto free_mem_and_ret;
            }

            // Decompressing Extra Bytes of size ctrl[1]
            while (ctrl[1] > 0) {
                if (ctrl[1] > AVAIL_MEM) {
                    ctrl[1] -= AVAIL_MEM;
                    xtra_len = AVAIL_MEM;
                } else {
                    xtra_len = ctrl[1];
                    ctrl[1] = 0;
                }

                ret = BZ2_bzDecompressRead(xtra_strm, (char *)out_buf, &xtra_len);

                if (ret != BZ_OK) {
                    printf("PATCHING: Unable to decompress required bytes. ret: %d, ctrl[1]: %d, xtra_len: %d\n", ret, ctrl[1], xtra_len);
                    goto free_mem_and_ret;
                }
                xtra_ptr = out_buf;

                // Writing the bytes from the Extra Block
                int write_size = xtra_len;
                while (write_size > FLASH_READ_WRITE_SIZE) {
                    if (!updater_write(xtra_ptr + xtra_len - write_size, FLASH_READ_WRITE_SIZE)) {
                        printf("Failed to write %d bytes from Extra Block to the Flash\n", FLASH_READ_WRITE_SIZE);
                        goto free_mem_and_ret;
                    }
                    write_size -= FLASH_READ_WRITE_SIZE;
                }

                if (write_size) {
                    if (!updater_write(xtra_ptr + xtra_len - write_size, write_size)) {
                        printf("Failed to write %d bytes from Extra Block to the Flash\n", write_size);
                        goto free_mem_and_ret;
                    }
                }
                newpos += xtra_len;
            }

            // Adjust the pointers
            oldpos += ctrl[2];
        }

        ESP_LOGI(TAG, "UPDATER_PATCH: PATCHED: %10d sized file\n", (int)newpos);

        ESP_LOGD(TAG, "UPDATER_PATCH: Old_Offset: %d, Offset: %d, Size: %d, ChunkSize: %d, Chunk: %d\n",
                 old_bin_offset, updater_data.offset, updater_data.size,
                 updater_data.chunk_size, updater_data.current_chunk);

        status = true;
        printf("Patching SUCCESSFUL.\n");

    free_mem_and_ret:
        heap_caps_free(patch_buf);
        heap_caps_free(out_buf);
        if (ctrl_strm) {
            BZ2_bzDecompressStreamEnd(ctrl_strm);
        }
        if (diff_strm) {
            BZ2_bzDecompressStreamEnd(diff_strm);
        }
        if (xtra_strm) {
            BZ2_bzDecompressStreamEnd(xtra_strm);
        }
    }

return_status:
    if (status) {
        // Updating BOOT INFO
        boot_info.PrevImg = boot_info.ActiveImg;

        if (boot_info.ActiveImg == IMG_ACT_UPDATE1) {
            boot_info.ActiveImg = IMG_ACT_UPDATE2;
        } else {
            boot_info.ActiveImg = IMG_ACT_UPDATE1;
        }
        boot_info.Status = IMG_STATUS_CHECK;
    } else {
        // In case of failure we don't change the active image in boot info
        // Changing the status to READY since new image will not be loaded
        boot_info.Status = IMG_STATUS_READY;
    }

    // save the actual boot_info structure to otadata partition
    updater_write_boot_info(&boot_info, boot_info_offset);
    updater_data.offset = 0;

    return status;
}
#endif

bool updater_finish (void) {
    if (updater_data.offset > 0) {
        ESP_LOGI(TAG, "Updater finished, boot status: %d\n", boot_info.Status);
//        sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER);
        // if we still have an image pending for verification, leave the boot info as it is
        if (boot_info.Status != IMG_STATUS_CHECK) {

            if(updater_is_delta_file()) {
#ifdef DIFF_UPDATE_ENABLED
                printf("Differential Update Image detected. Restart the device to apply the patch.\n");
                boot_info.Status = IMG_STATUS_PATCH;
                updater_write_boot_info(&boot_info, boot_info_offset);
#else
                printf("Differential Update Image detected. This feature is disabled in the build. Enable DIFF_UPDATE_ENABLED in the build to use it.\n");
                updater_data.offset = 0;

                return false;
#endif
            }else {
                printf("Full Update Image detected. Restart the device to load the new firmware.\n");
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

    int ota_offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);

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
                ota_offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);
            }
        }
        else
        {
            if(boot_info.ActiveImg == IMG_ACT_FACTORY)

            {
                ota_offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);
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

/* @brief Checks whether the image present in the inactive partition a patch
 * file or not.
 */
static bool updater_is_delta_file(void)
{
    const int HEADER_LEN      = 32;
    const int MAGIC_BYTES_LEN = 8;

    unsigned int offset;
    unsigned char header[HEADER_LEN];

    // Basically doing all that updater_ota_next_slot_address() does minus reading the bootinfo since we haven't saved it yet.
    if (boot_info.Status == IMG_STATUS_CHECK)
    {
        if (boot_info.ActiveImg == IMG_ACT_FACTORY)

        {
            offset = IMG_FACTORY_OFFSET;
        }
        else
        {
            offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);
        }
    }
    else
    {
        if (boot_info.ActiveImg == IMG_ACT_FACTORY)
        {
            offset = (esp32_get_chip_rev() > 0 ? IMG_UPDATE1_OFFSET_8MB : IMG_UPDATE1_OFFSET_4MB);
        }
        else
        {
            offset = IMG_FACTORY_OFFSET;
        }
    }

    if(ESP_OK != updater_spi_flash_read(offset, header, HEADER_LEN, false))
    {
        printf("Error while reading patch file header\n");
        return false;
    }

    // Check for appropriate magic
    if (memcmp(header, "BSDIFF40", MAGIC_BYTES_LEN) != 0)
    {
        return false;
    }

    return true;
}
