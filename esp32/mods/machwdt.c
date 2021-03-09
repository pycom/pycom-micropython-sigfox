/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdio.h>
#include <string.h>

#include "py/nlr.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "timeutils.h"
#include "esp_system.h"
#include "mpexception.h"

#include <esp_types.h>
#include "esp_err.h"
#include "esp_intr.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "esp_freertos_hooks.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "esp_log.h"
#include "driver/timer.h"

#include "machwdt.h"


/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

typedef struct _mach_wdt_obj_t {
    mp_obj_base_t base;
    uint32_t timeout_ms;
    bool started;
} mach_wdt_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

STATIC mach_wdt_obj_t mach_wdt_obj;
const mp_obj_type_t mach_wdt_type;


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

static void task_wdt_isr (void *arg) {
    // ack the interrupt
    TIMERG0.int_clr_timers.wdt = 1;
}

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

void machine_wdt_start (uint32_t timeout_ms) {
    uint32_t timeout = (timeout_ms > 0) ? timeout_ms : 1;

    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_config0.en = 0;
    TIMERG0.wdt_config0.sys_reset_length = 7;                   // 3.2uS
    TIMERG0.wdt_config0.cpu_reset_length = 7;                   // 3.2uS
    TIMERG0.wdt_config0.level_int_en = 1;
    TIMERG0.wdt_config0.stg0 = TIMG_WDT_STG_SEL_INT;            // 1st stage timeout: interrupt
    TIMERG0.wdt_config0.stg1 = TIMG_WDT_STG_SEL_RESET_SYSTEM;   // 2nd stage timeout: reset system
    TIMERG0.wdt_config1.clk_prescale = 80 * 500;                // Prescaler: wdt counts in ticks of 0.5mS
    TIMERG0.wdt_config2 = timeout;                              // Set timeout before interrupt
    TIMERG0.wdt_config3 = timeout;                              // Set timeout before reset
    TIMERG0.wdt_config0.en = 1;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
    TIMERG0.int_clr_timers.wdt = 1;
    if (!mach_wdt_obj.started) {
        esp_intr_alloc(ETS_TG0_WDT_LEVEL_INTR_SOURCE, 0, task_wdt_isr, NULL, NULL);
        mach_wdt_obj.started = true;
    }
}

/******************************************************************************/
// Micro Python bindings

STATIC const mp_arg_t mach_wdt_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_timeout,      MP_ARG_REQUIRED | MP_ARG_INT, },
};
STATIC mp_obj_t mach_wdt_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_wdt_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_wdt_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        mp_raise_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable);
    }

    // setup the object
    mach_wdt_obj_t *self = &mach_wdt_obj;
    self->base.type = &mach_wdt_type;

    // setup the WDT and timeout
    machine_wdt_start(args[1].u_int);

    // return constant object
    return (mp_obj_t)&mach_wdt_obj;
}

STATIC mp_obj_t mach_wdt_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_wdt_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_wdt_init_args[1], args);
    machine_wdt_start(args[0].u_int);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_wdt_init_obj, 1, mach_wdt_init);

STATIC mp_obj_t mach_wdt_feed (mp_obj_t self_in) {
    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
    TIMERG0.wdt_feed = 1;
    TIMERG0.wdt_wprotect = 0;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_wdt_feed_obj, mach_wdt_feed);

STATIC const mp_map_elem_t mach_wdt_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&mach_wdt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_feed),                (mp_obj_t)&mach_wdt_feed_obj },
};
STATIC MP_DEFINE_CONST_DICT(mach_wdt_locals_dict, mach_wdt_locals_dict_table);

const mp_obj_type_t mach_wdt_type = {
    { &mp_type_type },
    .name = MP_QSTR_WDT,
    .make_new = mach_wdt_make_new,
    .locals_dict = (mp_obj_t)&mach_wdt_locals_dict,
};
