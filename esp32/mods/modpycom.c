/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "mperror.h"
#include "updater.h"
#include "modled.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "pycom_config.h"
#include "mpexception.h"
#include "machpin.h"
#include "driver/rmt.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "antenna.h"
#include "py/mphal.h"

#include "bootmgr.h"
#include "updater.h"

#include "mptask.h"

#include "modmachine.h"
#include "esp32chipinfo.h"
#include "modwlan.h"


#include <string.h>

extern led_info_t led_info;


#define NVS_NAMESPACE                           "PY_NVM"

#define WDT_ON_BOOT_MIN_TIMEOUT_MS              (5000)

static nvs_handle pycom_nvs_handle;
boot_info_t boot_info;
uint32_t boot_info_offset;

static void modpycom_bootmgr(uint8_t boot_partition, uint8_t fs_type, uint8_t safeboot, bool reset);

void modpycom_init0(void) {
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &pycom_nvs_handle) != ESP_OK) {
        mp_printf(&mp_plat_print, "Error while opening Pycom NVS name space\n");
    }
    rmt_driver_install(RMT_CHANNEL_0, 1000, 0);
    if (updater_read_boot_info (&boot_info, &boot_info_offset) == false) {
        mp_printf(&mp_plat_print, "Error reading bootloader information!\n");
    }
}

static bool is_empty(uint8_t* value, uint8_t size) {
    bool ret_val = true;
    for (int i=0; i < size; i++) {
        if (value[i] != 0xFF) {
            ret_val = false;
        }
    }
    return ret_val;
}

static void modpycom_bootmgr(uint8_t boot_partition, uint8_t fs_type, uint8_t safeboot, bool reset) {
    bool update_part = false;
    bool update_fstype = false;
    bool update_safeboot = false;

    if (boot_partition < 255) {
        if ((boot_partition <= IMG_ACT_UPDATE1) && (boot_info.ActiveImg != boot_partition)) {
            boot_info.PrevImg = boot_info.ActiveImg;
            boot_info.ActiveImg = (uint32_t)boot_partition;
            boot_info.Status = IMG_STATUS_CHECK;
            update_part = true;
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error invalid boot partition! or partition already active!"));
        }
    }
    if (safeboot < 255) {
        if ((safeboot <= 1) && (safeboot != boot_info.safeboot)) {
            if(safeboot)
            {
                boot_info.safeboot = (uint32_t)SAFE_BOOT_SW;
            }
            else
            {
                boot_info.safeboot = (uint32_t)0x00;
            }
            update_safeboot = true;
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error safeboot must be True or False!"));
        }
    }
    if (fs_type < 255) {
        if (fs_type <= 1) {
            if(config_get_boot_fs_type() != fs_type)
            {
                config_set_boot_fs_type(fs_type);
                update_fstype = true;
            }
        }
        else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error invalid filesystem type!"));
        }
    }
    if (update_part || update_safeboot) {
        if (updater_write_boot_info (&boot_info, boot_info_offset) == false) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error writing bootloader information!"));
        }
        if(update_part)
        {
            machine_reset();
        }
    }
    if((update_fstype || update_safeboot) && reset)
    {
        machine_reset();
    }
}

/******************************************************************************/
// Micro Python bindings


STATIC mp_obj_t mod_pycom_heartbeat (mp_uint_t n_args, const mp_obj_t *args) {
#ifndef RGB_LED_DISABLE
    if (n_args) {
        mperror_enable_heartbeat (mp_obj_is_true(args[0]));
        if (!mp_obj_is_true(args[0])) {
            do {
                mp_hal_delay_ms(2);
            } while (!mperror_heartbeat_disable_done());
        }
    } else {
        return mp_obj_new_bool(mperror_is_heartbeat_enabled());
    }
#else
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "RGB Led Interface Disabled"));
#endif
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_heartbeat_obj, 0, 1, mod_pycom_heartbeat);

STATIC mp_obj_t mod_pycom_rgb_led (mp_uint_t n_args, const mp_obj_t *args) {
#ifndef RGB_LED_DISABLE
    if (mperror_is_heartbeat_enabled()) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if(n_args > 0){
        uint32_t color = mp_obj_get_int(args[0]);
        led_info.color.value = color;
        led_set_color(&led_info, true, false);
    } else {
        return mp_obj_new_int(led_info.color.value);
    }
#else
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "RGB Led Interface Disabled"));
#endif

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_rgb_led_obj, 0,1,mod_pycom_rgb_led);

STATIC mp_obj_t mod_pycom_ota_start (void) {
    if (!updater_start()) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_ota_start_obj, mod_pycom_ota_start);

STATIC mp_obj_t mod_pycom_ota_write (mp_obj_t data) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);

    if (!updater_write(bufinfo.buf, bufinfo.len)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_pycom_ota_write_obj, mod_pycom_ota_write);

