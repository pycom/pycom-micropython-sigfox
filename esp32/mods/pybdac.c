/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


#include "py/runtime.h"

#include "analog.h"
#include "pybdac.h"
#include "mpexception.h"
#include "machpin.h"


/******************************************************************************
 DECLARE CONSTANTS
 ******************************************************************************/
#define PYB_DAC_NUM                         2
/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

typedef struct {
    mp_obj_base_t base;
    uint8_t id;
    uint8_t attn;
    bool enabled;
    bool tone;
    uint8_t dc_value;
    uint16_t tone_step;
    uint8_t tone_scale;
} pyb_dac_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC pyb_dac_obj_t pyb_dac_obj[PYB_DAC_NUM] = { {.id = 0, .enabled = false, .tone = false},
                                                  {.id = 1, .enabled = false, .tone = false} };


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC mp_obj_t dac_deinit(mp_obj_t self_in);

esp_err_t set_dac(void){

    uint8_t dac_enable = pyb_dac_obj[0].enabled | pyb_dac_obj[1].enabled<<1;
    uint8_t dac_tone_enable = pyb_dac_obj[0].tone | pyb_dac_obj[1].tone<<1;
    uint16_t dc_value = pyb_dac_obj[0].dc_value | pyb_dac_obj[1].dc_value<<8;
    uint16_t tone_scale = pyb_dac_obj[0].tone_scale | pyb_dac_obj[1].tone_scale<<8;
    uint16_t tone_step = pyb_dac_obj[0].tone_step;      //Shared between all DAC's

    return analog_dac_out(dac_enable, dac_tone_enable, dc_value, tone_scale, tone_step);
}

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
STATIC void pyb_dac_init (pyb_dac_obj_t *self) {
    self->enabled = true;
}

/******************************************************************************/
/* Micro Python bindings : dac object                                         */

STATIC void dac_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_dac_obj_t *self = self_in;
    mp_printf(print, "DAC(%q)", self->id);
}


STATIC const mp_arg_t pyb_dac_init_args[] = {
    { MP_QSTR_pin,         MP_ARG_OBJ, {.u_obj = mp_const_none} },
};

STATIC mp_obj_t dac_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_dac_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), pyb_dac_init_args, args);


    int8_t id;
    if (args[0].u_obj != mp_const_none) {
        pin_obj_t *_pin = pin_find(args[0].u_obj);
        id = _pin->pin_number;
        if (_pin->pin_number == 25) {
            id = 0;
        } else if (_pin->pin_number == 26) {
            id = 1;
        }
        else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
    } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // setup the object
    pyb_dac_obj_t *self = &pyb_dac_obj[id];
    self->base.type = &pyb_dac_type;
    pyb_dac_init(self);
    return self;
}


STATIC mp_obj_t dac_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_dac_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_dac_init_args[1], args);
    pyb_dac_init(pos_args[0]);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(dac_init_obj, 1, dac_init);


STATIC mp_obj_t dac_deinit(mp_obj_t self_in) {
    pyb_dac_obj_t *self = self_in;

    self->enabled = false;
    self->dc_value = 0;
    set_dac();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(dac_deinit_obj, dac_deinit);


STATIC mp_obj_t dac_write(mp_obj_t self_in, mp_obj_t value_o) {
    pyb_dac_obj_t *self = self_in;
    float value = mp_obj_get_float(value_o);
    if (value > 1.0f) {
        value = 1.0f;
    } else if (value < 0.0f) {
        value = 0.0f;
    }

    uint8_t value_scaled =(uint8_t) (0xff * 1.0f * value);
    self->tone = false;
    self->dc_value =value_scaled;
    self->tone_scale = 0;
    self->tone_step = 0;

    if (set_dac()) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "failed to set DAC value"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(dac_write_obj, dac_write);


STATIC mp_obj_t dac_tone(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_freq,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_scale,   MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
    };

    pyb_dac_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    //DAC1 and DAC2 use the same step value, tone_freq = 8M/(2^16/(tone_step+1)
    uint16_t tone =  args[0].u_int;
    if (tone >=125 && tone <= 20000) {
        tone += 125;
        pyb_dac_obj[0].tone_step = (tone * (1 << 16)) / 8000000 - 1;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "tone frequency out of range"));
    }

    uint16_t tone_scale =  args[1].u_int;
    if (tone_scale >= 0 && tone_scale <= 3) {
        self->tone_scale = tone_scale;
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "tone scale out of range"));
    }
    self->tone = true;
    self->dc_value = 0;

    if (set_dac()) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "failed to set DAC value"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(dac_tone_obj, 1, dac_tone);


STATIC const mp_map_elem_t dac_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&dac_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&dac_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),               (mp_obj_t)&dac_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tone),                (mp_obj_t)&dac_tone_obj },
};

STATIC MP_DEFINE_CONST_DICT(dac_locals_dict, dac_locals_dict_table);

const mp_obj_type_t pyb_dac_type = {
    { &mp_type_type },
    .name = MP_QSTR_DAC,
    .print = dac_print,
    .make_new = dac_make_new,
    .locals_dict = (mp_obj_t)&dac_locals_dict,
};
