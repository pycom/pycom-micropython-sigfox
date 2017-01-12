/*
 * Copyright (c) 2016, Pycom Limited.
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

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "esp_types.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"

#include "analog.h"
#include "pybadc.h"
#include "mpexception.h"
#include "mpsleep.h"
#include "machpin.h"
#include "pins.h"


/******************************************************************************
 DECLARE CONSTANTS
 ******************************************************************************/
#define PYB_ADC_NUM_CHANNELS                (ADC1_CH_MAX)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
    bool enabled;
} pyb_adc_obj_t;

typedef struct {
    mp_obj_base_t base;
    pin_obj_t *pin;
    uint8_t channel;
    uint8_t id;
    uint8_t attn;
    bool enabled;
} pyb_adc_channel_obj_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC pyb_adc_channel_obj_t pyb_adc_channel_obj[PYB_ADC_NUM_CHANNELS] = { {.pin = &PIN_MODULE_P13, .channel = ADC1_CH0_GPIO36, .id = 0, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P14, .channel = ADC1_CH1_GPIO37, .id = 1, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P15, .channel = ADC1_CH2_GPIO38, .id = 2, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P16, .channel = ADC1_CH3_GPIO39, .id = 3, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P17, .channel = ADC1_CH7_GPIO35, .id = 4, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P18, .channel = ADC1_CH6_GPIO34, .id = 5, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P19, .channel = ADC1_CH4_GPIO32, .id = 6, .enabled = false},
                                                                           {.pin = &PIN_MODULE_P20, .channel = ADC1_CH5_GPIO33, .id = 7, .enabled = false} };
STATIC pyb_adc_obj_t pyb_adc_obj = {.enabled = false};

STATIC const mp_obj_type_t pyb_adc_channel_type;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC mp_obj_t adc_channel_deinit(mp_obj_t self_in);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
STATIC void pyb_adc_init (pyb_adc_obj_t *self) {
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
    // configure the pin in analog mode
    // pin_config (self->pin, -1, PIN_TYPE_ANALOG, PIN_TYPE_STD, -1, PIN_STRENGTH_2MA);
    // // enable the ADC channel
    // MAP_ADCChannelEnable(ADC_BASE, self->channel);
    self->enabled = true;
}

// STATIC void pyb_adc_deinit_all_channels (void) {
//     for (int i = 0; i < PYB_ADC_NUM_CHANNELS; i++) {
//         adc_channel_deinit(&pyb_adc_channel_obj[i]);
//     }
// }

/******************************************************************************/
/* Micro Python bindings : adc object                                         */

STATIC void adc_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pyb_adc_obj_t *self = self_in;
    if (self->enabled) {
        mp_printf(print, "ADC(0, bits=12)");
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
    if (args[1].u_int != 12) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    // setup the object
    pyb_adc_obj_t *self = &pyb_adc_obj;
    self->base.type = &pyb_adc_type;

    // initialize and register with the sleep module
    pyb_adc_init(self);
    //pyb_sleep_add ((const mp_obj_t)self, (WakeUpCB_t)pyb_adc_init);
    return self;
}

STATIC mp_obj_t adc_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_adc_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_adc_init_args[1], args);
    // check the number of bits
    if (args[0].u_int != 12) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
    pyb_adc_init(pos_args[0]);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(adc_init_obj, 1, adc_init);

STATIC mp_obj_t adc_deinit(mp_obj_t self_in) {
    pyb_adc_obj_t *self = self_in;
    self->enabled = false;
    // unregister it with the sleep module
    // pyb_sleep_remove ((const mp_obj_t)self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_deinit_obj, adc_deinit);

STATIC mp_obj_t adc_channel(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t pyb_adc_channel_args[] = {
        { MP_QSTR_id,                          MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_attn,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_pin,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

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

    if (args[1].u_int < ADC_ATTEN_0DB || args[1].u_int > ADC_ATTEN_12DB) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    // setup the object
    pyb_adc_channel_obj_t *self = &pyb_adc_channel_obj[ch_id];
    self->base.type = &pyb_adc_channel_type;
    self->attn = args[1].u_int;
    pyb_adc_channel_init (self);
    // register it with the sleep module
    // pyb_sleep_add ((const mp_obj_t)self, (WakeUpCB_t)pyb_adc_channel_init);
    return self;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(adc_channel_obj, 1, adc_channel);

STATIC const mp_map_elem_t adc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&adc_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&adc_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),             (mp_obj_t)&adc_channel_obj },
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
        mp_printf(print, "ADCChannel(%u, pin=%q)", self->id, self->pin->name);
    } else {
        mp_printf(print, "ADCChannel(%u)", self->id);
    }
}

STATIC mp_obj_t adc_channel_init(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    // re-enable it
    pyb_adc_channel_init(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_init_obj, adc_channel_init);

STATIC mp_obj_t adc_channel_deinit(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;

    // // unregister it with the sleep module
    // pyb_sleep_remove ((const mp_obj_t)self);
    self->enabled = false;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_deinit_obj, adc_channel_deinit);

STATIC mp_obj_t adc_channel_value(mp_obj_t self_in) {
    pyb_adc_channel_obj_t *self = self_in;
    uint32_t value;

    // the channel must be enabled
    if (!self->enabled) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    value = analog_adc1_read(self->channel, self->attn);
    return MP_OBJ_NEW_SMALL_INT(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(adc_channel_value_obj, adc_channel_value);

STATIC mp_obj_t adc_channel_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    return adc_channel_value (self_in);
}

STATIC const mp_map_elem_t adc_channel_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&adc_channel_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&adc_channel_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),               (mp_obj_t)&adc_channel_value_obj },
};

STATIC MP_DEFINE_CONST_DICT(adc_channel_locals_dict, adc_channel_locals_dict_table);

STATIC const mp_obj_type_t pyb_adc_channel_type = {
    { &mp_type_type },
    .name = MP_QSTR_ADCChannel,
    .print = adc_channel_print,
    .call = adc_channel_call,
    .locals_dict = (mp_obj_t)&adc_channel_locals_dict,
};