STATIC mp_obj_t mod_pycom_ota_finish (void) {
    if (!updater_finish()) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_ota_finish_obj, mod_pycom_ota_finish);

STATIC mp_obj_t mod_pycom_ota_verify (void) {
    bool ret_val = updater_verify();
    return mp_obj_new_bool(ret_val);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_ota_verify_obj, mod_pycom_ota_verify);

STATIC mp_obj_t mod_pycom_ota_slot (void) {
    int ota_slot = updater_ota_next_slot_address();
    return mp_obj_new_int(ota_slot);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_ota_slot_obj, mod_pycom_ota_slot);

STATIC mp_obj_t mod_pycom_diff_update_enabled (void) {
#ifdef DIFF_UPDATE_ENABLED
    return mp_obj_new_bool(true);
#else
    return mp_obj_new_bool(false);
#endif
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_diff_update_enabled_obj, mod_pycom_diff_update_enabled);

STATIC mp_obj_t mod_pycom_pulses_get (mp_obj_t gpio, mp_obj_t timeout) {
    rmt_config_t rmt_rx;
    rmt_rx.channel = RMT_CHANNEL_0;
    rmt_rx.gpio_num = pin_find(gpio)->pin_number;
    rmt_rx.clk_div = 80;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = 0xFFFF;
    rmt_config(&rmt_rx);

    RingbufHandle_t rb = NULL;
    mp_obj_t pulses_l = mp_obj_new_list(0, NULL);

    // get the RMT RX ringbuffer
    rmt_get_ringbuf_handle(RMT_CHANNEL_0, &rb);
    rmt_rx_start(RMT_CHANNEL_0, 1);

    size_t rx_size = 0;
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, mp_obj_get_int(timeout));
    if (item) {
        for (int i = 0; i < rx_size / 4; i++) {
            mp_obj_t tuple[2];
            tuple[0] = mp_obj_new_int(item[i].level0);
            tuple[1] = mp_obj_new_int(item[i].duration0);
            mp_obj_list_append(pulses_l, mp_obj_new_tuple(2, tuple));

            tuple[0] = mp_obj_new_int(item[i].level1);
            tuple[1] = mp_obj_new_int(item[i].duration1);
            mp_obj_list_append(pulses_l, mp_obj_new_tuple(2, tuple));
        }

        // after parsing the data, return spaces to ringbuffer.
        vRingbufferReturnItem(rb, (void*) item);
    }

    rmt_rx_stop(RMT_CHANNEL_0);

    return pulses_l;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_pulses_get_obj, mod_pycom_pulses_get);


STATIC mp_obj_t mod_pycom_nvs_set (mp_obj_t _key, mp_obj_t _value) {

    const char *key = mp_obj_str_get_str(_key);
    esp_err_t esp_err = ESP_OK;

    if (MP_OBJ_IS_STR_OR_BYTES(_value)) {
        const char *value = mp_obj_str_get_str(_value);
        if (strlen(value) >= 1984) {
            // Maximum length (including null character) can be 1984 bytes
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "value too long (max: 1984)"));
        }
        esp_err = nvs_set_str(pycom_nvs_handle, key, value);
    } else if(MP_OBJ_IS_INT(_value)) {
        uint32_t value = mp_obj_get_int_truncated(_value);
        esp_err = nvs_set_u32(pycom_nvs_handle, key, value);
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Value must be string, bytes or integer"));
    }

    if (ESP_OK == esp_err) {
        nvs_commit(pycom_nvs_handle);
    } else if (ESP_ERR_NVS_NOT_ENOUGH_SPACE == esp_err || ESP_ERR_NVS_PAGE_FULL == esp_err || ESP_ERR_NVS_NO_FREE_PAGES == esp_err) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "No free space available"));
    } else if (ESP_ERR_NVS_INVALID_NAME == esp_err) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Key is invalid"));
    } else if (ESP_ERR_NVS_KEY_TOO_LONG == esp_err) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Key is too long"));
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_Exception, "Error occurred while storing value, code: %d", esp_err));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_nvs_set_obj, mod_pycom_nvs_set);

STATIC mp_obj_t mod_pycom_nvs_get (mp_uint_t n_args, const mp_obj_t *args) {

    const char *key = mp_obj_str_get_str(args[0]);
    esp_err_t esp_err = ESP_OK;
    mp_obj_t ret = mp_const_none;
    uint32_t value;

    esp_err = nvs_get_u32(pycom_nvs_handle, key, &value);
    if (esp_err == ESP_OK) {
        ret = mp_obj_new_int(value);
    }
    else {
        esp_err = nvs_get_str(pycom_nvs_handle, key, NULL, &value);
        if(esp_err == ESP_OK) {
            char* value_string = (char*)malloc(value);

            esp_err = nvs_get_str(pycom_nvs_handle, key, value_string, &value);

            if(esp_err == ESP_OK) {
                //do not count the terminating \0
                ret = mp_obj_new_str(value_string, value-1);
            }
            free(value_string);
        }
    }

    if(esp_err == ESP_ERR_NVS_NOT_FOUND) {
        if (n_args > 1) {
            // return user defined NoExistValue
            return args[1];
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "No matching object for the provided key"));
        }
    } else if(esp_err != ESP_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_Exception, "Error occurred while fetching value, code: %d", esp_err));
    }

    return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_nvs_get_obj, 1, 2, mod_pycom_nvs_get);

