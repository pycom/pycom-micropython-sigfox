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
#include <stdint.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mpstate.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_intr.h"
#include "driver/rtc_io.h"

#include "gpio.h"
#include "machpin.h"
#include "mpirq.h"
#include "pins.h"
//#include "pybsleep.h"
#include "mpexception.h"
#include "mperror.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"

/******************************************************************************
DECLARE PRIVATE FUNCTIONS
******************************************************************************/
STATIC pin_obj_t *pin_find_named_pin(const mp_obj_dict_t *named_pins, mp_obj_t name);
STATIC void pin_obj_configure (const pin_obj_t *self);
STATIC void pin_validate_mode (uint mode);
STATIC void pin_validate_pull (uint pull);
STATIC void machpin_intr_process (void* arg);

/******************************************************************************
DEFINE CONSTANTS
******************************************************************************/
#define MACHPIN_SIMPLE_OUTPUT               0x100
#define MACHPIN_SIMPLE_IN_LOW               0x30
#define MACHPIN_SIMPLE_IN_HIGH              0x38
#define ETS_GPIO_INUM                       13

/******************************************************************************
DEFINE TYPES
******************************************************************************/
//typedef struct {
//    bool       active;
//    int8_t     lpds;
//    int8_t     hib;
//} pybpin_wake_pin_t;

/******************************************************************************
DECLARE PRIVATE DATA
******************************************************************************/
//STATIC pybpin_wake_pin_t pybpin_wake_pin[PYBPIN_NUM_WAKE_PINS] =
//                                    { {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT},
//                                      {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT},
//                                      {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT},
//                                      {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT},
//                                      {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT},
//                                      {.active = false, .lpds = PYBPIN_WAKES_NOT, .hib = PYBPIN_WAKES_NOT} } ;
/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void pin_preinit(void) {
    gpio_isr_register(machpin_intr_process, NULL, ESP_INTR_FLAG_IRAM, NULL);
}

void pin_init0(void) {
    // initialize all pins as inputs with pull downs enabled
    mp_map_t *named_map = mp_obj_dict_get_map((mp_obj_t)&pin_module_pins_locals_dict);

    for (uint i = 0; i < named_map->used - 1; i++) {
        pin_obj_t *self = (pin_obj_t *)named_map->table[i].value;
        if (self != &PIN_MODULE_P1) {  // temporal while we remove all the IDF logs
        #ifdef DEBUG
            // Pins needed for JTAG
            if (self == &PIN_MODULE_P4 || self == &PIN_MODULE_P9 || self == &PIN_MODULE_P10 || self == &PIN_MODULE_P23) {
               continue;
            }
        #endif
        
        #if defined (FIPY)
            if (self == &PIN_MODULE_P17 || self == &PIN_MODULE_P18 || self == &PIN_MODULE_P19 || self == &PIN_MODULE_P20) {
               continue;
            }
        #elif defined (GPY)
            if (self == &PIN_MODULE_P5 || self == &PIN_MODULE_P6 || self == &PIN_MODULE_P7) {
               continue;
            }
        #endif
            /* exclude the antenna switch pin from initialization as it is already initialized */
            if((micropy_hw_antenna_diversity_pin_num == MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM) && (self == &PIN_MODULE_P12))
            {
                continue;
            }
#ifndef RGB_LED_DISABLE
            /* exclude RGB led from initialization as it is already initialized by mperror */
            if (self == &PIN_MODULE_P2)
            {
                continue;
            }
#endif
            pin_config(self, -1, -1, GPIO_MODE_INPUT, MACHPIN_PULL_DOWN, 0);
        }
    }
}

