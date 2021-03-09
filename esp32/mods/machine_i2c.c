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
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George on behalf of Pycom Ltd
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
#include "gpio.h"

#include "machine_i2c.h"
#include "machpin.h"
#include "pins.h"
#include "mpexception.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"

typedef struct _machine_i2c_obj_t {
    mp_obj_base_t base;
    uint32_t us_delay;
    uint32_t baudrate;
    pin_obj_t *scl;
    pin_obj_t *sda;
    uint8_t bus_id;
} machine_i2c_obj_t;

#define MACHI2C_MASTER                          (0)
#define I2C_ACK_CHECK_EN                        (1)
#define I2C_ACK_VAL                             (0)
#define I2C_NACK_VAL                            (1)


STATIC void mp_hal_i2c_stop(machine_i2c_obj_t *self);

STATIC machine_i2c_obj_t mach_i2c_obj[3];
STATIC const mp_obj_t mach_i2c_def_pin[2] = {&PIN_MODULE_P9, &PIN_MODULE_P10};
STATIC const uint32_t mach_i2c_pin_af[2][4] = { { I2CEXT0_SDA_OUT_IDX, I2CEXT0_SDA_IN_IDX, I2CEXT0_SCL_OUT_IDX, I2CEXT0_SCL_IN_IDX },
                                                { I2CEXT1_SDA_OUT_IDX, I2CEXT1_SDA_IN_IDX, I2CEXT1_SCL_OUT_IDX, I2CEXT1_SCL_IN_IDX } };
STATIC uint32_t isrmask;


STATIC void mp_hal_i2c_delay(machine_i2c_obj_t *self) {
    // We need to use an accurate delay to get acceptable I2C
    // speeds (eg 1us should be not much more than 1us).
    ets_delay_us(self->us_delay);
}

STATIC void mp_hal_i2c_sda_low(machine_i2c_obj_t *self) {
    self->sda->value = 0;
    pin_set_value(self->sda);
}

STATIC void mp_hal_i2c_sda_release(machine_i2c_obj_t *self) {
    self->sda->value = 1;
    pin_set_value(self->sda);
}

STATIC int mp_hal_i2c_sda_read(machine_i2c_obj_t *self) {
    return pin_get_value(self->sda);
}

STATIC int mp_hal_i2c_scl_read(machine_i2c_obj_t *self) {
    return pin_get_value(self->scl);
}

STATIC bool mp_hal_i2c_wait_for_scl(machine_i2c_obj_t *self) {
    uint32_t count = 0;
    // clock stretching?
    while (!mp_hal_i2c_scl_read(self)) {
        ets_delay_us(2);
        if (count++ > 5000) {       // timeout after 10ms
            return false;
        }
    }
    return true;
}

STATIC void mp_hal_i2c_scl_low(machine_i2c_obj_t *self) {
    self->scl->value = 0;
    pin_set_value(self->scl);
}

STATIC void mp_hal_i2c_scl_release(machine_i2c_obj_t *self) {
    self->scl->value = 1;
    pin_set_value(self->scl);
    // wait until the slave releases the clock
    if (!mp_hal_i2c_wait_for_scl(self)) {
        MICROPY_END_ATOMIC_SECTION(isrmask);
        mp_hal_i2c_stop(self);
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
    }
}

STATIC void mp_hal_i2c_start(machine_i2c_obj_t *self) {
    isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_low(self);
    mp_hal_i2c_delay(self);
    MICROPY_END_ATOMIC_SECTION(isrmask);
}