STATIC mp_obj_t mod_pycom_nvs_erase (mp_obj_t _key) {
    const char *key = mp_obj_str_get_str(_key);

    if (ESP_ERR_NVS_NOT_FOUND == nvs_erase_key(pycom_nvs_handle, key)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_KeyError, "key not found"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_pycom_nvs_erase_obj, mod_pycom_nvs_erase);

STATIC mp_obj_t mod_pycom_nvs_erase_all (void) {
    if (ESP_OK != nvs_erase_all(pycom_nvs_handle)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    nvs_commit(pycom_nvs_handle);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_nvs_erase_all_obj, mod_pycom_nvs_erase_all);

STATIC mp_obj_t mod_pycom_wifi_on_boot (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return mp_obj_new_bool(config_get_wifi_on_boot());
    }
    if (config_get_wifi_on_boot() != mp_obj_is_true(args[0])) {
        config_set_wifi_on_boot (mp_obj_is_true(args[0]));
        if (n_args > 1 && mp_obj_is_true(args[1])) {
            if (mp_obj_is_true(args[0])) {
                mptask_config_wifi(true);
            } else {
                wlan_deinit(NULL);
            }
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_on_boot_obj, 0, 2, mod_pycom_wifi_on_boot);

STATIC mp_obj_t mod_pycom_wifi_mode (mp_uint_t n_args, const mp_obj_t *args) {
    uint8_t mode;
    if (n_args) {
        mode = mp_obj_get_int(args[0]);
        switch(mode)
        {
            case 0:
                config_set_wifi_mode ((uint8_t)PYCOM_WIFI_CONF_MODE_NONE, true);
                break;
            case 1:
                config_set_wifi_mode ((uint8_t)PYCOM_WIFI_CONF_MODE_STA, true);
                break;
            case 2:
                config_set_wifi_mode ((uint8_t)PYCOM_WIFI_CONF_MODE_AP, true);
                break;
            case 3:
                config_set_wifi_mode ((uint8_t)PYCOM_WIFI_CONF_MODE_APSTA, true);
                break;
            default:
                break;
        }
    } else {
        mode = config_get_wifi_mode();
        switch(mode)
        {
            case PYCOM_WIFI_CONF_MODE_STA:
                return MP_OBJ_NEW_SMALL_INT(1);
            case PYCOM_WIFI_CONF_MODE_AP:
                return MP_OBJ_NEW_SMALL_INT(2);
            case PYCOM_WIFI_CONF_MODE_APSTA:
                return MP_OBJ_NEW_SMALL_INT(3);
            case PYCOM_WIFI_CONF_MODE_NONE:
            default:
                return MP_OBJ_NEW_SMALL_INT(0);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_mode_obj, 0, 1, mod_pycom_wifi_mode);

STATIC mp_obj_t mod_pycom_wifi_ssid_sta (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        if(args[0] == mp_const_none)
        {
            config_set_sta_wifi_ssid (NULL, true);
        }
        else if (MP_OBJ_IS_STR(args[0]))
        {
            config_set_sta_wifi_ssid ((uint8_t *)(mp_obj_str_get_str(args[0])), true);
        }
        else{/*Nothing*/}

    } else {
        uint8_t * ssid = (uint8_t *)malloc(33);
        mp_obj_t ssid_obj;
        if(config_get_wifi_sta_ssid(ssid))
        {
            ssid_obj = mp_obj_new_str((const char *)ssid, strlen((const char *)ssid));
        }
        else
        {
            ssid_obj = mp_const_none;
        }
        free(ssid);
        return ssid_obj;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_ssid_sta_obj, 0, 1, mod_pycom_wifi_ssid_sta);

STATIC mp_obj_t mod_pycom_wifi_pwd_sta (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        if(args[0] == mp_const_none)
        {
            config_set_wifi_sta_pwd (NULL, true);
        }
        else if (MP_OBJ_IS_STR(args[0]))
        {
            config_set_wifi_sta_pwd ((uint8_t *)(mp_obj_str_get_str(args[0])), true);
        }
        else{/*Nothing*/}
    } else {
        uint8_t * pwd = (uint8_t *)malloc(65);
        mp_obj_t pwd_obj;
        if(config_get_wifi_sta_pwd(pwd))
        {
            pwd_obj = mp_obj_new_str((const char *)pwd, strlen((const char *)pwd));
        }
        else
        {
            pwd_obj = mp_const_none;
        }
        free(pwd);
        return pwd_obj;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_pwd_sta_obj, 0, 1, mod_pycom_wifi_pwd_sta);

STATIC mp_obj_t mod_pycom_wifi_ssid_ap (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        if(args[0] == mp_const_none)
        {
            config_set_wifi_ap_ssid (NULL);
        }
        else if (MP_OBJ_IS_STR(args[0]))
        {
            config_set_wifi_ap_ssid ((uint8_t *)(mp_obj_str_get_str(args[0])));
        }
        else{/*Nothing*/}
    } else {
        uint8_t * ssid = (uint8_t *)malloc(33);
        mp_obj_t ssid_obj;
        if(config_get_wifi_ap_ssid(ssid))
        {
            ssid_obj = mp_obj_new_str((const char *)ssid, strlen((const char *)ssid));
        }
        else
        {
            ssid_obj = mp_const_none;
        }
        free(ssid);
        return ssid_obj;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_ssid_ap_obj, 0, 1, mod_pycom_wifi_ssid_ap);

STATIC mp_obj_t mod_pycom_wifi_pwd_ap (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        if(args[0] == mp_const_none)
        {
            config_set_wifi_ap_pwd (NULL);
        }
        else if (MP_OBJ_IS_STR(args[0]))
        {
            config_set_wifi_ap_pwd ((uint8_t *)(mp_obj_str_get_str(args[0])));
        }
        else{/*Nothing*/}
    } else {
        uint8_t * pwd = (uint8_t *)malloc(65);
        mp_obj_t pwd_obj;
        if(config_get_wifi_ap_pwd(pwd))
        {
            pwd_obj = mp_obj_new_str((const char *)pwd, strlen((const char *)pwd));
        }
        else
        {
            pwd_obj = mp_const_none;
        }
        free(pwd);
        return pwd_obj;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_pwd_ap_obj, 0, 1, mod_pycom_wifi_pwd_ap);


STATIC mp_obj_t mod_pycom_wdt_on_boot (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_wdt_on_boot (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_wdt_on_boot());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wdt_on_boot_obj, 0, 1, mod_pycom_wdt_on_boot);

STATIC mp_obj_t mod_pycom_wdt_on_boot_timeout (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        uint32_t timeout_ms = mp_obj_get_int(args[0]);
        if (timeout_ms < WDT_ON_BOOT_MIN_TIMEOUT_MS) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "WDT on boot timeout must be >= 5000 ms"));
        }
        config_set_wdt_on_boot_timeout (timeout_ms);
    } else {
        return mp_obj_new_int_from_uint(config_get_wdt_on_boot_timeout());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wdt_on_boot_timeout_obj, 0, 1, mod_pycom_wdt_on_boot_timeout);

STATIC mp_obj_t mod_pycom_heartbeat_on_boot (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_heartbeat_on_boot (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_heartbeat_on_boot());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_heartbeat_on_boot_obj, 0, 1, mod_pycom_heartbeat_on_boot);

STATIC mp_obj_t mod_pycom_lte_modem_on_boot (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_lte_modem_enable_on_boot (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_lte_modem_enable_on_boot());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_lte_modem_on_boot_obj, 0, 1, mod_pycom_lte_modem_on_boot);

STATIC mp_obj_t mod_pycom_pybytes_on_boot (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_pybytes_autostart (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_pybytes_autostart());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_pybytes_on_boot_obj, 0, 1, mod_pycom_pybytes_on_boot);


STATIC mp_obj_t mod_pycom_pybytes_lte_config (size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_carrier, ARG_apn, ARG_cid, ARG_band, ARG_type, ARG_reset };
    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_carrier,          MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
            { MP_QSTR_apn,              MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
            { MP_QSTR_cid,              MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
            { MP_QSTR_band,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
            { MP_QSTR_type,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
            { MP_QSTR_reset,            MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },

    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    pycom_pybytes_lte_config_t pycom_pybytes_lte_config = config_get_pybytes_lte_config();

    if (n_args == 0) {
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(6, NULL));

        if(pycom_pybytes_lte_config.carrier[0] == 0xFF && pycom_pybytes_lte_config.carrier[1] == 0xFF && pycom_pybytes_lte_config.carrier[2] == 0xFF)
        {
            t->items[ARG_carrier] = mp_const_none;
        }
        else
        {
            t->items[ARG_carrier] = mp_obj_new_str((const char *)pycom_pybytes_lte_config.carrier, strlen((const char *)pycom_pybytes_lte_config.carrier));
        }

        if(pycom_pybytes_lte_config.apn[0] == 0xFF && pycom_pybytes_lte_config.apn[1] == 0xFF && pycom_pybytes_lte_config.apn[2] == 0xFF)
        {
            t->items[ARG_apn] = mp_const_none;
        }
        else
        {
            t->items[ARG_apn] = mp_obj_new_str((const char *)pycom_pybytes_lte_config.apn, strlen((const char *)pycom_pybytes_lte_config.apn));
        }
        if(pycom_pybytes_lte_config.cid == 0xFF)
        {
            t->items[ARG_cid] = mp_obj_new_int(1);
        }
        else
        {
            t->items[ARG_cid] = mp_obj_new_int(pycom_pybytes_lte_config.cid);
        }
        if(pycom_pybytes_lte_config.band == 0xFF)
        {
            t->items[ARG_band] = mp_const_none;
        }
        else
        {
            t->items[ARG_band] = mp_obj_new_int(pycom_pybytes_lte_config.band);
        }
        if(pycom_pybytes_lte_config.type[0] == 0xFF)
        {
            t->items[ARG_type] = mp_const_none;
        }
        else
        {
            t->items[ARG_type] = mp_obj_new_str((const char *)pycom_pybytes_lte_config.type, strlen((const char *)pycom_pybytes_lte_config.type));
        }
        if(pycom_pybytes_lte_config.reset == 0xff)
        {
            t->items[ARG_reset] = mp_const_false;
        }
        else
        {
            t->items[ARG_reset] = mp_obj_new_bool(pycom_pybytes_lte_config.reset);
        }
        return MP_OBJ_FROM_PTR(t);

    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error this functionality is not yet supported!"));
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_pycom_pybytes_lte_config_obj, 0, mod_pycom_pybytes_lte_config);

STATIC mp_obj_t mod_pycom_bootmgr (size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_boot_partition, ARG_fs_type, ARG_safeboot, ARG_status };
    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_boot_partition,   MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 255} },
            { MP_QSTR_fs_type,          MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 255} },
            { MP_QSTR_safeboot,         MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 255} },
            { MP_QSTR_reset,            MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_boot_partition].u_int == 255 && args[ARG_fs_type].u_int == 255 && args[ARG_safeboot].u_int == 255) {
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));

        if(boot_info.ActiveImg == 0x00)
        {
            t->items[ARG_boot_partition] = mp_obj_new_str("Factory", strlen("Factory"));
        }
        else
        {
            t->items[ARG_boot_partition] = mp_obj_new_str("ota_0", strlen("ota_0"));
        }

        if(config_get_boot_fs_type() == 0x00)
        {
            t->items[ARG_fs_type] = mp_obj_new_str("FAT", strlen("FAT"));
        }
        else
        {
            t->items[ARG_fs_type] = mp_obj_new_str("LittleFS", strlen("LittleFS"));
        }

        if(boot_info.safeboot == 0x00)
        {
            t->items[ARG_safeboot] = mp_obj_new_str("SafeBoot: False", strlen("SafeBoot: False"));
        }
        else
        {
            t->items[ARG_safeboot] = mp_obj_new_str("SafeBoot: True", strlen("SafeBoot: True"));
        }
        if(boot_info.Status == IMG_STATUS_CHECK)
        {
            t->items[ARG_status] = mp_obj_new_str("Status: Check", strlen("Status: Check"));
        }
        else if(boot_info.Status == IMG_STATUS_READY)
        {
            t->items[ARG_status] = mp_obj_new_str("Status: Ready", strlen("Status: Ready"));
        }
        else if(boot_info.Status == IMG_STATUS_PATCH)
        {
            t->items[ARG_status] = mp_obj_new_str("Status: Patch", strlen("Status: Patch"));
        }

        return MP_OBJ_FROM_PTR(t);

    } else {
        modpycom_bootmgr(args[ARG_boot_partition].u_int, args[ARG_fs_type].u_int, args[ARG_safeboot].u_int, args[3].u_bool);
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_pycom_bootmgr_obj, 0, mod_pycom_bootmgr);

