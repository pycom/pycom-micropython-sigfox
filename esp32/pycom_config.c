/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "ff.h"
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

bool config_set_wifi_smart_config (uint8_t smartConfig, bool update_flash)
{
    if (pycom_config_block.wifi_config.wifi_smart_config != smartConfig) {
        pycom_config_block.wifi_config.wifi_smart_config = smartConfig;
        if (update_flash) {
            return config_write();
        }
        else
        {
            return true;
        }
    }
    return true;
}

bool config_get_wifi_smart_config(void)
{
    return pycom_config_block.wifi_config.wifi_smart_config;
}

bool config_set_wifi_mode(uint8_t wifi_mode, bool update_flash)
{
    if (pycom_config_block.wifi_config.wifi_mode != wifi_mode) {
        pycom_config_block.wifi_config.wifi_mode = wifi_mode;
        return config_write();
    }
    if (update_flash) {
        return config_write();
    }
    else
    {
        return true;
    }
}

uint8_t config_get_wifi_mode(void)
{
    return pycom_config_block.wifi_config.wifi_mode;
}

bool config_set_wifi_auth(uint8_t wifi_auth, bool update_flash)
{
    if (pycom_config_block.wifi_config.wifi_auth != wifi_auth) {
        pycom_config_block.wifi_config.wifi_auth = wifi_auth;
        if (update_flash) {
            return config_write();
        }
        else
        {
            return true;
        }
    }
    return true;
}

uint8_t config_get_wifi_auth(void)
{
    return pycom_config_block.wifi_config.wifi_auth;
}

bool config_get_wifi_antenna(void)
{
    return (bool)pycom_config_block.wifi_config.wifi_antenna;
}

bool config_set_wifi_ap_ssid(const uint8_t *wifi_ssid)
{
    if(wifi_ssid != NULL)
    {
        memcpy(pycom_config_block.wifi_ap_config.wifi_ssid, wifi_ssid, sizeof(pycom_config_block.wifi_ap_config.wifi_ssid));
    }
    else
    {
        memset(pycom_config_block.wifi_ap_config.wifi_ssid, (uint8_t)0xFF, sizeof(pycom_config_block.wifi_ap_config.wifi_ssid));
    }

    return config_write();
}

bool config_get_wifi_ap_ssid(uint8_t *wifi_ssid)
{
    memcpy( wifi_ssid, pycom_config_block.wifi_ap_config.wifi_ssid, sizeof(pycom_config_block.wifi_ap_config.wifi_ssid));
    if (wifi_ssid[0]==0xff) {
        return false;
    }
    return true;
}

bool config_set_wifi_ap_pwd(const uint8_t *wifi_pwd)
{
    if(wifi_pwd != NULL)
    {
        memcpy(pycom_config_block.wifi_ap_config.wifi_pwd, wifi_pwd, sizeof(pycom_config_block.wifi_ap_config.wifi_pwd));
    }
    else
    {
        memset(pycom_config_block.wifi_ap_config.wifi_pwd, (uint8_t)0xFF, sizeof(pycom_config_block.wifi_ap_config.wifi_pwd));
    }

    return config_write();
}