// C API used to convert a user-supplied pin name into an ordinal pin number
// If the pin is a board and it's not found in the pin list, it will be created
pin_obj_t *pin_find(mp_obj_t user_obj) {
    pin_obj_t *pin_obj;

    // if a pin was provided, use it
    if (MP_OBJ_IS_TYPE(user_obj, &pin_type)) {
        return user_obj;
    }

    // see if the pin name matches a expansion board pin
    pin_obj = pin_find_named_pin(&pin_exp_board_pins_locals_dict, user_obj);
    if (pin_obj) {
        return pin_obj;
    }

    // otherwise see if the pin name matches a module pin
    pin_obj = pin_find_named_pin(&pin_module_pins_locals_dict, user_obj);
    if (pin_obj) {
        return pin_obj;
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

void pin_config (pin_obj_t *self, int af_in, int af_out, uint mode, uint pull, int value) {
    self->mode = mode;
    self->pull = pull;
    self->af_in = af_in;
    self->af_out = af_out;

    // if value is < 0, then we want to keep it as it is
    if (value >= 0) {
        self->value = value;
    }

    // clear the interrupt configuration
    self->irq_trigger = GPIO_INTR_DISABLE;

    pin_obj_configure ((const pin_obj_t *)self);
}

void pin_irq_enable (mp_obj_t self_in) {
    gpio_intr_enable(((pin_obj_t *)self_in)->pin_number);
}

void pin_irq_disable (mp_obj_t self_in) {
    gpio_intr_disable(((pin_obj_t *)self_in)->pin_number);
}

int pin_irq_flags (mp_obj_t self_in) {
    return 0;
}

void pin_extint_register(pin_obj_t *self, uint32_t trigger, uint32_t priority) {
    self->irq_trigger = trigger;
    pin_obj_configure(self);
}

void pin_deassign (pin_obj_t *self) {
    if (self->af_in >= 0) {
        gpio_matrix_in(self->af_in, self->value ? MACHPIN_SIMPLE_IN_HIGH : MACHPIN_SIMPLE_IN_LOW, false);
    }
    if (self->af_out >= 0) {
        pin_set_value(self);
        gpio_matrix_out(self->pin_number, MACHPIN_SIMPLE_OUTPUT, false, false);
    }
    self->af_in = -1;
    self->af_out = -1;
}

IRAM_ATTR void pin_set_value (const pin_obj_t* self) {
    uint32_t pin_number = self->pin_number;
    if (pin_number < 32) {
        // set the pin value
        if (self->value) {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << pin_number);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << pin_number);
        }
    } else {
        if (self->value) {
            GPIO_REG_WRITE(GPIO_OUT1_W1TS_REG, 1 << (pin_number & 31));
        } else {
            GPIO_REG_WRITE(GPIO_OUT1_W1TC_REG, 1 << (pin_number & 31));
        }
    }
}

uint32_t pin_get_value (const pin_obj_t* self) {
    return gpio_get_level(self->pin_number);
}

IRAM_ATTR pin_obj_t *pin_find_pin_by_num (const mp_obj_dict_t *named_pins, uint pin_num) {
    mp_map_t *named_map = mp_obj_dict_get_map((mp_obj_t)named_pins);
    for (uint i = 0; i < named_map->used; i++) {
        if ((((pin_obj_t *)named_map->table[i].value)->pin_number == pin_num)) {
            return named_map->table[i].value;
        }
    }
    return NULL;
}

