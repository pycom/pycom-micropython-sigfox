/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>

#include "py/mpstate.h"
#include "py/runtime.h"
#include "bufhelper.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "esp_types.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"

#include "adc.h"
#include "esp_adc_cal.h"
#include "pybadc.h"
#include "mpexception.h"
#include "mpsleep.h"
#include "machpin.h"
#include "pins.h"


/******************************************************************************
 DECLARE CONSTANTS
 ******************************************************************************/
#define PYB_ADC_NUM_CHANNELS                (ADC1_CHANNEL_MAX)
#define V_REF_NOM                           1100

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
    uint16_t vref;
    uint8_t width;
    bool enabled;
} pyb_adc_obj_t;

typedef struct {
    mp_obj_base_t base;
    esp_adc_cal_characteristics_t characteristics;
    pyb_adc_obj_t *adc;
    pin_obj_t *pin;
    uint8_t channel;
    uint8_t attn;
    bool calibrate;
    bool enabled;
} pyb_adc_channel_obj_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC pyb_adc_channel_obj_t pyb_adc_channel_obj[PYB_ADC_NUM_CHANNELS] = { {.pin = &PIN_MODULE_P13, .channel = ADC1_CHANNEL_0, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P14, .channel = ADC1_CHANNEL_1, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P15, .channel = ADC1_CHANNEL_2, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P16, .channel = ADC1_CHANNEL_3, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P19, .channel = ADC1_CHANNEL_4, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P20, .channel = ADC1_CHANNEL_5, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P18, .channel = ADC1_CHANNEL_6, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P17, .channel = ADC1_CHANNEL_7, .enabled = false}, };
STATIC pyb_adc_obj_t pyb_adc_obj = {.vref = V_REF_NOM, .enabled = false};
STATIC const mp_obj_type_t pyb_adc_channel_type;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC mp_obj_t adc_channel_deinit(mp_obj_t self_in);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
STATIC void pyb_adc_init (pyb_adc_obj_t *self) {
    adc1_config_width(self->width - 9);     // ADC_WIDTH_9Bit = 0
    self->enabled = true;
}

STATIC void pyb_adc_check_init(void) {
    // not initialized
    if (!pyb_adc_obj.enabled) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
}

STATIC void pyb_adc_channel_init (pyb_adc_channel_obj_t *self) {
    // the ADC block must be enabled first
    pyb_adc_check_init();
    adc1_config_channel_atten(self->channel, self->attn);
    esp_adc_cal_characterize(ADC_UNIT_1, self->attn, self->adc->width - 9,self->adc->vref, &self->characteristics);
    self->enabled = true;
}

/******************************************************************************/
/* Micro Python bindings : adc object                                         */

STATIC void adc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_adc_obj_t *self = self_in;
    if (self->enabled) {
        mp_printf(print, "ADC(0, bits=%d)", self->width);
    } else {
        mp_printf(print, "ADC(0)");
    }
}

STATIC const mp_arg_t pyb_adc_init_args[] = {
    { MP_QSTR_id,                          MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_bits,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 12} },
};
STATIC mp_obj_t adc_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_adc_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), pyb_adc_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // check the number of bits
    if (args[1].u_int < 9 || args[1].u_int > 12) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    // setup the object
    pyb_adc_obj_t *self = &pyb_adc_obj;
    self->base.type = &pyb_adc_type;
    self->width = args[1].u_int;

    // initialize and register with the sleep module
    pyb_adc_init(self);
    return self;
}

STATIC mp_obj_t adc_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_adc_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_adc_init_args[1], args);
    // check the number of bits
    if (args[0].u_int < 9 || args[0].u_int > 12) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
    pyb_adc_obj_t *self = pos_args[0];
    self->width = args[0].u_int;
    pyb_adc_init(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(adc_init_obj, 1, adc_init);

STATIC mp_obj_t adc_vref_to_pin(mp_obj_t self_in, mp_obj_t pin_o) {
    // get the verf out pin
    pin_obj_t *pin = pin_find(pin_o);
    if (pin->pin_number != 25 && pin->pin_number != 26 && pin->pin_number != 27) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "only pins P6, P21 and P22 are able to output VREF"));
    }
    adc2_vref_to_gpio(pin->pin_number);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(adc_vref_to_pin_obj, adc_vref_to_pin);

