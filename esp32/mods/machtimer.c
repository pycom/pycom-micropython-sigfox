/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include <timer.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"

#include "machtimer.h"
#include "machtimer_alarm.h"
#include "machtimer_chrono.h"

static uint64_t us_timer_calibration;

STATIC IRAM_ATTR mp_obj_t sleep_us(mp_obj_t delay);

void calibrate_us_timer(void) {
    uint64_t t1, t2;
    uint primsk;
    mp_obj_t tz = mp_obj_new_int(0);

    us_timer_calibration = 0;
    sleep_us(tz);
    primsk = MICROPY_BEGIN_ATOMIC_SECTION();
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t1);
    sleep_us(tz);
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t2);
    MICROPY_END_ATOMIC_SECTION(primsk);
    us_timer_calibration = t2 - t1;
}

void machtimer_preinit(void) {
    timer_config_t config = { .alarm_en = false,
                              .counter_en = false,
                              .counter_dir = TIMER_COUNT_UP,
                              .intr_type = TIMER_INTR_LEVEL,
                              .auto_reload = false, .divider = 2};

    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);
    calibrate_us_timer();
}

void machtimer_init0(void) {
    mach_timer_alarm_init_heap();
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
}

void machtimer_deinit(void) {
    timer_disable_intr(TIMER_GROUP_0, TIMER_0);
}

IRAM_ATTR uint64_t machtimer_get_timer_counter_value(void) {
    uint64_t t;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);
    return t;
}

STATIC IRAM_ATTR mp_obj_t sleep_us(mp_obj_t delay) {
    uint64_t d = mp_obj_get_int(delay);
    uint64_t t;
    bool unlock_gil = false;

    if (d >= 1000) {
        unlock_gil = true;
    }

    d *= CLK_FREQ / 1000000;

    d -= us_timer_calibration;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);
    d += t;

    if (unlock_gil) {
        MP_THREAD_GIL_EXIT();
        do {
            timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);
        } while(t < d);
        MP_THREAD_GIL_ENTER();

    } else {
        do {
            timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);
        } while(t < d);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sleep_us_fun_obj, sleep_us);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(sleep_us_obj, &sleep_us_fun_obj);

STATIC const mp_map_elem_t mach_timer_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_timer)},
    { MP_OBJ_NEW_QSTR(MP_QSTR_Alarm),               (mp_obj_t)&mach_timer_alarm_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Chrono),              (mp_obj_t)&mach_timer_chrono_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep_us),            (mp_obj_t)&sleep_us_obj },
};

STATIC MP_DEFINE_CONST_DICT(mach_timer_globals, mach_timer_globals_table);

const mp_obj_type_t mach_timer_type = {
   { &mp_type_type },
   .name = MP_QSTR_Timer,
   .locals_dict = (mp_obj_t)&mach_timer_globals,
};