/******************************************************************************
DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC pin_obj_t *pin_find_named_pin(const mp_obj_dict_t *named_pins, mp_obj_t name) {
    mp_map_t *named_map = mp_obj_dict_get_map((mp_obj_t)named_pins);
    mp_map_elem_t *named_elem = mp_map_lookup(named_map, name, MP_MAP_LOOKUP);
    if (named_elem != NULL && named_elem->value != NULL) {
        return named_elem->value;
    }
    return NULL;
}

STATIC void pin_obj_configure (const pin_obj_t *self) {
    // set the value first (to minimize glitches)
    pin_set_value(self);

    // configure the pin
    gpio_config_t gpioconf = {.pin_bit_mask = 1ull << self->pin_number,
                              .mode = self->mode,
                              .pull_up_en = (self->pull == MACHPIN_PULL_UP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
                              .pull_down_en = (self->pull == MACHPIN_PULL_DOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
                              .intr_type = self->irq_trigger
                             };
    gpio_config(&gpioconf);

    // assign the alternate function
    if (self->mode == GPIO_MODE_INPUT) {
        if (self->af_in >= 0) {
            gpio_matrix_in(self->pin_number, self->af_in, false);
        }
    } else {    // output or open drain
        if (self->af_out >= 0) {
            gpio_matrix_out(self->pin_number, self->af_out, false, false);
        } else {
            gpio_matrix_out(self->pin_number, MACHPIN_SIMPLE_OUTPUT, false, false);
        }
    }
}

STATIC void pin_validate_mode (uint mode) {
    if (mode != GPIO_MODE_INPUT && mode != GPIO_MODE_OUTPUT && mode != GPIO_MODE_INPUT_OUTPUT_OD) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void pin_validate_pull (uint pull) {
    if (pull != MACHPIN_PULL_UP && pull != MACHPIN_PULL_DOWN) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void pin_interrupt_queue_handler(void *arg) {
    // this function will be called by the interrupt thread
    pin_obj_t *pin = arg;
    if (pin->handler && pin->handler != mp_const_none) {
        mp_call_function_1(pin->handler, pin->handler_arg);
    }
}

STATIC IRAM_ATTR void call_interrupt_handler (pin_obj_t *pin) {
    if (pin->handler) {
        if (pin->handler_arg == NULL) {
            // do a direct call (this means the pin has a C interupt handler)
            ((void(*)(void))pin->handler)();
        } else {
            // pass it to the queue
            mp_irq_queue_interrupt(pin_interrupt_queue_handler, pin);
        }
    }
}

STATIC IRAM_ATTR void machpin_intr_process (void* arg) {
    uint32_t gpio_num = 0;
    uint32_t mask;

    uint32_t gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
    uint32_t gpio_intr_status_h = READ_PERI_REG(GPIO_STATUS1_REG);

#ifdef MICROPY_LPWAN_DIO_PIN
    // fast path for the LPWAN DIO interrupt
    if (gpio_intr_status & (1 << micropy_lpwan_dio_pin_num)) {
        if(((pin_obj_t *)micropy_lpwan_dio_pin)->handler != NULL)
        {
            ((void(*)(void))((pin_obj_t *)micropy_lpwan_dio_pin)->handler)();
        }

        // clear this bit from the interrupt status
        gpio_intr_status &= ~(1 << micropy_lpwan_dio_pin_num);
        // clear the interrupt
        SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, (1 << micropy_lpwan_dio_pin_num));
    }
#endif

    mask = 1;
    while (gpio_num < 32) {
        if (gpio_intr_status & mask) {
            pin_obj_t *self = (pin_obj_t *)pin_find_pin_by_num(&pin_cpu_pins_locals_dict, gpio_num);
            call_interrupt_handler(self);
        }
        gpio_num++;
        mask <<= 1;
    }

    // now do the same with the high portion
    mask = 1;
    while (gpio_num < 40) {
        if (gpio_intr_status_h & mask) {
            pin_obj_t *self = (pin_obj_t *)pin_find_pin_by_num(&pin_cpu_pins_locals_dict, gpio_num);
            call_interrupt_handler(self);
        }
        gpio_num++;
        mask <<= 1;
    }

    // clear the interrupts
    SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, gpio_intr_status);
    SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, gpio_intr_status_h);
}

/******************************************************************************/
// Micro Python bindings

STATIC const mp_arg_t pin_init_args[] = {
    { MP_QSTR_mode,                        MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_pull,                        MP_ARG_OBJ, {.u_obj = mp_const_none} },
    { MP_QSTR_value,    MP_ARG_KW_ONLY  |  MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_alt,      MP_ARG_KW_ONLY  |  MP_ARG_OBJ, {.u_obj = mp_const_none} },
};
#define pin_INIT_NUM_ARGS MP_ARRAY_SIZE(pin_init_args)

