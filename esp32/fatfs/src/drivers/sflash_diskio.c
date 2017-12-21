#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "diskio.h"
#include "sflash_diskio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_spi_flash.h"


static uint8_t *sflash_block_cache;
static bool sflash_cache_is_dirty;
static uint32_t sflash_prev_block_addr;
static bool sflash_init_done = false;

static uint32_t sflash_start_address;
static uint32_t sflash_fs_sector_count;


static bool sflash_write (void) {
    // erase the block first
    if (ESP_OK == spi_flash_erase_sector(sflash_prev_block_addr / SFLASH_BLOCK_SIZE)) {
        // then write it
        return (spi_flash_write(sflash_prev_block_addr, (void *)sflash_block_cache, SFLASH_BLOCK_SIZE) == ESP_OK);
    }
    return false;
}

DRESULT sflash_disk_init (void) {
    if (!sflash_init_done) {
        if (spi_flash_get_chip_size() > (4 * 1024 * 1024)) {
            sflash_start_address = SFLASH_START_ADDR_8MB;
            sflash_fs_sector_count = SFLASH_FS_SECTOR_COUNT_8MB;
        } else {
            sflash_start_address = SFLASH_START_ADDR_4MB;
            sflash_fs_sector_count = SFLASH_FS_SECTOR_COUNT_4MB;
        }
        sflash_block_cache = (uint8_t *)heap_caps_malloc(SFLASH_BLOCK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        sflash_prev_block_addr = UINT32_MAX;
        sflash_cache_is_dirty = false;
        sflash_init_done = true;
    }
    return RES_OK;
}

DRESULT sflash_disk_status(void) {
    if (!sflash_init_done) {
        return STA_NOINIT;
    }
    return RES_OK;
}

DRESULT sflash_disk_read(BYTE *buff, DWORD sector, UINT count) {
    uint32_t secindex;

    if ((sector + count > sflash_fs_sector_count) || !count) {
        // TODO sl_LockObjLock (&flash_LockObj, SL_OS_WAIT_FOREVER);
        sflash_disk_flush();
        // TODO sl_LockObjUnlock (&flash_LockObj);
        return RES_PARERR;
    }

    // TODO sl_LockObjLock (&flash_LockObj, SL_OS_WAIT_FOREVER);
    for (int index = 0; index < count; index++) {
        secindex = (sector + index) % SFLASH_SECTORS_PER_BLOCK;
        uint32_t sflash_block_addr = sflash_start_address + (((sector + index) / SFLASH_SECTORS_PER_BLOCK) * SFLASH_BLOCK_SIZE);
        // Check if it's a different block than last time
        if (sflash_prev_block_addr != sflash_block_addr) {
            if (sflash_disk_flush() != RES_OK) {
                // TODO sl_LockObjUnlock (&flash_LockObj);
                return RES_ERROR;
            }
            sflash_prev_block_addr = sflash_block_addr;
            if (ESP_OK != spi_flash_read(sflash_block_addr, (void *)sflash_block_cache, SFLASH_BLOCK_SIZE)) {
                // TODO sl_LockObjUnlock (&flash_LockObj);
                return RES_ERROR;
            }
        }
        // Copy the requested sector from the block cache
        memcpy (buff, (void *)&sflash_block_cache[secindex * SFLASH_FS_SECTOR_SIZE], SFLASH_FS_SECTOR_SIZE);
        buff += SFLASH_FS_SECTOR_SIZE;
    }

    // TODO sl_LockObjUnlock (&flash_LockObj);
    return RES_OK;
}

DRESULT sflash_disk_write(const BYTE *buff, DWORD sector, UINT count) {
    uint32_t secindex;
    uint32_t index = 0;

    if ((sector + count > sflash_fs_sector_count) || !count) {
        // TODO sl_LockObjLock (&flash_LockObj, SL_OS_WAIT_FOREVER);
        sflash_disk_flush();
        // TODO sl_LockObjUnlock (&flash_LockObj);
        return RES_PARERR;
    }

    // TODO sl_LockObjLock (&flash_LockObj, SL_OS_WAIT_FOREVER);
    do {
        secindex = (sector + index) % SFLASH_SECTORS_PER_BLOCK;
        uint32_t sflash_block_addr = sflash_start_address + (((sector + index) / SFLASH_SECTORS_PER_BLOCK) * SFLASH_BLOCK_SIZE);
        // Check if it's a different block than last time
        if (sflash_prev_block_addr != sflash_block_addr) {
            if (sflash_disk_flush() != RES_OK) {
                // TODO sl_LockObjUnlock (&flash_LockObj);
                return RES_ERROR;
            }
            sflash_prev_block_addr = sflash_block_addr;
            if (ESP_OK != spi_flash_read(sflash_block_addr, (void *)sflash_block_cache, SFLASH_BLOCK_SIZE)) {
//                // TODO sl_LockObjUnlock (&flash_LockObj);
                return RES_ERROR;
            }
        }
        // copy the input sector to the block cache
        memcpy ((void *)&sflash_block_cache[secindex * SFLASH_FS_SECTOR_SIZE], buff, SFLASH_FS_SECTOR_SIZE);
        buff += SFLASH_FS_SECTOR_SIZE;
        sflash_cache_is_dirty = true;
    } while (++index < count);

    // TODO sl_LockObjUnlock (&flash_LockObj);
    return RES_OK;
}

DRESULT sflash_disk_flush (void) {
    // write back the cache if it's dirty
    if (sflash_cache_is_dirty) {
        if (!sflash_write()) {
            return RES_ERROR;
        }
        sflash_cache_is_dirty = false;
    }
    return RES_OK;
}

uint32_t sflash_get_sector_count(void) {
    return sflash_fs_sector_count;
}
