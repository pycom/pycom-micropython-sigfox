/*
 * Copyright (c) 2016, Pycom Limited.
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

#include "mpexception.h"
#include "machpin.h"
#include "driver/rmt.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern led_info_t led_info;


#define NVS_NAMESPACE                           "PY_NVM"

static nvs_handle pycom_nvs_handle;

void modpycom_init0(void) {
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &pycom_nvs_handle) != ESP_OK) {
        printf("Error while opening Pycom NVS name space\n");
    }
}

/******************************************************************************/
// Micro Python bindings

STATIC mp_obj_t mod_pycom_heartbeat (mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args) {
        mperror_enable_heartbeat (mp_obj_is_true(args[0]));
        return mp_const_none;
    } else {
        return mp_obj_new_bool(mperror_is_heartbeat_enabled());
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_pycom_heartbeat_obj, 0, 1, mod_pycom_heartbeat);

STATIC mp_obj_t mod_pycom_rgb_led (mp_obj_t o_color) {
    
    if (mperror_is_heartbeat_enabled()) {
       nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));    
    }
    
    uint32_t color = mp_obj_get_int(o_color);
    led_info.color.value = color;
    led_set_color(&led_info, true);
    
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

STATIC mp_obj_t mod_pycom_pulses_get (mp_obj_t gpio, mp_obj_t timeout) {
    rmt_config_t rmt_rx;
    rmt_rx.channel = RMT_CHANNEL_0;
    rmt_rx.gpio_num = pin_find(gpio)->pin_number;
    rmt_rx.clk_div = 80;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = 20000;
    rmt_config(&rmt_rx);
    rmt_driver_install(RMT_CHANNEL_0, 1000, 0);

    RingbufHandle_t rb = NULL;
    mp_obj_t pulses_l = mp_obj_new_list(0, NULL);

    //get RMT RX ringbuffer
    rmt_get_ringbuf_handler(RMT_CHANNEL_0, &rb);
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
    rmt_driver_uninstall(RMT_CHANNEL_0);

    return pulses_l;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_pulses_get_obj, mod_pycom_pulses_get);

STATIC mp_obj_t mod_pycom_nvs_set (mp_obj_t _key, mp_obj_t _value) {
    const char *key = mp_obj_str_get_str(_key);
    uint32_t value = mp_obj_get_int_truncated(_value);

    if (ESP_OK == nvs_set_u32(pycom_nvs_handle, key, value)) {
        nvs_commit(pycom_nvs_handle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_pycom_nvs_set_obj, mod_pycom_nvs_set);

STATIC mp_obj_t mod_pycom_nvs_get (mp_obj_t _key) {
    const char *key = mp_obj_str_get_str(_key);
    uint32_t value;

    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u32(pycom_nvs_handle, key, &value)) {
        // initialize the value to 0
        value = 0;
        if (ESP_OK == nvs_set_u32(pycom_nvs_handle, key, value)) {
            nvs_commit(pycom_nvs_handle);
        }
    }
    return mp_obj_new_int(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_pycom_nvs_get_obj, mod_pycom_nvs_get);


STATIC const mp_map_elem_t pycom_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_pycom) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat),           (mp_obj_t)&mod_pycom_heartbeat_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rgbled),              (mp_obj_t)&mod_pycom_rgb_led_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_start),           (mp_obj_t)&mod_pycom_ota_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_write),           (mp_obj_t)&mod_pycom_ota_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_finish),          (mp_obj_t)&mod_pycom_ota_finish_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pulses_get),          (mp_obj_t)&mod_pycom_pulses_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_set),             (mp_obj_t)&mod_pycom_nvs_set_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvs_get),             (mp_obj_t)&mod_pycom_nvs_get_obj },
};

STATIC MP_DEFINE_CONST_DICT(pycom_module_globals, pycom_module_globals_table);

const mp_obj_module_t pycom_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&pycom_module_globals,
};