STATIC mp_obj_t pin_obj_init_helper(pin_obj_t *self, mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[pin_INIT_NUM_ARGS];
    mp_arg_parse_all(n_args, pos_args, kw_args, pin_INIT_NUM_ARGS, pin_init_args, args);

    // get the io mode
    uint mode;
    //  default is input
    if (args[0].u_obj == MP_OBJ_NULL) {
        mode = GPIO_MODE_INPUT;
    } else {
        mode = mp_obj_get_int(args[0].u_obj);
        pin_validate_mode (mode);
    }

    // get the pull type
    uint pull;
    if (args[1].u_obj == mp_const_none) {
        pull = MACHPIN_PULL_NONE;
    } else {
        pull = mp_obj_get_int(args[1].u_obj);
        pin_validate_pull (pull);
    }

    // get the value
    int value = -1;
    if (args[2].u_obj != MP_OBJ_NULL) {
        if (mp_obj_is_true(args[2].u_obj)) {
            value = 1;
        } else {
            value = 0;
        }
    }

    // get the alternate function
    int af_in = -1, af_out = -1;
    int af = (args[3].u_obj != mp_const_none) ? mp_obj_get_int(args[3].u_obj) : -1;
    if (af > 255) {
        goto invalid_args;
    }
    if (mode == GPIO_MODE_INPUT) {
        af_in = af;
    } else if (mode == GPIO_MODE_OUTPUT) {
        af_out = af;
    } else {    // open drain
        af_in = af;
        af_out = af;
    }

    pin_config(self, af_in, af_out, mode, pull, value);

    return mp_const_none;

invalid_args:
   nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC void pin_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pin_obj_t *self = self_in;
    uint32_t pull = self->pull;

    // pin name
    mp_printf(print, "Pin('%q'", self->name);

    // pin mode
    qstr mode_qst;
    uint32_t mode = self->mode;
    if (mode == GPIO_MODE_INPUT) {
        mode_qst = MP_QSTR_IN;
    } else if (mode == GPIO_MODE_OUTPUT) {
        mode_qst = MP_QSTR_OUT;
    } else {
        mode_qst = MP_QSTR_OPEN_DRAIN;
    }
    mp_printf(print, ", mode=Pin.%q", mode_qst);

    // pin pull
    qstr pull_qst;
    if (pull == MACHPIN_PULL_NONE) {
        mp_printf(print, ", pull=%q", MP_QSTR_None);
    } else {
        if (pull == MACHPIN_PULL_UP) {
            pull_qst = MP_QSTR_PULL_UP;
        } else {
            pull_qst = MP_QSTR_PULL_DOWN;
        }
        mp_printf(print, ", pull=Pin.%q", pull_qst);
    }

    // pin af
    int alt = (self->af_in >= 0) ? self->af_in : self->af_out;
    mp_printf(print, ", alt=%d)", alt);
}

STATIC mp_obj_t pin_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);

    // Run an argument through the mapper and return the result.
    pin_obj_t *self = (pin_obj_t *)pin_find(args[0]);

    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    pin_obj_init_helper(self, n_args - 1, args + 1, &kw_args);

    return (mp_obj_t)self;
}

STATIC mp_obj_t pin_obj_init(mp_uint_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    return pin_obj_init_helper(args[0], n_args - 1, args + 1, kw_args);
}
MP_DEFINE_CONST_FUN_OBJ_KW(pin_init_obj, 1, pin_obj_init);

