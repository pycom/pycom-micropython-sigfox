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
#include <string.h>


#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/gc.h"
#include "pybioctl.h"
#include "mpexception.h"
#include "machpwm.h"
#include "ledc.h"
#include "periph_ctrl.h"
#include "machpin.h"



STATIC mp_obj_t pwm_channel_duty(mp_obj_t self_in, mp_obj_t duty_o) {
    mach_pwm_channel_obj_t *self = self_in;
    float duty = mp_obj_get_float(duty_o);
    if (duty > 1.0f) {
        duty = 1.0f;
    } else if (duty < 0.0f) {
        duty = 0.0f;
    }

    uint32_t max_duty = (0x1 << ((mach_pwm_timer_obj_t *) MP_STATE_PORT(mach_pwm_timer_obj[self->config.timer_sel]))->config.bit_num) - 1;
    uint32_t duty_scaled =(uint32_t) (max_duty * 1.0f * duty);
    if (ledc_set_duty(self->config.speed_mode, self->config.channel, duty_scaled) != ESP_OK) { //set speed mode, channel, and duty.
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Failed to set duty-cycle"));
    }
    if (ledc_update_duty(self->config.speed_mode, self->config.channel) != ESP_OK){
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Failed to update duty-cycle"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mach_pwm_channel_duty_obj, pwm_channel_duty);


STATIC mp_obj_t mach_pwm_channel_init_helper(mach_pwm_channel_obj_t *self) {

    if (ledc_channel_config(&self->config) != ESP_OK){
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "PWM channel configuration failed"));
    }
    return mp_const_none;
}


STATIC const mp_map_elem_t mach_pwm_channel_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_duty_cycle),       (mp_obj_t)&mach_pwm_channel_duty_obj}
};
STATIC MP_DEFINE_CONST_DICT(mach_pwm_channel_locals_dict, mach_pwm_channel_locals_dict_table);


const mp_obj_type_t mach_pwm_channel_type = {
    { &mp_type_type },
    .name = MP_QSTR_channel,
    .locals_dict = (mp_obj_t)&mach_pwm_channel_locals_dict,
};


STATIC mp_obj_t mach_pwm_channel_make_new(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_id,              MP_ARG_REQUIRED | MP_ARG_INT, },
            { MP_QSTR_pin,             MP_ARG_REQUIRED | MP_ARG_KW_ONLY  | MP_ARG_OBJ, },
            { MP_QSTR_duty_cycle,      MP_ARG_OBJ,            {.u_obj = mp_const_none} }
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mach_pwm_timer_obj_t *pwm = pos_args[0];

    // work out the pwm timer id
    uint pwm_channel_id = args[0].u_int;
    if (pwm_channel_id > LEDC_CHANNEL_7) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // get the pwm pin
    pin_obj_t *pin = pin_find(args[1].u_obj);

    // get the correct pwm timer instance
    if (pwm->mach_pwm_channel_obj_t[pwm_channel_id] == NULL) {
        pwm->mach_pwm_channel_obj_t[pwm_channel_id] = gc_alloc(sizeof(mach_pwm_channel_obj_t), false);
    }
    mach_pwm_channel_obj_t *self = pwm->mach_pwm_channel_obj_t[pwm_channel_id];

    float duty = 0.5f;
    if (args[2].u_obj != mp_const_none) {
        //duty_cycle
        duty = mp_obj_get_float(args[2].u_obj);
        if (duty > 1.0f) {
            duty = 1.0f;
        } else if (duty < 0.0f) {
            duty = 0.0f;
        }
    }
    uint32_t max_duty = (0x1 << pwm->config.bit_num) - 1;
    uint32_t duty_scaled = (uint32_t) (max_duty * 1.0f * duty);

    self->base.type = &mach_pwm_channel_type;
    self->config.channel = (ledc_timer_t) pwm_channel_id;
    self->config.duty = duty_scaled;
    self->config.gpio_num = pin->pin_number;
    self->config.intr_type = LEDC_INTR_DISABLE;
    self->config.speed_mode = pwm->config.speed_mode;
    self->config.timer_sel = pwm->config.timer_num;

    // init the peripheral
    mach_pwm_channel_init_helper(self);

    if (ledc_set_duty(self->config.speed_mode, self->config.channel, duty_scaled) != ESP_OK) { // set speed mode, channel, and duty.
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Failed to set duty-cycle"));
    }
    if (ledc_update_duty(self->config.speed_mode, self->config.channel) != ESP_OK){
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Failed to update duty-cycle"));
    }

    return self;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_pwm_channel_obj, 1, mach_pwm_channel_make_new);

STATIC mp_obj_t mach_pwm_timer_init_helper(mach_pwm_timer_obj_t *self) {

    periph_module_enable(PERIPH_LEDC_MODULE);                //enable LEDC module, or you can not set any register of it.
    ledc_timer_config(&self->config);

    return mp_const_none;
}

STATIC const mp_arg_t mach_pwm_timer_init_args[] = {
    { MP_QSTR_timer,            MP_ARG_REQUIRED | MP_ARG_INT },
    { MP_QSTR_frequency,        MP_ARG_REQUIRED | MP_ARG_INT },
};
STATIC mp_obj_t mach_pwm_timer_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_pwm_timer_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_pwm_timer_init_args, args);

    // work out the pwm timer id
    uint pwm_timer_id = args[0].u_int;

    if (pwm_timer_id > LEDC_TIMER_3) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    uint32_t freq = args[1].u_int;

    // get the correct pwm timer instance or create one
    if (MP_STATE_PORT(mach_pwm_timer_obj[pwm_timer_id]) == NULL) {
        MP_STATE_PORT(mach_pwm_timer_obj[pwm_timer_id]) = gc_alloc(sizeof(mach_pwm_timer_obj_t), false);
        memset(((mach_pwm_timer_obj_t *) MP_STATE_PORT(mach_pwm_timer_obj[pwm_timer_id]))->mach_pwm_channel_obj_t,
            0,
            sizeof(((mach_pwm_timer_obj_t *) MP_STATE_PORT(mach_pwm_timer_obj[pwm_timer_id]))->mach_pwm_channel_obj_t));
    }
    mach_pwm_timer_obj_t *self = MP_STATE_PORT(mach_pwm_timer_obj[pwm_timer_id]);
    self->base.type = &mach_pwm_timer_type;
    self->config.timer_num = (ledc_timer_t) pwm_timer_id;
    self->config.freq_hz = freq;
    self->config.bit_num = LEDC_TIMER_12_BIT;
    self->config.speed_mode = LEDC_HIGH_SPEED_MODE;

    // start the peripheral
    mach_pwm_timer_init_helper(self);
    return self;
}


STATIC const mp_map_elem_t mach_pwm_timer_locals_dict_table[] = {
    // instance methods
    //{ MP_OBJ_NEW_QSTR(MP_QSTR_init),        (mp_obj_t)&mach_pwm_init_obj },
    //{ MP_OBJ_NEW_QSTR(MP_QSTR_deinit),      (mp_obj_t)&mach_pwm_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),       (mp_obj_t)&mach_pwm_channel_obj},

};
STATIC MP_DEFINE_CONST_DICT(mach_pwm_timer_locals_dict, mach_pwm_timer_locals_dict_table);


const mp_obj_type_t mach_pwm_timer_type = {
    { &mp_type_type },
    .name = MP_QSTR_PWM,
    //.print = mach_pwm_print,
    .make_new = mach_pwm_timer_make_new,
    .locals_dict = (mp_obj_t)&mach_pwm_timer_locals_dict,
};




