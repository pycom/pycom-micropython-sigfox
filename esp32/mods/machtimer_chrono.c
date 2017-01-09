#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <timer.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"

#include "esp_system.h"

#define CLK_FREQ (APB_CLK_FREQ / 2)

typedef struct {
    mp_obj_base_t base;
    uint64_t start;
    uint64_t accumulator;
} mp_obj_chrono_t;

STATIC mp_obj_t chrono_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_obj_chrono_t *self = m_new_obj(mp_obj_chrono_t);

    self->base.type = type;
    self->accumulator = 0;
    self->start = 0;

    return self;
}

STATIC mp_obj_t chrono_start(mp_obj_t self_in) {
    mp_obj_chrono_t *self = self_in;

    if (self->start != 0) {
        return mp_const_none;
    }

    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &self->start);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_start_obj, chrono_start);

STATIC mp_obj_t chrono_stop(mp_obj_t self_in) {
    uint64_t t;

    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);

    mp_obj_chrono_t *self = self_in;
    if (self->start == 0) {
        return mp_const_none;
    }

    self->accumulator += t - self->start;
    self->start = 0;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_stop_obj, chrono_stop);


STATIC inline uint64_t read_elapsed(mp_obj_t self_in) {
    uint64_t t;

    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &t);

    mp_obj_chrono_t *self = self_in;

    if (self->start == 0) {
        return self->accumulator;
    } else {
        return t - self->start + self->accumulator;
    }
}

STATIC mp_obj_t chrono_read(mp_obj_t self_in) {
    uint64_t t;

    t = read_elapsed(self_in);
    return mp_obj_new_float(t / (float) CLK_FREQ);
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_read_obj, chrono_read);

STATIC mp_obj_t chrono_read_ms(mp_obj_t self_in) {
    uint64_t t;

    t = read_elapsed(self_in);
    return mp_obj_new_float(t / (float) (CLK_FREQ / 1000));
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_read_ms_obj, chrono_read_ms);

STATIC mp_obj_t chrono_read_us(mp_obj_t self_in) {
    uint64_t t;

    t = read_elapsed(self_in);
    return mp_obj_new_float(t / (float) (CLK_FREQ / 1000000));
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_read_us_obj, chrono_read_us);

STATIC mp_obj_t chrono_reset(mp_obj_t self_in) {
    mp_obj_chrono_t *self = self_in;

    self->accumulator = 0;
    if (self->start) {
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &self->start);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(chrono_reset_obj, chrono_reset);


STATIC const mp_map_elem_t mach_timer_chrono_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),    MP_OBJ_NEW_QSTR(MP_QSTR_Chrono) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),        (mp_obj_t) &chrono_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read_ms),     (mp_obj_t) &chrono_read_ms_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read_us),     (mp_obj_t) &chrono_read_us_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start),       (mp_obj_t) &chrono_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop),        (mp_obj_t) &chrono_stop_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),       (mp_obj_t) &chrono_reset_obj },
};

STATIC MP_DEFINE_CONST_DICT(mach_timer_chrono_dict, mach_timer_chrono_dict_table);

const mp_obj_type_t mach_timer_chrono_type = {
   { &mp_type_type },
   .name = MP_QSTR_Chrono,
   .make_new = chrono_make_new,
   .locals_dict = (mp_obj_t)&mach_timer_chrono_dict,
};
