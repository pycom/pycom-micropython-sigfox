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

bool config_set_lpwan_mac (const uint8_t *mac) {
    memcpy(pycom_config.lpwan_mac, mac, sizeof(pycom_config.lpwan_mac));
    return config_write();
}

void config_get_lpwan_mac (uint8_t *mac) {
    memcpy(mac, pycom_config.lpwan_mac, sizeof(pycom_config.lpwan_mac));
}

bool config_set_sigfox_id (uint8_t *id) {
    memcpy(pycom_config.sigfox_id, id, sizeof(pycom_config.sigfox_id));
    return config_write();
}

void config_get_sigfox_id (uint8_t *id) {
    memcpy(id, pycom_config.sigfox_id, sizeof(pycom_config.sigfox_id));
}

bool config_set_sigfox_pac (uint8_t *pac) {
    memcpy(pycom_config.sigfox_pac, pac, sizeof(pycom_config.sigfox_pac));
    return config_write();
}

void config_get_sigfox_pac (uint8_t *pac) {
    memcpy(pac, pycom_config.sigfox_pac, sizeof(pycom_config.sigfox_pac));
}

bool config_set_sigfox_public_key (uint8_t *public_key) {
    memcpy(pycom_config.sigfox_public_key, public_key, sizeof(pycom_config.sigfox_public_key));
    return config_write();
}

void config_get_sigfox_public_key (uint8_t *public_key) {
    memcpy(public_key, pycom_config.sigfox_public_key, sizeof(pycom_config.sigfox_public_key));
}

bool config_set_sigfox_private_key (uint8_t *private_key) {
    memcpy(pycom_config.sigfox_private_key, private_key, sizeof(pycom_config.sigfox_private_key));
    return config_write();
}

void config_get_sigfox_private_key (uint8_t *private_key) {
    memcpy(private_key, pycom_config.sigfox_private_key, sizeof(pycom_config.sigfox_private_key));
}

bool config_set_wifi_on_boot (uint8_t wifi_on_boot) {
    if (pycom_config.wifi_on_boot != wifi_on_boot) {
        pycom_config.wifi_on_boot = wifi_on_boot;
        return config_write();
    }
    return true;
}

bool config_get_wifi_on_boot (void) {
    return pycom_config.wifi_on_boot;
}

static bool config_write (void) {
    // erase the block first
    if (ESP_OK == spi_flash_erase_sector(CONFIG_DATA_FLASH_BLOCK)) {
        // then write it
        return (spi_flash_write(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config, sizeof(pycom_config)) == ESP_OK);
    }
    return false;
}
