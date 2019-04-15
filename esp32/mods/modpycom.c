/*
 * Copyright (c) 2018, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "mperror.h"
#include "updater.h"
#include "modled.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "entropy.h"
#include "pk.h"
#include "ctr_drbg.h"
#include "base64url.h"

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

#include "modmachine.h"


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
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_heartbeat_obj, 0, 1, mod_pycom_heartbeat);

STATIC mp_obj_t mod_pycom_rgb_led (mp_obj_t o_color) {

    if (mperror_is_heartbeat_enabled()) {
       nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    uint32_t color = mp_obj_get_int(o_color);
    led_info.color.value = color;
    led_set_color(&led_info, true, false);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_pycom_rgb_led_obj, mod_pycom_rgb_led);

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
    uint32_t value = mp_obj_get_int_truncated(_value);

    esp_err_t esp_err = nvs_set_u32(pycom_nvs_handle, key, value);
    if (ESP_OK == esp_err) {
        nvs_commit(pycom_nvs_handle);
    } else if (ESP_ERR_NVS_NOT_ENOUGH_SPACE == esp_err || ESP_ERR_NVS_PAGE_FULL == esp_err || ESP_ERR_NVS_NO_FREE_PAGES == esp_err) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "no space available"));
    } else if (ESP_ERR_NVS_INVALID_NAME == esp_err || ESP_ERR_NVS_KEY_TOO_LONG == esp_err) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid key (or too long)"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_nvs_set_obj, mod_pycom_nvs_set);

STATIC mp_obj_t mod_pycom_nvs_get (mp_obj_t _key) {
    const char *key = mp_obj_str_get_str(_key);
    uint32_t value;

    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u32(pycom_nvs_handle, key, &value)) {
        return mp_const_none;
    }
    return mp_obj_new_int(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_pycom_nvs_get_obj, mod_pycom_nvs_get);

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
    if (n_args) {
        config_set_wifi_on_boot (mp_obj_is_true(args[0]));
    } else {
        return mp_obj_new_bool(config_get_wifi_on_boot());
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_on_boot_obj, 0, 1, mod_pycom_wifi_on_boot);

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

STATIC mp_obj_t mod_pycom_wifi_ssid (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        const char *wifi_ssid = mp_obj_str_get_str(args[0]);
        config_set_wifi_ssid ((const uint8_t*)wifi_ssid);
    } else {
        uint8_t wifi_ssid[32];
        config_get_wifi_ssid(wifi_ssid);
        return mp_obj_new_str((const char*)wifi_ssid,strlen((const char*)wifi_ssid));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_ssid_obj, 0, 1, mod_pycom_wifi_ssid);

STATIC mp_obj_t mod_pycom_wifi_pwd (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        const char *wifi_pwd = mp_obj_str_get_str(args[0]);
        config_set_wifi_pwd ((const uint8_t*)wifi_pwd);
    } else {
        uint8_t wifi_pwd[64];
        config_get_wifi_pwd(wifi_pwd);
        return mp_obj_new_str((const char*)wifi_pwd,strlen((const char*)wifi_pwd));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_wifi_pwd_obj, 0, 1, mod_pycom_wifi_pwd);

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
        if(boot_info.Status == 0x00)
        {
            t->items[ARG_status] = mp_obj_new_str("Status: Check", strlen("Status: Check"));
        }
        else
        {
            t->items[ARG_status] = mp_obj_new_str("Status: Ready", strlen("Status: Ready"));
        }

        return MP_OBJ_FROM_PTR(t);

    } else {
            modpycom_bootmgr(args[ARG_boot_partition].u_int, args[ARG_fs_type].u_int, args[ARG_safeboot].u_int, args[3].u_bool);
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_pycom_bootmgr_obj, 0, mod_pycom_bootmgr);

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
#endif

STATIC mp_obj_t mod_pycom_generate_jwt_signature(mp_obj_t header_payload_in, mp_obj_t private_key_in) {

    /* The header and the payload must be UTF-8 and Base64Url encoded, delimited with ".", like: HEADER.PAYLOAD*/
    const char* header_payload = mp_obj_str_get_str(header_payload_in);
    const char* private_key = mp_obj_str_get_str(private_key_in);

    mbedtls_pk_context pk_context;
    mbedtls_pk_init(&pk_context);

    int rc = mbedtls_pk_parse_key(&pk_context, (const unsigned char*)private_key, strlen(private_key)+1, NULL, 0);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid Private Key, error code: %d", rc));
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    const char* pers="MyEntropy";

    mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)pers,
        strlen(pers));

    uint8_t digest[32];
    rc = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char*)header_payload, strlen(header_payload), digest);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Message Digest operation failed, error code: %d", rc));
    }

    unsigned char *signature = m_malloc(5000);
    size_t signature_length;

    rc = mbedtls_pk_sign(&pk_context, MBEDTLS_MD_SHA256, digest, sizeof(digest), signature, &signature_length, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Signing failed, error code: %d!", rc));
    }

    char *signature_base64url = m_malloc(1000);
    base64url_encode(signature, signature_length, signature_base64url);

    mp_obj_t ret_signature = mp_obj_new_bytearray(strlen(signature_base64url), signature_base64url);

    mbedtls_pk_free(&pk_context);
    m_free(header_payload);
    m_free(signature);
    m_free(signature_base64url);

    return ret_signature;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_generate_jwt_signature_obj, mod_pycom_generate_jwt_signature);

