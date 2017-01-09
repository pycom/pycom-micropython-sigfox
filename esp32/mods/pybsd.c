/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "esp_heap_alloc_caps.h"

#include "ff.h"
#include "diskio.h"
#include "sd_diskio.h"
#include "pybsd.h"
#include "mpexception.h"
#include "machpin.h"
#include "pins.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
pybsd_obj_t pybsd_obj = {.enabled = false};

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void pyb_sd_hw_init (pybsd_obj_t *self);
STATIC mp_obj_t pyb_sd_make_new (const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args);
STATIC mp_obj_t pyb_sd_deinit (mp_obj_t self_in);

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
/// initalizes the sd card hardware driver
STATIC void pyb_sd_hw_init (pybsd_obj_t *self) {
    if (!self->enabled) {
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        sdmmc_host_init();
        sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
        self->enabled = true;
    }
}

STATIC mp_obj_t pyb_sd_init_helper (pybsd_obj_t *self, const mp_arg_val_t *args) {
    pyb_sd_hw_init (self);
    if (sd_disk_init() != 0) {
        mp_raise_msg(&mp_type_OSError, mpexception_os_operation_failed);
    }

    // register it with the sleep module
    return mp_const_none;
}

/******************************************************************************/
// Micro Python bindings
//

STATIC const mp_arg_t pyb_sd_init_args[] = {
    { MP_QSTR_id,                          MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_freq,                        MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t pyb_sd_make_new (const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_sd_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), pyb_sd_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        mp_raise_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable);
    }

    // setup and initialize the object
    mp_obj_t self = &pybsd_obj;
    pybsd_obj.base.type = &pyb_sd_type;
    pyb_sd_init_helper (self, &args[1]);
    return self;
}

STATIC mp_obj_t pyb_sd_init (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_sd_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_sd_init_args[1], args);
    return pyb_sd_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pyb_sd_init_obj, 1, pyb_sd_init);

STATIC mp_obj_t pyb_sd_deinit (mp_obj_t self_in) {
    pybsd_obj_t *self = self_in;
    // de-initialze the sd card at diskio level
    sd_disk_deinit();
    sdmmc_host_deinit();
    // disable the peripheral
    self->enabled = false;
    // unregister it from the sleep module
    // pyb_sleep_remove (self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_sd_deinit_obj, pyb_sd_deinit);

STATIC const mp_map_elem_t pyb_sd_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),            (mp_obj_t)&pyb_sd_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),          (mp_obj_t)&pyb_sd_deinit_obj },
};

STATIC MP_DEFINE_CONST_DICT(pyb_sd_locals_dict, pyb_sd_locals_dict_table);

const mp_obj_type_t pyb_sd_type = {
    { &mp_type_type },
    .name = MP_QSTR_SD,
    .make_new = pyb_sd_make_new,
    .locals_dict = (mp_obj_t)&pyb_sd_locals_dict,
};