STATIC mp_obj_t mod_pycom_get_free_heap (void) {

    size_t heap_psram_free = 0;
    mp_obj_t items[2];

    if (esp32_get_chip_rev() > 0) {
        heap_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    items[0] = mp_obj_new_int(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if(heap_psram_free)
    {
        items[1] = mp_obj_new_int(heap_psram_free);
    }
    else
    {
        items[1] = mp_obj_new_str("NO_EXT_RAM", strlen("NO_EXT_RAM"));
    }

    return mp_obj_new_tuple(2, items);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_get_free_heap_obj, mod_pycom_get_free_heap);

#if (VARIANT == PYBYTES)

STATIC mp_obj_t mod_pycom_pybytes_device_token (void) {
    uint8_t pybytes_device_token[39];
    config_get_pybytes_device_token(pybytes_device_token);
    return mp_obj_new_str((const char*)pybytes_device_token,strlen((const char*)pybytes_device_token));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_pybytes_device_token_obj, mod_pycom_pybytes_device_token);


STATIC mp_obj_t mod_pycom_pybytes_mqttServiceAddress (void) {
    uint8_t pybytes_mqttServiceAddress[39];
    config_get_pybytes_mqttServiceAddress(pybytes_mqttServiceAddress);
    return mp_obj_new_str((const char*)pybytes_mqttServiceAddress,strlen((const char*)pybytes_mqttServiceAddress));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_pybytes_mqttServiceAddress_obj, mod_pycom_pybytes_mqttServiceAddress);

STATIC mp_obj_t mod_pycom_pybytes_userId (void) {
    uint8_t pybytes_userId[254];
    config_get_pybytes_userId(pybytes_userId);
    return mp_obj_new_str((const char*)pybytes_userId,strlen((const char*)pybytes_userId));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_pybytes_userId_obj, mod_pycom_pybytes_userId);

STATIC mp_obj_t mod_pycom_pybytes_network_preferences (void) {
    uint8_t pybytes_network_preferences[54];
    config_get_pybytes_network_preferences(pybytes_network_preferences);
    return mp_obj_new_str((const char*)pybytes_network_preferences,strlen((const char*)pybytes_network_preferences));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_pybytes_network_preferences_obj, mod_pycom_pybytes_network_preferences);

STATIC mp_obj_t mod_pycom_pybytes_extra_preferences (void) {
    uint8_t pybytes_extra_preferences[99];
    config_get_pybytes_extra_preferences(pybytes_extra_preferences);
    return mp_obj_new_str((const char*)pybytes_extra_preferences,strlen((const char*)pybytes_extra_preferences));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_pycom_pybytes_extra_preferences_obj, mod_pycom_pybytes_extra_preferences);

STATIC mp_obj_t mod_pycom_pybytes_force_update (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_pybytes_force_update (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_pybytes_force_update());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_pybytes_force_update_obj, 0, 1, mod_pycom_pybytes_force_update);

STATIC mp_obj_t mod_pycom_smartConfig (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        config_set_wifi_smart_config ((uint8_t)(mp_obj_is_true(args[0])), true);
    } else {
        return mp_obj_new_bool(config_get_wifi_smart_config());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_smartConfig_obj, 0, 1, mod_pycom_smartConfig);

#endif //(VARIANT == PYBYTES)


// Helper function to return decimal value of a hexadecimal character coded in ASCII
STATIC uint8_t hex_from_char(const char c) {

    if((uint8_t)c >= '0' && (uint8_t)c <= '9') {
        return c - '0';
    }
    else if((uint8_t)c >= 'A' && (uint8_t)c <= 'F') {
        return c - ('A' - 10);
    }
    else if((uint8_t)c >= 'a' && (uint8_t)c <= 'f') {
        return c - ('a' - 10);
    }
    else {
        // 16 is invalid, because in hexa allowed range is 0 - 15
        return 16;
    }

}


STATIC mp_obj_t mod_pycom_sigfox_info (size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_id, ARG_pac, ARG_public_key, ARG_private_key, ARG_force };
    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_id,           MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
            { MP_QSTR_pac,          MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
            { MP_QSTR_public_key,   MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
            { MP_QSTR_private_key,  MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
            { MP_QSTR_force,        MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} }
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint8_t id[4];
    uint8_t pac[8];
    uint8_t public_key[16];
    uint8_t private_key[16];

    config_get_sigfox_id(id);
    config_get_sigfox_pac(pac);
    config_get_sigfox_public_key(public_key);
    config_get_sigfox_private_key(private_key);


    if (args[ARG_id].u_obj == mp_const_none && args[ARG_pac].u_obj == mp_const_none && args[ARG_public_key].u_obj == mp_const_none && args[ARG_private_key].u_obj == mp_const_none) {
        // query sigfox info

        if ( !is_empty(id, sizeof(id)) && !is_empty(pac, sizeof(pac)) && !is_empty(public_key, sizeof(public_key)) && !is_empty(private_key, sizeof(private_key)) ){
            // all configs valid
            return mp_const_true;
        } else {
            return mp_const_false;
        }
    } else {
        // write sigfox info

        // temporary array to store even the longest value from id, pac, public_key and private_key
        uint8_t tmp_array[16];
        size_t length;
        bool ret_val = false;

        if (args[ARG_id].u_obj != mp_const_none) {

            if ( args[ARG_force].u_bool == true || is_empty(id, sizeof(id)) ) {

                const char* id = mp_obj_str_get_data(args[ARG_id].u_obj, &length);

                if(length != 8) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "ID must have length of 8!"));
                }

                // Put together every 2 characters of the string (which are digits from 0 - 9 and a/A - f/F) into 1 bytes because the available space is half of the required one
                for(int i = 0, j = 0; i < length; i = i+2) {
                    uint8_t lower_nibble = hex_from_char(id[i+1]);
                    uint8_t upper_nibble = hex_from_char(id[i]);

                    if(lower_nibble == 16 || upper_nibble == 16) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "ID must only contain hexadecimal digits!"));
                    }

                    tmp_array[j] = lower_nibble | (upper_nibble << 4);
                    j++;
                }

                ret_val = config_set_sigfox_id(tmp_array);
                if (ret_val == false) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to write id"));
                }
            } else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Use force option to overwrite existing id!"));
            }
        }
        if (args[ARG_pac].u_obj != mp_const_none) {

            if (args[ARG_force].u_bool == true || is_empty(pac, sizeof(pac))) {

                const char* pac = mp_obj_str_get_data(args[ARG_pac].u_obj, &length);

                if(length != 16) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "PAC must have length of 16!"));
                }

                // Put together every 2 characters of the string (which are digits from 0 - 9 and a/A - f/F) into 1 bytes because the available space is half of the required one
                for(int i = 0, j = 0; i < length; i = i+2) {
                    uint8_t lower_nibble = hex_from_char(pac[i+1]);
                    uint8_t upper_nibble = hex_from_char(pac[i]);

                    if(lower_nibble == 16 || upper_nibble == 16) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "PAC must only contain hexadecimal digits!"));
                    }

                    tmp_array[j] = lower_nibble | (upper_nibble << 4);
                    j++;
                }

                ret_val = config_set_sigfox_pac(tmp_array);
                if (ret_val == false) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to write pac"));
                }
            } else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Use force option to overwrite existing pac!"));
            }
        }
        if (args[ARG_public_key].u_obj != mp_const_none) {

            if (args[ARG_force].u_bool == true || is_empty(public_key, sizeof(public_key))) {

                const char* public_key = mp_obj_str_get_data(args[ARG_public_key].u_obj, &length);

                if(length != 32) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Public Key must have length of 32!"));
                }

                // Put together every 2 characters of the string (which are digits from 0 - 9 and a/A - f/F) into 1 bytes because the available space is half of the required one
                for(int i = 0, j = 0; i < length; i = i+2) {
                    uint8_t lower_nibble = hex_from_char(public_key[i+1]);
                    uint8_t upper_nibble = hex_from_char(public_key[i]);

                    if(lower_nibble == 16 || upper_nibble == 16) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Public Key must only contain hexadecimal digits!"));
                    }

                    tmp_array[j] = lower_nibble | (upper_nibble << 4);
                    j++;
                }

                ret_val = config_set_sigfox_public_key(tmp_array);
                if (ret_val == false) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to write public key"));
                }
            } else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Use force option to overwrite existing public key!"));
            }

        }
        if (args[ARG_private_key].u_obj != mp_const_none) {

            if (args[ARG_force].u_bool == true || is_empty(private_key, sizeof(private_key))) {

                const char* private_key = mp_obj_str_get_data(args[ARG_private_key].u_obj, &length);

                if(length != 32) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Private Key must have length of 32"));
                }

                // Put together every 2 characters of the string (which are digits from 0 - 9 and a/A - f/F) into 1 bytes because the available space is half of the required one
                for(int i = 0, j = 0; i < length; i = i+2) {
                    uint8_t lower_nibble = hex_from_char(private_key[i+1]);
                    uint8_t upper_nibble = hex_from_char(private_key[i]);

                    if(lower_nibble == 16 || upper_nibble == 16) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Private Key must only contain hexadecimal digits!"));
                    }

                    tmp_array[j] = lower_nibble | (upper_nibble << 4);
                    j++;
                }

                ret_val = config_set_sigfox_private_key(tmp_array);
                if (ret_val == false) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Failed to write private key"));
                }
            } else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Use force option to overwrite existing private key!"));
            }
        }
        return mp_const_true;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_pycom_sigfox_info_obj, 0, mod_pycom_sigfox_info);