STATIC void mp_hal_i2c_stop(machine_i2c_obj_t *self) {
    isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_low(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
    MICROPY_END_ATOMIC_SECTION(isrmask);
}

STATIC void mp_hal_i2c_init(machine_i2c_obj_t *self) {
    self->us_delay = 500000 / self->baudrate;
    if (self->us_delay == 0) {
        self->us_delay = 1;
    }
    pin_config (self->scl, -1, -1, GPIO_MODE_INPUT_OUTPUT_OD, MACHPIN_PULL_UP, 1);
    pin_config (self->sda, -1, -1, GPIO_MODE_INPUT_OUTPUT_OD, MACHPIN_PULL_UP, 1);
}

STATIC int mp_hal_i2c_write_byte(machine_i2c_obj_t *self, uint8_t val) {

    mp_hal_i2c_delay(self);

    for (int i = 7; i >= 0; i--) {
        isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
        mp_hal_i2c_scl_low(self);
        if ((val >> i) & 1) {
            mp_hal_i2c_sda_release(self);
        } else {
            mp_hal_i2c_sda_low(self);
        }
        mp_hal_i2c_delay(self);
        mp_hal_i2c_scl_release(self);
        mp_hal_i2c_delay(self);
        MICROPY_END_ATOMIC_SECTION(isrmask);
    }

    isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_hal_i2c_scl_low(self);
    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    int ret = mp_hal_i2c_sda_read(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);
    MICROPY_END_ATOMIC_SECTION(isrmask);

    return !ret;
}

STATIC void mp_hal_i2c_write(machine_i2c_obj_t *self, uint8_t addr, uint8_t *data, size_t len, bool stop) {
    mp_hal_i2c_start(self);
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
    }
    while (len--) {
        if (!mp_hal_i2c_write_byte(self, *data++)) {
            goto er;
        }
    }
    if (stop) {
        mp_hal_i2c_stop(self);
    }
    return;

er:
    mp_hal_i2c_stop(self);
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
}

STATIC int mp_hal_i2c_read_byte(machine_i2c_obj_t *self, uint8_t *val, int nack) {

    mp_hal_i2c_delay(self);

    uint8_t data = 0;
    for (int i = 7; i >= 0; i--) {
        isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
        mp_hal_i2c_scl_low(self);
        mp_hal_i2c_delay(self);
        mp_hal_i2c_scl_release(self);
        mp_hal_i2c_delay(self);
        data = (data << 1) | mp_hal_i2c_sda_read(self);
        MICROPY_END_ATOMIC_SECTION(isrmask);
    }
    *val = data;

    isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
    mp_hal_i2c_scl_low(self);
    mp_hal_i2c_delay(self);
    // send ack/nack bit
    if (!nack) {
        mp_hal_i2c_sda_low(self);
    }
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);
    mp_hal_i2c_sda_release(self);
    MICROPY_END_ATOMIC_SECTION(isrmask);

    return 1; // success
}

STATIC void mp_hal_i2c_read(machine_i2c_obj_t *self, uint8_t addr, uint8_t *data, size_t len) {
    mp_hal_i2c_start(self);
    if (!mp_hal_i2c_write_byte(self, (addr << 1) | 1)) {
        goto er;
    }
    while (len--) {
        if (!mp_hal_i2c_read_byte(self, data++, len == 0)) {
            goto er;
        }
    }
    mp_hal_i2c_stop(self);
    return;

er:
    mp_hal_i2c_stop(self);
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
}

STATIC void mp_hal_i2c_write_mem(machine_i2c_obj_t *self, uint8_t addr, uint16_t memaddr, uint8_t addr_size, const uint8_t *src, size_t len) {
    // start the I2C transaction
    mp_hal_i2c_start(self);

    // write the slave address and the memory address within the slave
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
    }
    if (addr_size == 16) {
        if (!mp_hal_i2c_write_byte(self, memaddr >> 8)) {
            goto er;
        }
    }
    if (!mp_hal_i2c_write_byte(self, memaddr)) {
        goto er;
    }

    // write the buffer to the I2C memory
    while (len--) {
        if (!mp_hal_i2c_write_byte(self, *src++)) {
            goto er;
        }
    }

    // finish the I2C transaction
    mp_hal_i2c_stop(self);
    return;

er:
    mp_hal_i2c_stop(self);
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
}

