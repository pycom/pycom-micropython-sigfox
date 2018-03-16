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

#define CONFIG_DATA_FLASH_BLOCK         (SFLASH_START_BLOCK_4MB + SFLASH_BLOCK_COUNT_4MB)
#define CONFIG_DATA_FLASH_ADDR          (SFLASH_START_ADDR_4MB + (SFLASH_BLOCK_COUNT_4MB * SFLASH_BLOCK_SIZE))

static bool config_write (void);

static pycom_config_block_t pycom_config_block;

void config_init0 (void) {
    // read the config struct from flash
    spi_flash_read(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config_block, sizeof(pycom_config_block));
    // printf("Config block has size: %d\n", sizeof(pycom_config_block));
}

bool config_set_lpwan_mac (const uint8_t *mac) {
    memcpy(pycom_config_block.lpwan_config.lpwan_mac, mac, sizeof(pycom_config_block.lpwan_config.lpwan_mac));
    return config_write();
}

void config_get_lpwan_mac (uint8_t *mac) {
    memcpy(mac, pycom_config_block.lpwan_config.lpwan_mac, sizeof(pycom_config_block.lpwan_config.lpwan_mac));
}

bool config_set_sigfox_id (uint8_t *id) {
    memcpy(pycom_config_block.lpwan_config.sigfox_id, id, sizeof(pycom_config_block.lpwan_config.sigfox_id));
    return config_write();
}

void config_get_sigfox_id (uint8_t *id) {
    memcpy(id, pycom_config_block.lpwan_config.sigfox_id, sizeof(pycom_config_block.lpwan_config.sigfox_id));
}

bool config_set_sigfox_pac (uint8_t *pac) {
    memcpy(pycom_config_block.lpwan_config.sigfox_pac, pac, sizeof(pycom_config_block.lpwan_config.sigfox_pac));
    return config_write();
}

void config_get_sigfox_pac (uint8_t *pac) {
    memcpy(pac, pycom_config_block.lpwan_config.sigfox_pac, sizeof(pycom_config_block.lpwan_config.sigfox_pac));
}

bool config_set_sigfox_public_key (uint8_t *public_key) {
    memcpy(pycom_config_block.lpwan_config.sigfox_public_key, public_key, sizeof(pycom_config_block.lpwan_config.sigfox_public_key));
    return config_write();
}

void config_get_sigfox_public_key (uint8_t *public_key) {
    memcpy(public_key, pycom_config_block.lpwan_config.sigfox_public_key, sizeof(pycom_config_block.lpwan_config.sigfox_public_key));
}

bool config_set_sigfox_private_key (uint8_t *private_key) {
    memcpy(pycom_config_block.lpwan_config.sigfox_private_key, private_key, sizeof(pycom_config_block.lpwan_config.sigfox_private_key));
    return config_write();
}

void config_get_sigfox_private_key (uint8_t *private_key) {
    memcpy(private_key, pycom_config_block.lpwan_config.sigfox_private_key, sizeof(pycom_config_block.lpwan_config.sigfox_private_key));
}

bool config_set_lora_region (uint8_t lora_region) {
    if (pycom_config_block.lpwan_config.lora_region != lora_region) {
        pycom_config_block.lpwan_config.lora_region = lora_region;
    }
    return config_write();
}

uint8_t config_get_lora_region (void) {
    return pycom_config_block.lpwan_config.lora_region;
}

bool config_set_wifi_on_boot (uint8_t wifi_on_boot) {
    if (pycom_config_block.wifi_config.wifi_on_boot != wifi_on_boot) {
        pycom_config_block.wifi_config.wifi_on_boot = wifi_on_boot;
        return config_write();
    }
    return true;
}

bool config_get_wifi_on_boot (void) {
    return pycom_config_block.wifi_config.wifi_on_boot;
}

bool config_set_wdt_on_boot (uint8_t wdt_on_boot) {
    if (pycom_config_block.wdt_config.wdt_on_boot != wdt_on_boot) {
        pycom_config_block.wdt_config.wdt_on_boot = wdt_on_boot;
        return config_write();
    }
    return true;
}

bool config_get_wdt_on_boot (void) {
    return pycom_config_block.wdt_config.wdt_on_boot;
}

bool config_set_wdt_on_boot_timeout (uint32_t wdt_on_boot_timeout) {
    if (pycom_config_block.wdt_config.wdt_on_boot_timeout != wdt_on_boot_timeout) {
        pycom_config_block.wdt_config.wdt_on_boot_timeout = wdt_on_boot_timeout;
        return config_write();
    }
    return true;
}

uint32_t config_get_wdt_on_boot_timeout (void) {
    return pycom_config_block.wdt_config.wdt_on_boot_timeout;
}

bool config_set_heartbeat_on_boot (uint8_t hb_on_boot) {
    if (pycom_config_block.rgbled_config.heartbeat_on_boot != hb_on_boot) {
        pycom_config_block.rgbled_config.heartbeat_on_boot = hb_on_boot;
        return config_write();
    }
    return true;
}

bool config_get_heartbeat_on_boot (void) {
    return pycom_config_block.rgbled_config.heartbeat_on_boot;
}

bool config_set_wifi_ssid (const uint8_t *wifi_ssid) {
    memcpy(pycom_config_block.wifi_config.wifi_ssid, wifi_ssid, sizeof(pycom_config_block.wifi_config.wifi_ssid));
    return config_write();
}

void config_get_wifi_ssid (uint8_t *wifi_ssid) {
    memcpy( wifi_ssid, pycom_config_block.wifi_config.wifi_ssid, sizeof(pycom_config_block.wifi_config.wifi_ssid));
    if (wifi_ssid[0]==0xff) {
        wifi_ssid[0]=0x0;
    }
}

bool config_set_wifi_pwd (const uint8_t *wifi_pwd) {
    memcpy(pycom_config_block.wifi_config.wifi_pwd, wifi_pwd, sizeof(pycom_config_block.wifi_config.wifi_pwd));
    return config_write();
}

void config_get_wifi_pwd (uint8_t *wifi_pwd) {
    memcpy( wifi_pwd, pycom_config_block.wifi_config.wifi_pwd, sizeof(pycom_config_block.wifi_config.wifi_pwd));
    if (wifi_pwd[0]==0xff) {
        wifi_pwd[0]=0x0;
    }
}

static bool config_write (void) {
// printf("Config block has size: %d\n", sizeof(pycom_config_block));
    // erase the block first
    if (ESP_OK == spi_flash_erase_sector(CONFIG_DATA_FLASH_BLOCK)) {
        // then write it
        return (spi_flash_write(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config_block, sizeof(pycom_config_block)) == ESP_OK);
    }
    return false;
}