// This function creates a 128 bit long UUID stored in a byte array in Little Endian order from an input String
STATIC mp_obj_t create_128bit_le_uuid_from_string(mp_obj_t uuid_in) {

    size_t length;
    uint8_t new_uuid[16];
    uint8_t i, j;

    const char* uuid_char_in = mp_obj_str_get_data(uuid_in, &length);
    // 1 character is stored on 1 byte because we received a String
    // For 128 bit UUID maximum 32 characters long String can be accepted
    if (length > 32) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Input string must not be longer than 32 characters!"));
    }

    // Pre-fill the whole array with 0 because the remaining/not given digits will be 0
    char uuid_char[32] = {0};
    memcpy(uuid_char, uuid_char_in, length);

    for(i = 0, j = 0; i < 32; i = i+2) {

        uint8_t lower_nibble = 0;
        uint8_t upper_nibble = 0;

        if(uuid_char[i] > 0) {
            upper_nibble = hex_from_char(uuid_char[i]);
        }

        if(uuid_char[i+1] > 0) {
            lower_nibble = hex_from_char(uuid_char[i+1]);
        }

        if(lower_nibble == 16 || upper_nibble == 16) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "UUID must only contain hexadecimal digits!"));
        }

        // Pack together the 4 bits digits into 1 byte
        // Convert to Little Endian order because we expect that the digits of the input String follows the Natural Byte (Big Endian) order
        new_uuid[15-j] = lower_nibble | (upper_nibble << 4);
        j++;
    }

    mp_obj_t new_uuid_mp = mp_obj_new_bytearray(16, new_uuid);
    return new_uuid_mp;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(create_128bit_le_uuid_from_string_obj, create_128bit_le_uuid_from_string);