STATIC void mp_hal_i2c_read_mem(machine_i2c_obj_t *self, uint8_t addr, uint16_t memaddr, uint8_t addr_size, uint8_t *dest, size_t len) {
    // start the I2C transaction
    mp_hal_i2c_start(self);

    // write the slave address and the memory address within the slave
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
    }
    if (addr_size == 16) {
        if (!mp_hal_i2c_write_byte(self, memaddr >> 8)) {
            goto er;
        }
    }
    if (!mp_hal_i2c_write_byte(self, memaddr)) {
        goto er;
    }

    // i2c_read will do a repeated start, and then read the I2C memory
    mp_hal_i2c_read(self, addr, dest, len);
    return;

er:
    mp_hal_i2c_stop(self);
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
}

STATIC void hw_i2c_initialise_master (machine_i2c_obj_t *i2c_obj) {

    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = i2c_obj->sda->pin_number;
    conf.scl_io_num = i2c_obj->scl->pin_number;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = i2c_obj->baudrate;

    i2c_param_config(i2c_obj->bus_id, &conf);
    i2c_driver_install(i2c_obj->bus_id, I2C_MODE_MASTER, 0, 0, 0);
}

STATIC void hw_i2c_master_writeto(machine_i2c_obj_t *i2c_obj, uint16_t slave_addr, uint32_t memaddr, uint8_t addr_size, uint8_t *data, uint16_t len, bool stop) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (slave_addr << 1) | I2C_MASTER_WRITE, I2C_ACK_CHECK_EN));

    if (addr_size > 0) {
        if (addr_size == 16) {
            ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (memaddr >> 8), I2C_ACK_CHECK_EN));
        }
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, memaddr, I2C_ACK_CHECK_EN));
    }

    ESP_ERROR_CHECK(i2c_master_write(cmd, data, len, I2C_ACK_CHECK_EN));
    if (stop) {
        ESP_ERROR_CHECK(i2c_master_stop(cmd));
    }

    esp_err_t ret = i2c_master_cmd_begin(i2c_obj->bus_id, cmd, (5000 + (1000 * len)) / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
    }
}

STATIC void hw_i2c_master_readfrom(machine_i2c_obj_t *i2c_obj, uint16_t slave_addr, uint32_t memaddr, uint8_t addr_size, uint8_t *data, uint16_t len) {

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    ESP_ERROR_CHECK(i2c_master_start(cmd));

    if (addr_size > 0) {
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, ( slave_addr << 1 ) | I2C_MASTER_WRITE, I2C_ACK_CHECK_EN));
        if (addr_size == 16) {
            ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (memaddr >> 8), I2C_ACK_CHECK_EN));
        }
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, memaddr, I2C_ACK_CHECK_EN));

        // repeated start
        ESP_ERROR_CHECK(i2c_master_start(cmd));
    }

    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, ( slave_addr << 1 ) | I2C_MASTER_READ, I2C_ACK_CHECK_EN));

    if (len > 1) {
        ESP_ERROR_CHECK(i2c_master_read(cmd, data, len - 1, I2C_ACK_VAL));
    }

    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data + len - 1, I2C_NACK_VAL));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));

    esp_err_t ret = i2c_master_cmd_begin(i2c_obj->bus_id, cmd, (5000 + (1000 * len)) / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
    }
}

STATIC bool hw_i2c_slave_ping (machine_i2c_obj_t *i2c_obj, uint16_t slave_addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (slave_addr << 1) | I2C_MASTER_WRITE, I2C_ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ret = i2c_master_cmd_begin(i2c_obj->bus_id, cmd, 5000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return (ret == ESP_OK) ? true : false;
}

STATIC void i2c_deassign_pins_af (machine_i2c_obj_t *self) {
    if (self->sda && self->scl) {
        // we must set the value to 1 so that when Rx pins are deassigned, their are hardwired to 1
        self->sda->value = 1;
        self->scl->value = 1;
        pin_deassign(self->sda);
        pin_deassign(self->scl);
        gpio_pullup_dis(self->sda->pin_number);
        gpio_pullup_dis(self->scl->pin_number);
        self->sda = MP_OBJ_NULL;
        self->scl = MP_OBJ_NULL;
    }
}

STATIC void i2c_validate_address_size(uint8_t addr_size) {
    if (addr_size != 8 && addr_size != 16) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid memory address size %d", addr_size));
    }
}

