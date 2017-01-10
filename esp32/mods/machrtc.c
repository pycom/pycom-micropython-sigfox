/*
 * Copyright (c) 2016, Pycom Limited.
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
#include "py/runtime.h"
#include "timeutils.h"
#include "esp_system.h"
#include "mpexception.h"
#include "machrtc.h"

// next functions are from the IDF
extern uint64_t get_time_since_boot();
extern void rtc_calibrate_timer(int32_t adjust_value);
extern int32_t rtc_get_timer_calibration(void);

#define MAX_CAL_VAL ((1 << 27) - 1)

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

typedef struct _mach_rtc_obj_t {
    mp_obj_base_t base;
    uint64_t delta_from_epoch_til_boot;
} mach_rtc_obj_t;

STATIC mach_rtc_obj_t mach_rtc_obj;
const mp_obj_type_t mach_rtc_type;


void rtc_init0(void) {
    mach_rtc_set_us_since_epoch(0);
}

void mach_rtc_set_us_since_epoch(uint64_t nowus) {
    mach_rtc_obj.delta_from_epoch_til_boot = nowus - get_time_since_boot();
}

uint64_t mach_rtc_get_us_since_epoch(void) {
    return get_time_since_boot() + mach_rtc_obj.delta_from_epoch_til_boot;
};

STATIC uint64_t mach_rtc_datetime_us(const mp_obj_t datetime) {
    timeutils_struct_time_t tm;
    uint64_t useconds;

    // set date and time
    mp_obj_t *items;
    uint len;
    mp_obj_get_array(datetime, &len, &items);

    // verify the tuple
    if (len < 3 || len > 8) {
        mp_raise_ValueError(mpexception_value_invalid_arguments);
    }

    tm.tm_year = mp_obj_get_int(items[0]);
    tm.tm_mon = mp_obj_get_int(items[1]);
    tm.tm_mday = mp_obj_get_int(items[2]);
    if (len < 7) {
        useconds = 0;
    } else {
        useconds = mp_obj_get_int(items[6]);
    }
    if (len < 6) {
        tm.tm_sec = 0;
    } else {
        tm.tm_sec = mp_obj_get_int(items[5]);
    }
    if (len < 5) {
        tm.tm_min = 0;
    } else {
        tm.tm_min = mp_obj_get_int(items[4]);
    }
    if (len < 4) {
        tm.tm_hour = 0;
    } else {
        tm.tm_hour = mp_obj_get_int(items[3]);
    }
    useconds += 1000000 * timeutils_seconds_since_epoch(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return useconds;
}

/// The 8-tuple has the same format as CPython's datetime object:
///
///     (year, month, day, hours, minutes, seconds, milliseconds, tzinfo=None)
///
STATIC void mach_rtc_datetime(const mp_obj_t datetime) {
    uint64_t useconds;

    if (datetime != MP_OBJ_NULL) {
        useconds = mach_rtc_datetime_us(datetime);
        mach_rtc_set_us_since_epoch(useconds);
    } else {
        mach_rtc_set_us_since_epoch(0);
    }

    // set WLAN time and date, this is needed to verify certificates
    // #todo: something similar to the next line is needed
    // wlan_set_current_time(seconds);
}

/******************************************************************************/
// Micro Python bindings

STATIC const mp_arg_t mach_rtc_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_datetime,                       MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t mach_rtc_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rtc_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_rtc_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        mp_raise_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable);
    }

    // setup the object
    mach_rtc_obj_t *self = &mach_rtc_obj;
    self->base.type = &mach_rtc_type;

    // set the time and date
    if (args[1].u_obj != MP_OBJ_NULL) {
        mach_rtc_datetime(args[1].u_obj);
    }

    // return constant object
    return (mp_obj_t)&mach_rtc_obj;
}

STATIC mp_obj_t mach_rtc_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rtc_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_rtc_init_args[1], args);
    mach_rtc_datetime(args[0].u_obj);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_rtc_init_obj, 1, mach_rtc_init);

// STATIC mp_obj_t mach_rtc_deinit (mp_obj_t self_in) {
//     mach_rtc_set_us_since_epoch(0);
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_rtc_deinit_obj, mach_rtc_deinit);

STATIC mp_obj_t mach_rtc_now (mp_obj_t self_in) {
    timeutils_struct_time_t tm;
    uint64_t useconds;

    // get the time from the RTC
    useconds = mach_rtc_get_us_since_epoch();
    timeutils_seconds_since_epoch_to_struct_time(useconds / 1000000, &tm);

    mp_obj_t tuple[8] = {
        mp_obj_new_int(tm.tm_year),
        mp_obj_new_int(tm.tm_mon),
        mp_obj_new_int(tm.tm_mday),
        mp_obj_new_int(tm.tm_hour),
        mp_obj_new_int(tm.tm_min),
        mp_obj_new_int(tm.tm_sec),
        mp_obj_new_int(useconds % 1000000),
        mp_const_none
    };
    return mp_obj_new_tuple(8, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_rtc_now_obj, mach_rtc_now);

// calibration(None)
// calibration(cal)
// When an integer argument is provided, set the calibration value;
//      otherwise return calibration value
mp_obj_t mach_rtc_calibration(mp_uint_t n_args, const mp_obj_t *args) {
    mp_int_t cal;
    if (n_args == 2) {
        cal = mp_obj_get_int(args[1]);
        if ((cal > MAX_CAL_VAL) || (-cal > MAX_CAL_VAL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
                            "calibration value out of range"));
        }
        rtc_calibrate_timer(cal);
        return mp_const_none;
    } else {
        cal = rtc_get_timer_calibration();
        return mp_obj_new_int(cal);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mach_rtc_calibration_obj, 1, 2, mach_rtc_calibration);

STATIC const mp_map_elem_t mach_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),            (mp_obj_t)&mach_rtc_init_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),          (mp_obj_t)&mach_rtc_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_now),             (mp_obj_t)&mach_rtc_now_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_calibration),     (mp_obj_t)&mach_rtc_calibration_obj },
};
STATIC MP_DEFINE_CONST_DICT(mach_rtc_locals_dict, mach_rtc_locals_dict_table);

const mp_obj_type_t mach_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = mach_rtc_make_new,
    .locals_dict = (mp_obj_t)&mach_rtc_locals_dict,
};