STATIC mp_obj_t pin_value(mp_uint_t n_args, const mp_obj_t *args) {
    pin_obj_t *self = args[0];
    if (n_args == 1) {
        // get the value
        if (self->mode == GPIO_MODE_OUTPUT) {
            return MP_OBJ_NEW_SMALL_INT(self->value);
        }
        return MP_OBJ_NEW_SMALL_INT(pin_get_value(self));
    } else {
        self->value = mp_obj_is_true(args[1]);
        pin_set_value(self);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_value_obj, 1, 2, pin_value);

STATIC mp_obj_t pin_toggle(mp_obj_t self_in) {
    pin_obj_t *self = self_in;
    self->value = !self->value;
    pin_set_value(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pin_toggle_obj, pin_toggle);

STATIC mp_obj_t pin_id(mp_obj_t self_in) {
    pin_obj_t *self = self_in;
    return MP_OBJ_NEW_QSTR(self->name);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pin_id_obj, pin_id);

STATIC mp_obj_t pin_mode(mp_uint_t n_args, const mp_obj_t *args) {
    pin_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->mode);
    } else {
        uint32_t mode = mp_obj_get_int(args[1]);
        pin_validate_mode(mode);
        self->mode = mode;
        pin_obj_configure(self);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_mode_obj, 1, 2, pin_mode);

STATIC mp_obj_t pin_pull(mp_uint_t n_args, const mp_obj_t *args) {
    pin_obj_t *self = args[0];
    if (n_args == 1) {
        if (self->pull == MACHPIN_PULL_NONE) {
            return mp_const_none;
        }
        return mp_obj_new_int(self->pull);
    } else {
        uint32_t pull;
        if (args[1] == mp_const_none) {
            pull = MACHPIN_PULL_NONE;
        } else {
            pull = mp_obj_get_int(args[1]);
            pin_validate_pull (pull);
        }
        self->pull = pull;
        pin_obj_configure(self);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_pull_obj, 1, 2, pin_pull);

STATIC mp_obj_t pin_hold(mp_uint_t n_args, const mp_obj_t *args) {
    pin_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_bool(self->hold);
    } else {
        self->hold = mp_obj_is_true(args[1]);
        if (self->hold == true) {
            if (ESP_OK != rtc_gpio_hold_en(self->pin_number)) {
                goto error;
            }
        } else {
            if (ESP_OK != rtc_gpio_hold_dis(self->pin_number)) {
                goto error;
            }
        }
    }
    return mp_const_none;

error:
    mp_raise_msg(&mp_type_OSError, "the hold functionality is only available for GPIO pins in the RTC domain");

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_hold_obj, 1, 2, pin_hold);

STATIC mp_obj_t pin_call(mp_obj_t self_in, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    mp_obj_t _args[2] = {self_in, *args};
    return pin_value (n_args + 1, _args);
}

//STATIC mp_obj_t pin_alt_list(mp_obj_t self_in) {
//    pin_obj_t *self = self_in;
//    mp_obj_t af[2];
//    mp_obj_t afs = mp_obj_new_list(0, NULL);
//
//    for (int i = 0; i < self->num_afs; i++) {
//        af[0] = MP_OBJ_NEW_QSTR(self->af_list[i].name);
//        af[1] = mp_obj_new_int(self->af_list[i].idx);
//        mp_obj_list_append(afs, mp_obj_new_tuple(MP_ARRAY_SIZE(af), af));
//    }
//    return afs;
//}
//STATIC MP_DEFINE_CONST_FUN_OBJ_1(pin_alt_list_obj, pin_alt_list);

STATIC void set_pin_callback_helper(mp_obj_t self_in, mp_obj_t handler, mp_obj_t handler_arg) {
    pin_obj_t *self = self_in;

    self->handler = handler;
    if (handler_arg == mp_const_none) {
        handler_arg = self_in;
    }
    self->handler_arg = handler_arg;
}

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t pin_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_INT,                  {.u_int = GPIO_INTR_DISABLE} },
        { MP_QSTR_handler,      MP_ARG_OBJ,                  {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                  {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    pin_obj_t *self = pos_args[0];

    pin_irq_disable(self);

    // enable the interrupt just before leaving
    if (args[0].u_int != GPIO_INTR_DISABLE && args[1].u_obj != mp_const_none) {
        set_pin_callback_helper(self, args[1].u_obj, args[2].u_obj);
        pin_extint_register(self, args[0].u_int, 0);
        mp_irq_add(self, args[1].u_obj);
        pin_irq_enable(self);
    } else {
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pin_callback_obj, 1, pin_callback);

void machpin_register_irq_c_handler(pin_obj_t *self, void *handler) {
    self->handler = handler;
    self->handler_arg = NULL;
}

STATIC const mp_map_elem_t pin_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&pin_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),                   (mp_obj_t)&pin_value_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_toggle),                  (mp_obj_t)&pin_toggle_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_id),                      (mp_obj_t)&pin_id_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mode),                    (mp_obj_t)&pin_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pull),                    (mp_obj_t)&pin_pull_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hold),                    (mp_obj_t)&pin_hold_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_alt_list),                (mp_obj_t)&pin_alt_list_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&pin_callback_obj },

    // class attributes
    { MP_OBJ_NEW_QSTR(MP_QSTR_module),                  (mp_obj_t)&pin_module_pins_obj_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_exp_board),               (mp_obj_t)&pin_exp_board_pins_obj_type },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_IN),                      MP_OBJ_NEW_SMALL_INT(GPIO_MODE_INPUT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_OUT),                     MP_OBJ_NEW_SMALL_INT(GPIO_MODE_OUTPUT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_OPEN_DRAIN),              MP_OBJ_NEW_SMALL_INT(GPIO_MODE_INPUT_OUTPUT_OD) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PULL_UP),                 MP_OBJ_NEW_SMALL_INT(MACHPIN_PULL_UP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PULL_DOWN),               MP_OBJ_NEW_SMALL_INT(MACHPIN_PULL_DOWN) },