/******************************************************************************/
// MicroPython bindings for I2C

STATIC void machine_i2c_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_i2c_obj_t *self = self_in;
    if (self->baudrate > 0) {
        mp_printf(print, "I2C(%u, I2C.MASTER, baudrate=%u)", self->bus_id, self->baudrate);
    } else {
        mp_printf(print, "I2C(%u)", self->bus_id);
    }
}

STATIC mp_obj_t machine_i2c_init_helper(machine_i2c_obj_t *self, const mp_arg_val_t *args) {
    // verify that mode is master
    if (args[0].u_int != MACHI2C_MASTER) {
        goto invalid_args;
    }

    // before assigning the baudrate
    if (self->bus_id < 2) {
        if (self->baudrate > 0) {
            i2c_driver_delete(self->bus_id);
        }
        i2c_deassign_pins_af(self);
    }

    // assign the pins
    mp_obj_t pins_o = args[2].u_obj;
    if (pins_o != mp_const_none) {
        mp_obj_t *pins;
        if (pins_o == MP_OBJ_NULL) {
            // use the default pins
            pins = (mp_obj_t *)mach_i2c_def_pin;
        } else {
            mp_obj_get_array_fixed_n(pins_o, 2, &pins);
        }
        self->sda = pin_find(pins[0]);
        self->scl = pin_find(pins[1]);
    }

    // get the baudrate
    if (args[1].u_int > 0) {
        self->baudrate = args[1].u_int;
    } else {
        goto invalid_args;
    }

    if (self->bus_id < 2) {
        hw_i2c_initialise_master(self);
        // set the af values, so that deassign works later on
        if (self->sda && self->scl) {
            self->sda->af_out = mach_i2c_pin_af[self->bus_id][0];
            self->sda->af_in = mach_i2c_pin_af[self->bus_id][1];
            self->scl->af_out = mach_i2c_pin_af[self->bus_id][2];
            self->scl->af_in = mach_i2c_pin_af[self->bus_id][3];
        }
    } else {
        mp_hal_i2c_init(self);
    }

    return mp_const_none;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t machine_i2c_init_args[] = {
    { MP_QSTR_id,                          MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_mode,                        MP_ARG_INT, {.u_int = MACHI2C_MASTER} },
    { MP_QSTR_baudrate,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 100000} },
    { MP_QSTR_pins,      MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t machine_i2c_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(machine_i2c_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), machine_i2c_init_args, args);

    uint32_t bus_id = args[0].u_int;

    // check the peripheral id
    if (bus_id < 0 || bus_id > 2) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // setup the object
    machine_i2c_obj_t *self = &mach_i2c_obj[bus_id];
    self->base.type = &machine_i2c_type;
    self->bus_id = bus_id;

    // start the peripheral
    machine_i2c_init_helper(self, &args[1]);

    return (mp_obj_t)self;
}

STATIC mp_obj_t machine_i2c_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(machine_i2c_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &machine_i2c_init_args[1], args);
    return machine_i2c_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_init_obj, 1, machine_i2c_init);

