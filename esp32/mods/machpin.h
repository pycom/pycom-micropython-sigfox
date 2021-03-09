/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
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

#ifndef MACHPIN_H_
#define MACHPIN_H_

#include "gpio.h"

#define MACHPIN_PULL_NONE                 0x00
#define MACHPIN_PULL_UP                   0x01
#define MACHPIN_PULL_DOWN                 0x02

enum {
    PIN_TYPE_UART_TXD = 0,
    PIN_TYPE_UART_RXD,
    PIN_TYPE_UART_RTS,
    PIN_TYPE_UART_CTS,
};

enum {
    PIN_TYPE_SPI_CLK = 0,
    PIN_TYPE_SPI_MOSI,
    PIN_TYPE_SPI_MISO,
};

enum {
    PIN_TYPE_I2C_SDA = 0,
    PIN_TYPE_I2C_SCL,
};

typedef struct {
    mp_obj_base_t       base;
    qstr                name;
    mp_obj_t            handler;
    mp_obj_t            handler_arg;
    int16_t             af_in;
    int16_t             af_out;
    unsigned int        pin_number : 6;
    unsigned int        pull : 2;
    unsigned int        mode : 3;
    unsigned int        irq_trigger : 3;
    unsigned int        value : 1;
    unsigned int        hold : 1;
} pin_obj_t;

extern const mp_obj_type_t pin_type;

typedef struct {
    const char *name;
    const pin_obj_t *pin;
} pin_named_pin_t;

typedef struct {
    mp_obj_base_t base;
    qstr name;
    const pin_named_pin_t *named_pins;
} pin_named_pins_obj_t;

extern const mp_obj_type_t pin_cpu_pins_obj_type;
extern mp_obj_dict_t pin_cpu_pins_locals_dict;

extern const mp_obj_type_t pin_exp_board_pins_obj_type;
extern mp_obj_dict_t pin_exp_board_pins_locals_dict;

extern const mp_obj_type_t pin_module_pins_obj_type;
extern mp_obj_dict_t pin_module_pins_locals_dict;

extern void pin_preinit(void);
extern void pin_init0(void);
extern void pin_config (pin_obj_t *self, int af_in, int af_out, uint mode, uint pull, int value);
extern pin_obj_t *pin_find(mp_obj_t user_obj);
extern pin_obj_t *pin_find_pin_by_num (const mp_obj_dict_t *named_pins, uint pin_num);
extern uint32_t pin_get_value (const pin_obj_t* self);
extern void pin_extint_register(pin_obj_t *self, uint32_t trigger, uint32_t priority);
extern void machpin_register_irq_c_handler(pin_obj_t *self, void *handler);
extern void pin_irq_enable (mp_obj_t self_in);
extern void pin_irq_disable (mp_obj_t self_in);
extern void pin_set_value (const pin_obj_t* self);
extern uint32_t pin_get_value (const pin_obj_t* self);
extern void pin_deassign (pin_obj_t *self);

#endif  // MACHPIN_H_
