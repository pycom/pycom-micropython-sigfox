#ifndef SFLASH_DISKIO_H_
#define SFLASH_DISKIO_H_

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "mpconfigport.h"

#define SFLASH_BLOCK_SIZE               SPI_FLASH_SEC_SIZE
#define SFLASH_BLOCK_COUNT              MICROPY_PORT_SFLASH_BLOCK_COUNT
#define SFLASH_FS_SECTOR_SIZE           512
#define SFLASH_FS_SECTOR_COUNT          ((SFLASH_BLOCK_SIZE * SFLASH_BLOCK_COUNT) / SFLASH_FS_SECTOR_SIZE)
#define SFLASH_SECTORS_PER_BLOCK        (SFLASH_BLOCK_SIZE / SFLASH_FS_SECTOR_SIZE)
#define SFLASH_START_ADDR               0x00380000
#define SFLASH_START_BLOCK              (SFLASH_START_ADDR / SFLASH_BLOCK_SIZE)
#define SFLASH_END_BLOCK                (SFLASH_START_BLOCK + (SFLASH_BLOCK_COUNT - 1))

DRESULT sflash_disk_init(void);
DRESULT sflash_disk_status(void);
DRESULT sflash_disk_read(BYTE *buff, DWORD sector, UINT count);
DRESULT sflash_disk_write(const BYTE *buff, DWORD sector, UINT count);
DRESULT sflash_disk_flush(void);

#endif /* SFLASH_DISKIO_H_ */