STATIC mp_obj_t machine_i2c_scan(mp_obj_t self_in) {
    machine_i2c_obj_t *self = self_in;
    mp_obj_t list = mp_obj_new_list(0, NULL);

    // 7-bit addresses 0b0000xxx and 0b1111xxx are reserved

    if (self->bus_id < 2) {
        for (int addr = 0x08; addr <= 0x78; ++addr) {
            if (hw_i2c_slave_ping(self, addr)) {
                mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(addr));
            }
        }
    } else {
        for (int addr = 0x08; addr < 0x78; ++addr) {
            mp_hal_i2c_start(self);
            int ack = mp_hal_i2c_write_byte(self, (addr << 1) | 1);
            mp_hal_i2c_stop(self);
            if (ack) {
                mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(addr));
            }
        }
    }
    return list;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_scan_obj, machine_i2c_scan);

STATIC mp_obj_t machine_i2c_readfrom(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t machine_i2c_readfrom_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_nbytes,  MP_ARG_REQUIRED | MP_ARG_INT, },
    };

    machine_i2c_obj_t *self = pos_args[0];

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(machine_i2c_readfrom_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), machine_i2c_readfrom_args, args);

    vstr_t vstr;
    vstr_init_len(&vstr, args[1].u_int);
    if (self->bus_id < 2) {
        hw_i2c_master_readfrom(self, args[0].u_int, 0, 0, (uint8_t*)vstr.buf, vstr.len);
    } else {
        mp_hal_i2c_read(self, args[0].u_int, (uint8_t*)vstr.buf, vstr.len);
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_obj, 1, machine_i2c_readfrom);

STATIC mp_obj_t machine_i2c_readfrom_into(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    machine_i2c_obj_t *self = pos_args[0];

    STATIC const mp_arg_t machine_i2c_readfrom_into_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_buf,     MP_ARG_REQUIRED | MP_ARG_OBJ, },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(machine_i2c_readfrom_into_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), machine_i2c_readfrom_into_args, args);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1].u_obj, &bufinfo, MP_BUFFER_WRITE);
    if (self->bus_id < 2) {
        hw_i2c_master_readfrom(self, args[0].u_int, 0, 0, bufinfo.buf, bufinfo.len);
    } else {
        mp_hal_i2c_read(self, args[0].u_int, (uint8_t*)bufinfo.buf, bufinfo.len);
    }
    return mp_obj_new_int(bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_into_obj, 1, machine_i2c_readfrom_into);

STATIC mp_obj_t machine_i2c_writeto(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    machine_i2c_obj_t *self = pos_args[0];

    STATIC const mp_arg_t machine_i2c_writeto_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT,  },
        { MP_QSTR_buf,     MP_ARG_REQUIRED | MP_ARG_OBJ,  },
        { MP_QSTR_stop,    MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(machine_i2c_writeto_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), machine_i2c_writeto_args, args);

    mp_buffer_info_t bufinfo;
    uint8_t data[1];
    pyb_buf_get_for_send(args[1].u_obj, &bufinfo, data);

    if (self->bus_id < 2) {
        hw_i2c_master_writeto(self, args[0].u_int, 0, 0, bufinfo.buf, bufinfo.len, args[2].u_bool);
    } else {
        mp_hal_i2c_write(self, args[0].u_int, bufinfo.buf, bufinfo.len, args[2].u_bool);
    }
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_writeto_obj, 1, machine_i2c_writeto);

STATIC mp_obj_t machine_i2c_readfrom_mem(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_n , ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,     MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_memaddr,  MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_nbytes,   MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_addrsize, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 8} },
    };
    machine_i2c_obj_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    i2c_validate_address_size(args[ARG_addrsize].u_int);

    if (args[ARG_n].u_int > 0) {
        // create the buffer to store data into
        vstr_t vstr;
        vstr_init_len(&vstr, args[ARG_n].u_int);

        // do the transfer
        if (self->bus_id < 2) {
            hw_i2c_master_readfrom(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, (uint8_t *)vstr.buf, vstr.len);
        } else {
            mp_hal_i2c_read_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, (uint8_t*)vstr.buf, vstr.len);
        }
        return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
    }
    return mp_const_empty_bytes;
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_mem_obj, 1, machine_i2c_readfrom_mem);

