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

#include "mpexception.h"

extern led_info_t led_info;

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

STATIC const mp_map_elem_t pycom_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_pycom) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_heartbeat),           (mp_obj_t)&mod_pycom_heartbeat_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rgbled),              (mp_obj_t)&mod_pycom_rgb_led_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_start),           (mp_obj_t)&mod_pycom_ota_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_write),           (mp_obj_t)&mod_pycom_ota_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ota_finish),          (mp_obj_t)&mod_pycom_ota_finish_obj },
};

STATIC MP_DEFINE_CONST_DICT(pycom_module_globals, pycom_module_globals_table);

const mp_obj_module_t pycom_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&pycom_module_globals,
};