STATIC const mp_map_elem_t pycom_module_globals_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_pycom) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat),                       (mp_obj_t)&mod_pycom_heartbeat_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_rgbled),                          (mp_obj_t)&mod_pycom_rgb_led_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ota_start),                       (mp_obj_t)&mod_pycom_ota_start_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ota_write),                       (mp_obj_t)&mod_pycom_ota_write_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ota_finish),                      (mp_obj_t)&mod_pycom_ota_finish_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ota_verify),                      (mp_obj_t)&mod_pycom_ota_verify_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ota_slot),                        (mp_obj_t)&mod_pycom_ota_slot_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_diff_update_enabled),             (mp_obj_t)&mod_pycom_diff_update_enabled_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pulses_get),                      (mp_obj_t)&mod_pycom_pulses_get_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_set),                         (mp_obj_t)&mod_pycom_nvs_set_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_get),                         (mp_obj_t)&mod_pycom_nvs_get_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_erase),                       (mp_obj_t)&mod_pycom_nvs_erase_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_erase_all),                   (mp_obj_t)&mod_pycom_nvs_erase_all_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_on_boot),                    (mp_obj_t)&mod_pycom_wifi_on_boot_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wdt_on_boot),                     (mp_obj_t)&mod_pycom_wdt_on_boot_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wdt_on_boot_timeout),             (mp_obj_t)&mod_pycom_wdt_on_boot_timeout_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat_on_boot),               (mp_obj_t)&mod_pycom_heartbeat_on_boot_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_lte_modem_en_on_boot),            (mp_obj_t)&mod_pycom_lte_modem_on_boot_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_get_free_heap),                   (mp_obj_t)&mod_pycom_get_free_heap_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_ssid_sta),                   (mp_obj_t)&mod_pycom_wifi_ssid_sta_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_ssid_ap),                    (mp_obj_t)&mod_pycom_wifi_ssid_ap_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_pwd_sta),                    (mp_obj_t)&mod_pycom_wifi_pwd_sta_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_pwd_ap),                     (mp_obj_t)&mod_pycom_wifi_pwd_ap_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_mode_on_boot),               (mp_obj_t)&mod_pycom_wifi_mode_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_create_128bit_le_uuid_from_string), (mp_obj_t)&create_128bit_le_uuid_from_string_obj },


