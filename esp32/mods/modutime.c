/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Daniel Campora
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "py/nlr.h"
#include "py/obj.h"
#include "py/gc.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/smallint.h"
#include "machrtc.h"
#include "machtimer.h"
#include "timeutils.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

extern uint64_t system_get_rtc_time(void);

static int32_t timezone_offset;

STATIC mp_obj_t seconds_to_tuple_helper(mp_uint_t n_args, const mp_obj_t *args, int32_t offset) {
    timeutils_struct_time_t tm;
    mp_time_t seconds;
    if (n_args == 0 || args[0] == mp_const_none) {
        seconds = mach_rtc_get_us_since_epoch() / 1000000ull;
    } else {
        seconds = mp_obj_get_int(args[0]);
    }

    seconds += offset;
    timeutils_seconds_since_epoch_to_struct_time(seconds, &tm);
    mp_obj_t tuple[8] = {
        tuple[0] = mp_obj_new_int(tm.tm_year),
        tuple[1] = mp_obj_new_int(tm.tm_mon),
        tuple[2] = mp_obj_new_int(tm.tm_mday),
        tuple[3] = mp_obj_new_int(tm.tm_hour),
        tuple[4] = mp_obj_new_int(tm.tm_min),
        tuple[5] = mp_obj_new_int(tm.tm_sec),
        tuple[6] = mp_obj_new_int(tm.tm_wday),
        tuple[7] = mp_obj_new_int(tm.tm_yday),
    };
    return mp_obj_new_tuple(8, tuple);
}

/// \module time - time related functions
///
/// The `time` module provides functions for getting the current time and date,
/// and for sleeping.

/// \function localtime([secs])
/// Convert a time expressed in seconds since Jan 1, 2000 into an 8-tuple which
/// contains: (year, month, mday, hour, minute, second, weekday, yearday)
/// If secs is not provided or None, then the current time from the RTC is used.
/// year includes the century (for example 2014)
/// month   is 1-12
/// mday    is 1-31
/// hour    is 0-23
/// minute  is 0-59
/// second  is 0-59
/// weekday is 0-6 for Mon-Sun.
/// yearday is 1-366
STATIC mp_obj_t time_localtime(mp_uint_t n_args, const mp_obj_t *args) {
    return seconds_to_tuple_helper(n_args, args, timezone_offset);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(time_localtime_obj, 0, 1, time_localtime);


STATIC mp_obj_t time_gmtime(mp_uint_t n_args, const mp_obj_t *args) {
    return seconds_to_tuple_helper(n_args, args, 0);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(time_gmtime_obj, 0, 1, time_gmtime);

/// \function mktime()
/// This is inverse function of localtime. It's argument is a full 8-tuple
/// which expresses a time as per localtime. It returns an integer which is
/// the number of seconds since the Epoch (Jan 1, 1970).
STATIC mp_obj_t time_mktime(mp_obj_t tuple) {
    mp_uint_t len;
    mp_obj_t *elem;
    mp_obj_get_array(tuple, &len, &elem);

    // localtime generates a tuple of len 8. CPython uses 9, so we accept both.
    if (len < 8 || len > 9) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_TypeError, "mktime needs a tuple of length 8 or 9 (%d given)", len));
    }

    return mp_obj_new_int(timeutils_mktime_since_epoch(mp_obj_get_int(elem[0]),
            mp_obj_get_int(elem[1]), mp_obj_get_int(elem[2]), mp_obj_get_int(elem[3]),
            mp_obj_get_int(elem[4]), mp_obj_get_int(elem[5])) - timezone_offset);
}
MP_DEFINE_CONST_FUN_OBJ_1(time_mktime_obj, time_mktime);

/// \function sleep(seconds)
/// Sleep for the given number of seconds.
STATIC mp_obj_t time_sleep(mp_obj_t seconds_o) {
    mp_hal_delay_ms(1000 * mp_obj_get_float(seconds_o));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(time_sleep_obj, time_sleep);

STATIC mp_obj_t time_sleep_ms(mp_obj_t arg) {
    mp_hal_delay_ms(mp_obj_get_int(arg));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(time_sleep_ms_obj, time_sleep_ms);

STATIC mp_obj_t time_sleep_us(mp_obj_t arg) {
    mp_hal_delay_us(mp_obj_get_int(arg));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(time_sleep_us_obj, time_sleep_us);

STATIC mp_obj_t time_ticks_ms(void) {
    return mp_obj_new_int_from_uint(mp_hal_ticks_ms());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(time_ticks_ms_obj, time_ticks_ms);

STATIC mp_obj_t time_ticks_us(void) {
    return mp_obj_new_int_from_uint(mp_hal_ticks_us());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(time_ticks_us_obj, time_ticks_us);

STATIC mp_obj_t time_ticks_cpu(void) {
   return mp_obj_new_int_from_uint(mp_hal_ticks_us_non_blocking());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(time_ticks_cpu_obj, time_ticks_cpu);

STATIC mp_obj_t time_ticks_diff(mp_obj_t end_in, mp_obj_t start_in) {
    int32_t start = mp_obj_get_int_truncated(start_in);
    int32_t end = mp_obj_get_int_truncated(end_in);
    return mp_obj_new_int(end - start);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(time_ticks_diff_obj, time_ticks_diff);

STATIC mp_obj_t time_ticks_add(mp_obj_t start_in, mp_obj_t delta_in) {
    uint32_t start = mp_obj_get_int_truncated(start_in);
    uint32_t delta = mp_obj_get_int_truncated(delta_in);
    return mp_obj_new_int_from_uint(start + delta);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(time_ticks_add_obj, time_ticks_add);

/// \function time()
/// Returns the number of seconds, as an integer, since 1/1/1970.
STATIC mp_obj_t time_time(void) {
   // get date and time
   return mp_obj_new_int_from_uint(mach_rtc_get_us_since_epoch() / 1000000ull);
}
MP_DEFINE_CONST_FUN_OBJ_0(time_time_obj, time_time);

/// \function time_timezone()
/// Return or set the timezone offset, in seconds
STATIC mp_obj_t time_timezone(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 0 || args[0] == mp_const_none) {
        return mp_obj_new_int(timezone_offset);
    } else {
        timezone_offset = mp_obj_get_int(args[0]);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(time_timezone_obj, 0, 1, time_timezone);

STATIC const mp_map_elem_t time_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_utime) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_localtime),           (mp_obj_t)&time_localtime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_gmtime),              (mp_obj_t)&time_gmtime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mktime),              (mp_obj_t)&time_mktime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep),               (mp_obj_t)&time_sleep_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep_ms),            (mp_obj_t)&time_sleep_ms_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep_us),            (mp_obj_t)&time_sleep_us_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ticks_ms),            (mp_obj_t)&time_ticks_ms_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ticks_us),            (mp_obj_t)&time_ticks_us_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ticks_cpu),           (mp_obj_t)&time_ticks_cpu_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ticks_add),           (mp_obj_t)&time_ticks_add_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ticks_diff),          (mp_obj_t)&time_ticks_diff_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_time),                (mp_obj_t)&time_time_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_timezone),            (mp_obj_t)&time_timezone_obj },
};

STATIC MP_DEFINE_CONST_DICT(time_module_globals, time_module_globals_table);

const mp_obj_module_t utime_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&time_module_globals,
};
