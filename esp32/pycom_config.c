/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "diskio.h"
#include "sflash_diskio.h"
#include "pycom_config.h"


#define CONFIG_DATA_FLASH_BLOCK         (SFLASH_START_BLOCK + SFLASH_BLOCK_COUNT)
#define CONFIG_DATA_FLASH_ADDR          (SFLASH_START_ADDR + (SFLASH_BLOCK_COUNT * SFLASH_BLOCK_SIZE))

static bool config_write (void);

static pycom_config_t pycom_config;

void config_init0 (void) {
    // read the config struct from flash
    spi_flash_read(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config, sizeof(pycom_config));
}

bool config_set_lora_mac (const uint8_t *mac) {
    memcpy(pycom_config.lora_mac, mac, sizeof(pycom_config.lora_mac));
    return config_write();
}

void config_get_lora_mac (uint8_t *mac) {
    memcpy(mac, pycom_config.lora_mac, sizeof(pycom_config.lora_mac));
}

static bool config_write (void) {
    // erase the block first
    if (ESP_OK == spi_flash_erase_sector(CONFIG_DATA_FLASH_BLOCK)) {
        // then write it
        return (spi_flash_write(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config, sizeof(pycom_config)) == ESP_OK);
    }
    return false;
}
