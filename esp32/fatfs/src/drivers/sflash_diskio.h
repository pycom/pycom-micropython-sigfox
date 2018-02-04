#ifndef SFLASH_DISKIO_H_
#define SFLASH_DISKIO_H_

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "ff.h"

#include "mpconfigport.h"

#define SFLASH_BLOCK_SIZE               SPI_FLASH_SEC_SIZE
#define SFLASH_FS_SECTOR_SIZE           512
#define SFLASH_SECTORS_PER_BLOCK        (SFLASH_BLOCK_SIZE / SFLASH_FS_SECTOR_SIZE)

#define SFLASH_BLOCK_COUNT_4MB          MICROPY_PORT_SFLASH_BLOCK_COUNT_4MB
#define SFLASH_FS_SECTOR_COUNT_4MB      ((SFLASH_BLOCK_SIZE * SFLASH_BLOCK_COUNT_4MB) / SFLASH_FS_SECTOR_SIZE)
#define SFLASH_START_ADDR_4MB           0x00380000
#define SFLASH_START_BLOCK_4MB          (SFLASH_START_ADDR_4MB / SFLASH_BLOCK_SIZE)
#define SFLASH_END_BLOCK_4MB            (SFLASH_START_BLOCK_4MB + (SFLASH_BLOCK_COUNT - 1))

#define SFLASH_BLOCK_COUNT_8MB          MICROPY_PORT_SFLASH_BLOCK_COUNT_8MB
#define SFLASH_FS_SECTOR_COUNT_8MB      ((SFLASH_BLOCK_SIZE * SFLASH_BLOCK_COUNT_8MB) / SFLASH_FS_SECTOR_SIZE)
#define SFLASH_START_ADDR_8MB           0x00400000
#define SFLASH_START_BLOCK_8MB          (SFLASH_START_ADDR_8MB / SFLASH_BLOCK_SIZE)
#define SFLASH_END_BLOCK_8MB            (SFLASH_START_BLOCK_8MB + (SFLASH_BLOCK_COUNT - 1))

DRESULT sflash_disk_init(void);
DRESULT sflash_disk_status(void);
DRESULT sflash_disk_read(BYTE *buff, DWORD sector, UINT count);
DRESULT sflash_disk_write(const BYTE *buff, DWORD sector, UINT count);
DRESULT sflash_disk_flush(void);
uint32_t sflash_get_sector_count(void);

#endif /* SFLASH_DISKIO_H_ */
