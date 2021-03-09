/*
 * Copyright (c) 2021, Pycom Limited.
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
#include "py/mperrno.h"
#include "lib/oofatfs/ff.h"
#include "lib/oofatfs/diskio.h"
#include "extmod/vfs_fat.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

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
        sdmmc_slot_config_t slot_config =
        {
            .gpio_cd = SDMMC_SLOT_NO_CD,
            .gpio_wp = SDMMC_SLOT_NO_WP,
            .width   = 1,  // single bit bus
        };

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

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_sd_deinit_obj, pyb_sd_deinit);

STATIC mp_obj_t pyb_sd_readblocks(mp_obj_t self, mp_obj_t block_num, mp_obj_t buf) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_WRITE);
    DRESULT res = sd_disk_read(bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / SD_SECTOR_SIZE);
    return MP_OBJ_NEW_SMALL_INT(res != RES_OK); // return of 0 means success
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(pyb_sd_readblocks_obj, pyb_sd_readblocks);

STATIC mp_obj_t pyb_sd_writeblocks(mp_obj_t self, mp_obj_t block_num, mp_obj_t buf) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    DRESULT res = sd_disk_write(bufinfo.buf, mp_obj_get_int(block_num), bufinfo.len / SD_SECTOR_SIZE);
    return MP_OBJ_NEW_SMALL_INT(res != RES_OK); // return of 0 means success
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(pyb_sd_writeblocks_obj, pyb_sd_writeblocks);
STATIC mp_obj_t pyb_sd_ioctl(mp_obj_t self, mp_obj_t cmd_in, mp_obj_t arg_in) {
    mp_int_t cmd = mp_obj_get_int(cmd_in);
    switch (cmd) {
        case BP_IOCTL_INIT:
        case BP_IOCTL_DEINIT:
        case BP_IOCTL_SYNC:
            // nothing to do
            return MP_OBJ_NEW_SMALL_INT(0); // success

        case BP_IOCTL_SEC_COUNT:
            return MP_OBJ_NEW_SMALL_INT(sdmmc_card_info.csd.capacity);

        case BP_IOCTL_SEC_SIZE:
            return MP_OBJ_NEW_SMALL_INT(sdmmc_card_info.csd.sector_size);

        default: // unknown command
            return MP_OBJ_NEW_SMALL_INT(-1); // error
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(pyb_sd_ioctl_obj, pyb_sd_ioctl);

STATIC const mp_map_elem_t pyb_sd_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),            (mp_obj_t)&pyb_sd_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),          (mp_obj_t)&pyb_sd_deinit_obj },
    // block device protocol
    { MP_OBJ_NEW_QSTR(MP_QSTR_readblocks), (mp_obj_t)&pyb_sd_readblocks_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_writeblocks), (mp_obj_t)&pyb_sd_writeblocks_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ioctl), (mp_obj_t)&pyb_sd_ioctl_obj },
};

STATIC MP_DEFINE_CONST_DICT(pyb_sd_locals_dict, pyb_sd_locals_dict_table);

const mp_obj_type_t pyb_sd_type = {
    { &mp_type_type },
    .name = MP_QSTR_SD,
    .make_new = pyb_sd_make_new,
    .locals_dict = (mp_obj_t)&pyb_sd_locals_dict,
};
