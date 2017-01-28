/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef PYBPIN_H_
#define PYBPIN_H_

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
    int8_t              af_in;
    int8_t              af_out;
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

void pin_init0(void);
void pin_config (pin_obj_t *self, int af_in, int af_out, uint mode, uint pull, int value);
pin_obj_t *pin_find(mp_obj_t user_obj);
void pin_assign_pins_af (mp_obj_t *pins, uint32_t n_pins, uint32_t pull, uint32_t fn, uint32_t unit);
uint8_t pin_find_peripheral_unit (const mp_obj_t pin, uint8_t fn, uint8_t type);
uint8_t pin_find_peripheral_type (const mp_obj_t pin, uint8_t fn, uint8_t unit);
int8_t pin_find_af_index (const pin_obj_t* pin, uint8_t fn, uint8_t unit, uint8_t type);
uint32_t pin_get_value (const pin_obj_t* self);
void pin_extint_register(pin_obj_t *self, uint32_t trigger, uint32_t priority);
void machpin_register_irq_c_handler(pin_obj_t *self, void *handler);
void pin_irq_enable (mp_obj_t self_in);
void pin_irq_disable (mp_obj_t self_in);
void pin_set_value (const pin_obj_t* self);
uint32_t pin_get_value (const pin_obj_t* self);
#endif  // PYBPIN_H_