STATIC const mp_map_elem_t pycom_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_pycom) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat),                       (mp_obj_t)&mod_pycom_heartbeat_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rgbled),                          (mp_obj_t)&mod_pycom_rgb_led_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_start),                       (mp_obj_t)&mod_pycom_ota_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_write),                       (mp_obj_t)&mod_pycom_ota_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_finish),                      (mp_obj_t)&mod_pycom_ota_finish_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_verify),                      (mp_obj_t)&mod_pycom_ota_verify_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_slot),                        (mp_obj_t)&mod_pycom_ota_slot_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pulses_get),                      (mp_obj_t)&mod_pycom_pulses_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_set),                         (mp_obj_t)&mod_pycom_nvs_set_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_get),                         (mp_obj_t)&mod_pycom_nvs_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_erase),                       (mp_obj_t)&mod_pycom_nvs_erase_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_erase_all),                   (mp_obj_t)&mod_pycom_nvs_erase_all_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_on_boot),                    (mp_obj_t)&mod_pycom_wifi_on_boot_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wdt_on_boot),                     (mp_obj_t)&mod_pycom_wdt_on_boot_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wdt_on_boot_timeout),             (mp_obj_t)&mod_pycom_wdt_on_boot_timeout_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_ssid),                       (mp_obj_t)&mod_pycom_wifi_ssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_pwd),                        (mp_obj_t)&mod_pycom_wifi_pwd_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat_on_boot),               (mp_obj_t)&mod_pycom_heartbeat_on_boot_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_lte_modem_en_on_boot),            (mp_obj_t)&mod_pycom_lte_modem_on_boot_obj },
#if (VARIANT == PYBYTES)
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_device_token),            (mp_obj_t)&mod_pycom_pybytes_device_token_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_mqttServiceAddress),      (mp_obj_t)&mod_pycom_pybytes_mqttServiceAddress_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_userId),                  (mp_obj_t)&mod_pycom_pybytes_userId_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_network_preferences),     (mp_obj_t)&mod_pycom_pybytes_network_preferences_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_extra_preferences),       (mp_obj_t)&mod_pycom_pybytes_extra_preferences_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pybytes_force_update),            (mp_obj_t)&mod_pycom_pybytes_force_update_obj },
#endif
    { MP_OBJ_NEW_QSTR(MP_QSTR_bootmgr),                         (mp_obj_t)&mod_pycom_bootmgr_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_generate_jwt_signature),          (mp_obj_t)&mod_pycom_generate_jwt_signature_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_FACTORY),                         MP_OBJ_NEW_SMALL_INT(0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_OTA_0),                           MP_OBJ_NEW_SMALL_INT(1) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FAT),                             MP_OBJ_NEW_SMALL_INT(0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LittleFS),                        MP_OBJ_NEW_SMALL_INT(1) },

};

STATIC MP_DEFINE_CONST_DICT(pycom_module_globals, pycom_module_globals_table);

const mp_obj_module_t pycom_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&pycom_module_globals,
};