bool config_get_wifi_ap_pwd(uint8_t *wifi_pwd)
{
    memcpy( wifi_pwd, pycom_config_block.wifi_ap_config.wifi_pwd, sizeof(pycom_config_block.wifi_ap_config.wifi_pwd));
    if (wifi_pwd[0]==0xff) {
        return false;
    }
    return true;
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

bool config_set_lte_modem_enable_on_boot (bool lte_modem_en_on_boot) {
    if (pycom_config_block.lte_config.lte_modem_en_on_boot != (uint8_t)lte_modem_en_on_boot) {
        pycom_config_block.lte_config.lte_modem_en_on_boot = (uint8_t)lte_modem_en_on_boot;
        return config_write();
    }
    return true;
}

bool config_get_lte_modem_enable_on_boot (void) {
    return (bool)pycom_config_block.lte_config.lte_modem_en_on_boot;
}

#if (VARIANT == PYBYTES)
bool config_set_pybytes_force_update (uint8_t force_update) {
    if (pycom_config_block.pybytes_config.force_update != force_update) {
        pycom_config_block.pybytes_config.force_update = force_update;
        return config_write();
    }
    return true;
}

bool config_get_pybytes_force_update (void) {
    if (pycom_config_block.pybytes_config.force_update == 0xFF) {
        pycom_config_block.pybytes_config.force_update = 0x00;
    }
    return pycom_config_block.pybytes_config.force_update;
}
#endif
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

bool config_set_sta_wifi_ssid (const uint8_t *wifi_ssid, bool update_flash) {
    if (wifi_ssid != NULL) {
        memcpy(pycom_config_block.wifi_sta_config.wifi_ssid, wifi_ssid, sizeof(pycom_config_block.wifi_sta_config.wifi_ssid));
    }
    else
    {
        memset(pycom_config_block.wifi_sta_config.wifi_ssid, (uint8_t)0xFF, sizeof(pycom_config_block.wifi_sta_config.wifi_ssid));
    }
    if (update_flash) {
        return config_write();
    }
    else
    {
        return true;
    }
}

bool config_get_wifi_sta_ssid (uint8_t *wifi_ssid) {
    memcpy( wifi_ssid, pycom_config_block.wifi_sta_config.wifi_ssid, sizeof(pycom_config_block.wifi_sta_config.wifi_ssid));
    if (wifi_ssid[0]==0xff) {
        return false;
    }
    return true;
}

bool config_set_wifi_sta_pwd (const uint8_t *wifi_pwd, bool update_flash) {
    if (wifi_pwd != NULL) {
        memcpy(pycom_config_block.wifi_sta_config.wifi_pwd, wifi_pwd, sizeof(pycom_config_block.wifi_sta_config.wifi_pwd));
    }
    else
    {
        memset(pycom_config_block.wifi_sta_config.wifi_pwd, (uint8_t)0xFF, sizeof(pycom_config_block.wifi_sta_config.wifi_pwd));
    }
    if (update_flash) {
        return config_write();
    }
    else
    {
        return true;
    }
}

bool config_get_wifi_sta_pwd (uint8_t *wifi_pwd) {
    memcpy( wifi_pwd, pycom_config_block.wifi_sta_config.wifi_pwd, sizeof(pycom_config_block.wifi_sta_config.wifi_pwd));
    if (wifi_pwd[0]==0xff) {
        return false;
    }
    return true;
}
#if (VARIANT == PYBYTES)
void config_get_pybytes_userId (uint8_t *pybytes_userId) {
    memcpy( pybytes_userId, pycom_config_block.pybytes_config.userId, sizeof(pycom_config_block.pybytes_config.userId));
    if (pybytes_userId[0]==0xff) {
        pybytes_userId[0]=0x0;
    }
}

void config_get_pybytes_mqttServiceAddress (uint8_t *pybytes_mqttServiceAddress) {
    memcpy( pybytes_mqttServiceAddress, pycom_config_block.pybytes_config.mqttServiceAddress, sizeof(pycom_config_block.pybytes_config.mqttServiceAddress));
    if (pybytes_mqttServiceAddress[0]==0xff) {
        pybytes_mqttServiceAddress[0]=0x0;
    }
}

void config_get_pybytes_device_token (uint8_t *pybytes_device_token) {
    memcpy( pybytes_device_token, pycom_config_block.pybytes_config.device_token, sizeof(pycom_config_block.pybytes_config.device_token));
    if (pybytes_device_token[0]==0xff) {
        pybytes_device_token[0]=0x0;
    }
}

void config_get_pybytes_network_preferences (uint8_t *pybytes_network_preferences) {
    memcpy( pybytes_network_preferences, pycom_config_block.pybytes_config.network_preferences, sizeof(pycom_config_block.pybytes_config.network_preferences));
    if (pybytes_network_preferences[0]==0xff) {
        pybytes_network_preferences[0]=0x0;
    }
}

void config_get_pybytes_extra_preferences (uint8_t *pybytes_extra_preferences) {
    memcpy( pybytes_extra_preferences, pycom_config_block.pybytes_config.extra_preferences, sizeof(pycom_config_block.pybytes_config.extra_preferences));
    if (pybytes_extra_preferences[0]==0xff) {
        pybytes_extra_preferences[0]=0x0;
    }
}
#endif

uint8_t config_get_boot_partition (void) {
    if (pycom_config_block.pycom_config.boot_partition==0xff) {
        return 0x00;
    }
    return pycom_config_block.pycom_config.boot_partition;
}

bool config_set_boot_partition (const uint8_t boot_partition) {
    pycom_config_block.pycom_config.boot_partition=boot_partition;
    return config_write();
}

uint8_t config_get_boot_fs_type (void) {
#ifdef FS_USE_LITTLEFS
    return 0x01;
#endif
    if (pycom_config_block.pycom_config.boot_fs_type==0xff) {
        return 0x01;
    }
    return pycom_config_block.pycom_config.boot_fs_type;
}

pycom_pybytes_lte_config_t config_get_pybytes_lte_config (void) {
	return pycom_config_block.pycom_pybytes_lte_config;
}

bool config_set_pybytes_lte_config(const pycom_pybytes_lte_config_t pycom_pybytes_lte_config) {
	pycom_config_block.pycom_pybytes_lte_config=pycom_pybytes_lte_config;
	return config_write();
}

bool config_set_boot_fs_type (const uint8_t boot_fs_type) {
    pycom_config_block.pycom_config.boot_fs_type=boot_fs_type;
    return config_write();
}

bool config_set_pybytes_autostart (bool pybytes_autostart) {
    if (pycom_config_block.pybytes_config.auto_start != (uint8_t)pybytes_autostart) {
        pycom_config_block.pybytes_config.auto_start = (uint8_t)pybytes_autostart;
        return config_write();
    }
    return true;
}

bool config_get_pybytes_autostart (void) {
    return (bool)pycom_config_block.pybytes_config.auto_start;
}

static bool config_write (void) {
    // erase the block first
    if (ESP_OK == spi_flash_erase_sector(CONFIG_DATA_FLASH_BLOCK)) {
        // then write it
        return (spi_flash_write(CONFIG_DATA_FLASH_ADDR, (void *)&pycom_config_block, sizeof(pycom_config_block)) == ESP_OK);
    }
    return false;
}