STATIC mp_obj_t adc_vref(uint n_args, const mp_obj_t *arg) {
    pyb_adc_obj_t *self = arg[0];
    if (n_args == 1) {
        return MP_OBJ_NEW_SMALL_INT(self->vref);
    } else {
        self->vref = mp_obj_get_int(arg[1]);
        // all active channels must be reinitialized next time .voltage() is called
        for (int i = 0; i < PYB_ADC_NUM_CHANNELS; i++) {
            pyb_adc_channel_obj[i].calibrate = true;
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(adc_vref_obj, 1, 2, adc_vref);

STATIC mp_obj_t adc_deinit(mp_obj_t self_in) {
    pyb_adc_obj_t *self = self_in;
    self->enabled = false;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_deinit_obj, adc_deinit);

STATIC mp_obj_t adc_channel(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t pyb_adc_channel_args[] = {
        { MP_QSTR_id,                          MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_attn,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = ADC_ATTEN_0db} },
        { MP_QSTR_pin,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    pyb_adc_obj_t *adc_o = pos_args[0];

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_adc_channel_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), pyb_adc_channel_args, args);

    int32_t ch_id = -1;
    if (args[0].u_obj != MP_OBJ_NULL) {
        ch_id = mp_obj_get_int(args[0].u_obj);
        if (ch_id >= PYB_ADC_NUM_CHANNELS) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_os_resource_not_avaliable));
        } else if (args[2].u_obj != mp_const_none) {
            if (pyb_adc_channel_obj[ch_id].pin != pin_find(args[2].u_obj)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
            }
        }
    } else {
        pin_obj_t *_pin = pin_find(args[2].u_obj);
        for (int i = 0; i < PYB_ADC_NUM_CHANNELS; i++) {
            if (pyb_adc_channel_obj[i].pin == _pin) {
                ch_id = i;
                break;
            }
        }
        if (ch_id < 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
    }

    if (args[1].u_int < ADC_ATTEN_0db || args[1].u_int > ADC_ATTEN_11db) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    // setup the object
    pyb_adc_channel_obj_t *self = &pyb_adc_channel_obj[ch_id];
    self->base.type = &pyb_adc_channel_type;
    self->attn = args[1].u_int;
    self->adc = adc_o;
    pyb_adc_channel_init (self);
    return self;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(adc_channel_obj, 1, adc_channel);

STATIC const mp_map_elem_t adc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&adc_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&adc_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),             (mp_obj_t)&adc_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_vref),                (mp_obj_t)&adc_vref_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_vref_to_pin),         (mp_obj_t)&adc_vref_to_pin_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ATTN_0DB),            MP_OBJ_NEW_SMALL_INT(ADC_ATTEN_0db) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ATTN_2_5DB),          MP_OBJ_NEW_SMALL_INT(ADC_ATTEN_2_5db) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ATTN_6DB),            MP_OBJ_NEW_SMALL_INT(ADC_ATTEN_6db) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ATTN_11DB),           MP_OBJ_NEW_SMALL_INT(ADC_ATTEN_11db) },
};

STATIC MP_DEFINE_CONST_DICT(adc_locals_dict, adc_locals_dict_table);

const mp_obj_type_t pyb_adc_type = {
    { &mp_type_type },
    .name = MP_QSTR_ADC,
    .print = adc_print,
    .make_new = adc_make_new,
    .locals_dict = (mp_obj_t)&adc_locals_dict,
};

STATIC void adc_channel_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_adc_channel_obj_t *self = self_in;
    if (self->enabled) {
        mp_printf(print, "ADCChannel(%u, pin=%q, attn=%d)", self->channel, self->pin->name, self->attn);
    } else {
        mp_printf(print, "ADCChannel(%u)", self->channel);
    }
}

STATIC mp_obj_t adc_channel_init(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    // re-init the channel
    pyb_adc_channel_init(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_init_obj, adc_channel_init);

STATIC mp_obj_t adc_channel_deinit(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    self->enabled = false;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_deinit_obj, adc_channel_deinit);

STATIC mp_obj_t adc_channel_value(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    // the channel must be enabled
    if (!self->enabled) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    return MP_OBJ_NEW_SMALL_INT(adc1_get_raw(self->channel));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_value_obj, adc_channel_value);

STATIC mp_obj_t adc_channel_voltage(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    uint32_t voltage;
    // the channel must be enabled
    if (!self->enabled) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (self->calibrate) {
        self->calibrate = false;
        esp_adc_cal_characterize(ADC_UNIT_1, self->attn, self->adc->width - 9,self->adc->vref, &self->characteristics);
    }
    esp_adc_cal_get_voltage(self->channel, &self->characteristics, &voltage);
    return MP_OBJ_NEW_SMALL_INT(voltage);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_voltage_obj, adc_channel_voltage);

STATIC mp_obj_t adc_channel_value_to_voltage(mp_obj_t self_in, mp_obj_t value_o) {
    pyb_adc_channel_obj_t *self = self_in;
    // the channel must be enabled
    if (!self->enabled) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    return MP_OBJ_NEW_SMALL_INT(esp_adc_cal_raw_to_voltage(mp_obj_get_int(value_o), &self->characteristics));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(adc_channel_value_to_voltage_obj, adc_channel_value_to_voltage);

STATIC mp_obj_t adc_channel_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    return adc_channel_value (self_in);
}

STATIC const mp_map_elem_t adc_channel_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&adc_channel_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&adc_channel_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),               (mp_obj_t)&adc_channel_value_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_voltage),             (mp_obj_t)&adc_channel_voltage_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value_to_voltage),    (mp_obj_t)&adc_channel_value_to_voltage_obj },
};

STATIC MP_DEFINE_CONST_DICT(adc_channel_locals_dict, adc_channel_locals_dict_table);

STATIC const mp_obj_type_t pyb_adc_channel_type = {
    { &mp_type_type },
    .name = MP_QSTR_ADCChannel,
    .print = adc_channel_print,
    .call = adc_channel_call,
    .locals_dict = (mp_obj_t)&adc_channel_locals_dict,
};