//    { MP_OBJ_NEW_QSTR(MP_QSTR_LOW_POWER),               MP_OBJ_NEW_SMALL_INT(0) },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_MED_POWER),               MP_OBJ_NEW_SMALL_INT(1) },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_HIGH_POWER),              MP_OBJ_NEW_SMALL_INT(2) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_IRQ_FALLING),             MP_OBJ_NEW_SMALL_INT(GPIO_INTR_NEGEDGE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IRQ_RISING),              MP_OBJ_NEW_SMALL_INT(GPIO_INTR_POSEDGE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IRQ_LOW_LEVEL),           MP_OBJ_NEW_SMALL_INT(GPIO_INTR_LOW_LEVEL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IRQ_HIGH_LEVEL),          MP_OBJ_NEW_SMALL_INT(GPIO_INTR_HIGH_LEVEL) },
};

STATIC MP_DEFINE_CONST_DICT(pin_locals_dict, pin_locals_dict_table);

const mp_obj_type_t pin_type = {
    { &mp_type_type },
    .name = MP_QSTR_Pin,
    .print = pin_print,
    .make_new = pin_make_new,
    .call = pin_call,
    .locals_dict = (mp_obj_t)&pin_locals_dict,
};

STATIC void pin_named_pins_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    pin_named_pins_obj_t *self = self_in;
    mp_printf(print, "<Pin.%q>", self->name);
}

const mp_obj_type_t pin_module_pins_obj_type = {
    { &mp_type_type },
    .name = MP_QSTR_cpu,
    .print = pin_named_pins_obj_print,
    .locals_dict = (mp_obj_t)&pin_module_pins_locals_dict,
};

const mp_obj_type_t pin_exp_board_pins_obj_type = {
    { &mp_type_type },
    .name = MP_QSTR_board,
    .print = pin_named_pins_obj_print,
    .locals_dict = (mp_obj_t)&pin_exp_board_pins_locals_dict,
};