#if defined(FIPY) || defined(LOPY4) || defined(SIPY)
        { MP_OBJ_NEW_QSTR(MP_QSTR_sigfox_info),                     (mp_obj_t)&mod_pycom_sigfox_info_obj },
#endif

#if (VARIANT == PYBYTES)
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_device_token),            (mp_obj_t)&mod_pycom_pybytes_device_token_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_mqttServiceAddress),      (mp_obj_t)&mod_pycom_pybytes_mqttServiceAddress_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_userId),                  (mp_obj_t)&mod_pycom_pybytes_userId_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_network_preferences),     (mp_obj_t)&mod_pycom_pybytes_network_preferences_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_extra_preferences),       (mp_obj_t)&mod_pycom_pybytes_extra_preferences_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_force_update),            (mp_obj_t)&mod_pycom_pybytes_force_update_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_smart_config_on_boot),            (mp_obj_t)&mod_pycom_smartConfig_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_lte_config),              (mp_obj_t)&mod_pycom_pybytes_lte_config_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_on_boot),                 (mp_obj_t)&mod_pycom_pybytes_on_boot_obj },

#endif //(VARIANT == PYBYTES)
        { MP_OBJ_NEW_QSTR(MP_QSTR_bootmgr),                         (mp_obj_t)&mod_pycom_bootmgr_obj },

        // class constants
        { MP_OBJ_NEW_QSTR(MP_QSTR_FACTORY),                         MP_OBJ_NEW_SMALL_INT(0) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_OTA_0),                           MP_OBJ_NEW_SMALL_INT(1) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_FAT),                           MP_OBJ_NEW_SMALL_INT(0) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LittleFS),                        MP_OBJ_NEW_SMALL_INT(1) },

};

STATIC MP_DEFINE_CONST_DICT(pycom_module_globals, pycom_module_globals_table);

const mp_obj_module_t pycom_module = {
        .base = { &mp_type_module },
        .globals = (mp_obj_dict_t*)&pycom_module_globals,
};