STATIC mp_obj_t machine_i2c_readfrom_mem_into(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_buf, ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,     MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_memaddr,  MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_buf,      MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_addrsize, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 8} },
    };
    machine_i2c_obj_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    i2c_validate_address_size(args[ARG_addrsize].u_int);

    // get the buffer to store data into
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[ARG_buf].u_obj, &bufinfo, MP_BUFFER_WRITE);

    if (bufinfo.len > 0) {
        // do the transfer
        if (self->bus_id < 2) {
            hw_i2c_master_readfrom(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, bufinfo.buf, bufinfo.len);
        } else {
            mp_hal_i2c_read_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, bufinfo.buf, bufinfo.len);
        }
    }
    return mp_obj_new_int(bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_mem_into_obj, 1, machine_i2c_readfrom_mem_into);

STATIC mp_obj_t machine_i2c_writeto_mem(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_buf, ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,     MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_memaddr,  MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_buf,      MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_addrsize, MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 8} },
    };
    machine_i2c_obj_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    i2c_validate_address_size(args[ARG_addrsize].u_int);

    // get the buffer to write the data from
    mp_buffer_info_t bufinfo;
    uint8_t data[1];
    pyb_buf_get_for_send(args[ARG_buf].u_obj, &bufinfo, data);

    // do the transfer
    if (self->bus_id < 2) {
        hw_i2c_master_writeto(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, bufinfo.buf, bufinfo.len, true);
    } else {
        mp_hal_i2c_write_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, args[ARG_addrsize].u_int, bufinfo.buf, bufinfo.len);
    }

    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_writeto_mem_obj, 1, machine_i2c_writeto_mem);

STATIC mp_obj_t machine_i2c_deinit(mp_obj_t self_in) {
    machine_i2c_obj_t *self = self_in;

    if (self->baudrate > 0) {
        // before assigning the baudrate
        if (self->bus_id < 2) {
            i2c_driver_delete(self->bus_id);
            i2c_deassign_pins_af(self);
        }
        // invalidate the baudrate
        self->baudrate = 0;
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_deinit_obj, machine_i2c_deinit);

STATIC const mp_rom_map_elem_t machine_i2c_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init),                (mp_obj_t)&machine_i2c_init_obj },
    { MP_ROM_QSTR(MP_QSTR_deinit),              (mp_obj_t)&machine_i2c_deinit_obj },
    { MP_ROM_QSTR(MP_QSTR_scan),                (mp_obj_t)&machine_i2c_scan_obj },

    // standard bus operations
    { MP_ROM_QSTR(MP_QSTR_readfrom),            (mp_obj_t)&machine_i2c_readfrom_obj },
    { MP_ROM_QSTR(MP_QSTR_readfrom_into),       (mp_obj_t)&machine_i2c_readfrom_into_obj },
    { MP_ROM_QSTR(MP_QSTR_writeto),             (mp_obj_t)&machine_i2c_writeto_obj },

    // memory operations
    { MP_ROM_QSTR(MP_QSTR_readfrom_mem),        (mp_obj_t)&machine_i2c_readfrom_mem_obj },
    { MP_ROM_QSTR(MP_QSTR_readfrom_mem_into),   (mp_obj_t)&machine_i2c_readfrom_mem_into_obj },
    { MP_ROM_QSTR(MP_QSTR_writeto_mem),         (mp_obj_t)&machine_i2c_writeto_mem_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_MASTER),          MP_OBJ_NEW_SMALL_INT(MACHI2C_MASTER) },
};

STATIC MP_DEFINE_CONST_DICT(machine_i2c_locals_dict, machine_i2c_locals_dict_table);

const mp_obj_type_t machine_i2c_type = {
    { &mp_type_type },
    .name = MP_QSTR_I2C,
    .print = machine_i2c_print,
    .make_new = machine_i2c_make_new,
    .locals_dict = (mp_obj_dict_t*)&machine_i2c_locals_dict,
};
