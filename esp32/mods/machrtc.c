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

#include "lwip/apps/sntp.h"

#include "py/nlr.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "timeutils.h"
#include "esp_system.h"
#include "mpexception.h"
#include "machrtc.h"
#include "soc/rtc.h"
#include "esp_clk.h"
#include "esp_clk_internal.h"


uint32_t sntp_update_period = 3600000; // in ms

#define RTC_SOURCE_INTERNAL_RC                  RTC_SLOW_FREQ_RTC
#define RTC_SOURCE_EXTERNAL_XTAL                RTC_SLOW_FREQ_32K_XTAL
/* There is 8K of rtc_slow_memory, but some is used by the system software
    If the USER_MAXLEN is set to high, the following compile error will happen:
        region `rtc_slow_seg' overflowed by N bytes
    The current system software allows almost 4096 to be used.
    To avoid running into issues if the system software uses more, 2048 was picked as a max length
*/
#define MEM_USER_MAXLEN     2048

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

typedef struct _mach_rtc_obj_t {
    mp_obj_base_t base;
    mp_obj_t sntp_server_name;
    mp_obj_t sntp_backup_server_name;
    bool   synced;
} mach_rtc_obj_t;

static RTC_DATA_ATTR uint32_t rtc_user_mem_len;
static RTC_DATA_ATTR uint8_t rtc_user_mem_data[MEM_USER_MAXLEN];

STATIC mach_rtc_obj_t mach_rtc_obj;
const mp_obj_type_t mach_rtc_type;

void rtc_init0(void) {
    mach_rtc_set_us_since_epoch(0);
}

void mach_rtc_set_us_since_epoch(uint64_t nowus) {
    struct timeval tv;
    // store the packet timestamp
    tv.tv_usec = nowus % 1000000ull;
    tv.tv_sec = nowus / 1000000ull;
    settimeofday(&tv, NULL);
}

void mach_rtc_synced (void) {
    mach_rtc_obj.synced = true;
}

bool mach_is_rtc_synced (void) {
    return mach_rtc_obj.synced;
}

uint64_t mach_rtc_get_us_since_epoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000000ull ) + (tv.tv_usec);
    
}

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
    useconds += 1000000ull * timeutils_seconds_since_epoch(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return useconds;
}

/// The 8-tuple has the same format as CPython's datetime object:
///
///     (year, month, day, hours, minutes, seconds, milliseconds, tzinfo=None)
///
STATIC void mach_rtc_datetime(const mp_obj_t datetime) {
    uint64_t useconds;

    if (datetime != mp_const_none) {
        useconds = mach_rtc_datetime_us(datetime);
        mach_rtc_set_us_since_epoch(useconds);
    }
}

/******************************************************************************/
// Micro Python bindings

STATIC const mp_arg_t mach_rtc_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_datetime,                       MP_ARG_OBJ, {.u_obj = mp_const_none} },
    { MP_QSTR_source,                         MP_ARG_OBJ, {.u_obj = mp_const_none} },
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
    if (args[1].u_obj != mp_const_none) {
        mach_rtc_datetime(args[1].u_obj);
    }

    // change the RTC clock source
    if (args[2].u_obj != mp_const_none) {
        struct timeval now;
        gettimeofday(&now, NULL);

        uint32_t clk_src = mp_obj_get_int(args[2].u_obj);
        if (clk_src == RTC_SOURCE_EXTERNAL_XTAL) {
            if (!rtc_clk_32k_enabled()) {
                rtc_clk_32k_bootstrap(5);
            }
        }
        rtc_clk_select_rtc_slow_clk(clk_src);
        settimeofday(&now, NULL);
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

STATIC mp_obj_t mach_rtc_now (mp_obj_t self_in) {
    timeutils_struct_time_t tm;
    uint64_t useconds;
    
    useconds = mach_rtc_get_us_since_epoch();

    timeutils_seconds_since_epoch_to_struct_time((useconds) / 1000000ull, &tm);
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

STATIC mp_obj_t mach_rtc_ntp_sync(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_server,           MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_update_period,                      MP_ARG_INT, {.u_int = 3600} },
        { MP_QSTR_backup_server,                      MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    mach_rtc_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    sntp_update_period = args[1].u_int * 1000;
    if (sntp_update_period < 15000) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "update period cannot be shorter than 15 s"));
    }

    mach_rtc_obj.synced = false;
    if (sntp_enabled()) {
        sntp_stop();
    }

    if (args[0].u_obj != mp_const_none) {
        self->sntp_server_name = args[0].u_obj;
        sntp_setservername(0, (char *) mp_obj_str_get_str(self->sntp_server_name));
        if (args[2].u_obj != mp_const_none) {
            self->sntp_backup_server_name = args[2].u_obj;
            sntp_setservername(1, (char *) mp_obj_str_get_str(self->sntp_backup_server_name));
        }
        sntp_init();
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_rtc_ntp_sync_obj, 1, mach_rtc_ntp_sync);

STATIC mp_obj_t mach_rtc_has_synced (mp_obj_t self_in) {
    if (mach_rtc_obj.synced) {
        return mp_const_true;
    } else {
        return mp_const_false;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_rtc_has_synced_obj, mach_rtc_has_synced);

STATIC mp_obj_t machine_rtc_memory (size_t n_args, const mp_obj_t *args) {

    if (n_args == 1) {
        // read RTC memory
        uint32_t len = rtc_user_mem_len;
        uint8_t rtcram[MEM_USER_MAXLEN];
        memcpy( (char *) rtcram, (char *) rtc_user_mem_data, len);
        return mp_obj_new_bytes(rtcram,  len);
    } else {
        // write RTC memory
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);

        if (bufinfo.len > MEM_USER_MAXLEN) {
            mp_raise_ValueError("buffer too long");
        }
        memcpy( (char *) rtc_user_mem_data, (char *) bufinfo.buf, bufinfo.len);
        rtc_user_mem_len = bufinfo.len;
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rtc_memory_obj, 1, 2, machine_rtc_memory);

STATIC const mp_map_elem_t mach_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&mach_rtc_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_now),                 (mp_obj_t)&mach_rtc_now_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ntp_sync),            (mp_obj_t)&mach_rtc_ntp_sync_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_synced),              (mp_obj_t)&mach_rtc_has_synced_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_memory),                 (mp_obj_t)&machine_rtc_memory_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_INTERNAL_RC),         MP_OBJ_NEW_SMALL_INT(RTC_SOURCE_INTERNAL_RC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_XTAL_32KHZ),          MP_OBJ_NEW_SMALL_INT(RTC_SOURCE_EXTERNAL_XTAL) },
};
STATIC MP_DEFINE_CONST_DICT(mach_rtc_locals_dict, mach_rtc_locals_dict_table);

const mp_obj_type_t mach_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = mach_rtc_make_new,
    .locals_dict = (mp_obj_t)&mach_rtc_locals_dict,
};
