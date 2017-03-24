/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
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

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

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
#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"

typedef struct _machine_i2c_obj_t {
    mp_obj_base_t base;
    uint32_t us_delay;
    uint32_t baudrate;
    pin_obj_t *scl;
    pin_obj_t *sda;
} machine_i2c_obj_t;

#define PYBI2C_MASTER                          (0)

STATIC const mp_obj_t mach_i2c_def_pin[2] = {&PIN_MODULE_P9, &PIN_MODULE_P10};

STATIC void mp_hal_i2c_delay(machine_i2c_obj_t *self) {
    // We need to use an accurate delay to get acceptable I2C
    // speeds (eg 1us should be not much more than 1us).
    ets_delay_us(self->us_delay);
}

STATIC void mp_hal_i2c_scl_low(machine_i2c_obj_t *self) {
    self->scl->value = 0;
    pin_set_value(self->scl);
}

STATIC void mp_hal_i2c_scl_release(machine_i2c_obj_t *self) {
    self->scl->value = 1;
    pin_set_value(self->scl);
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

STATIC void mp_hal_i2c_start(machine_i2c_obj_t *self) {
    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_low(self);
    mp_hal_i2c_delay(self);
}

STATIC void mp_hal_i2c_stop(machine_i2c_obj_t *self) {
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_low(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
}

STATIC void mp_hal_i2c_init(machine_i2c_obj_t *self, uint32_t freq) {
    self->us_delay = 500000 / freq;
    if (self->us_delay == 0) {
        self->us_delay = 1;
    }
    pin_config (self->scl, -1, -1, GPIO_MODE_INPUT_OUTPUT_OD, MACHPIN_PULL_UP, 0);
    pin_config (self->sda, -1, -1, GPIO_MODE_INPUT_OUTPUT_OD, MACHPIN_PULL_UP, 0);
    mp_hal_i2c_stop(self);
}

STATIC int mp_hal_i2c_write_byte(machine_i2c_obj_t *self, uint8_t val) {
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);

    for (int i = 7; i >= 0; i--) {
        if ((val >> i) & 1) {
            mp_hal_i2c_sda_release(self);
        } else {
            mp_hal_i2c_sda_low(self);
        }
        mp_hal_i2c_delay(self);
        mp_hal_i2c_scl_release(self);
        mp_hal_i2c_delay(self);
        mp_hal_i2c_scl_low(self);
    }

    mp_hal_i2c_sda_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);

    int ret = mp_hal_i2c_sda_read(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);

    return !ret;
}

STATIC void mp_hal_i2c_write(machine_i2c_obj_t *self, uint8_t addr, uint8_t *data, size_t len) {
    mp_hal_i2c_start(self);
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
    }
    while (len--) {
        if (!mp_hal_i2c_write_byte(self, *data++)) {
            goto er;
        }
    }
    mp_hal_i2c_stop(self);
    return;

er:
    mp_hal_i2c_stop(self);
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
}

STATIC int mp_hal_i2c_read_byte(machine_i2c_obj_t *self, uint8_t *val, int nack) {
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);
    mp_hal_i2c_delay(self);

    uint8_t data = 0;
    for (int i = 7; i >= 0; i--) {
        mp_hal_i2c_scl_release(self);
        mp_hal_i2c_delay(self);
        data = (data << 1) | mp_hal_i2c_sda_read(self);
        mp_hal_i2c_scl_low(self);
        mp_hal_i2c_delay(self);
    }
    *val = data;

    // send ack/nack bit
    if (!nack) {
        mp_hal_i2c_sda_low(self);
    }
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_release(self);
    mp_hal_i2c_delay(self);
    mp_hal_i2c_scl_low(self);
    mp_hal_i2c_sda_release(self);

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

STATIC void mp_hal_i2c_write_mem(machine_i2c_obj_t *self, uint8_t addr, uint16_t memaddr, const uint8_t *src, size_t len) {
    // start the I2C transaction
    mp_hal_i2c_start(self);

    // write the slave address and the memory address within the slave
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
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

STATIC void mp_hal_i2c_read_mem(machine_i2c_obj_t *self, uint8_t addr, uint16_t memaddr, uint8_t *dest, size_t len) {
    // start the I2C transaction
    mp_hal_i2c_start(self);

    // write the slave address and the memory address within the slave
    if (!mp_hal_i2c_write_byte(self, addr << 1)) {
        goto er;
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

/******************************************************************************/
// MicroPython bindings for I2C

STATIC void pyb_i2c_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_i2c_obj_t *self = self_in;
    mp_printf(print, "I2C(0, I2C.MASTER, baudrate=%u)", self->baudrate);
}

STATIC mp_obj_t pyb_i2c_init_helper(machine_i2c_obj_t *self, const mp_arg_val_t *args) {
    // verify that mode is master
    if (args[0].u_int != PYBI2C_MASTER) {
        goto invalid_args;
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

    // init the I2C bus
    self->baudrate = args[1].u_int;
    mp_hal_i2c_init(self, args[1].u_int);

    // register it with the sleep module
    // pyb_sleep_add ((const mp_obj_t)self, (WakeUpCB_t)i2c_init);

    return mp_const_none;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t pyb_i2c_init_args[] = {
    { MP_QSTR_id,                          MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_mode,                        MP_ARG_INT, {.u_int = PYBI2C_MASTER} },
    { MP_QSTR_baudrate,  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 100000} },
    { MP_QSTR_pins,      MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t pyb_i2c_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_i2c_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), pyb_i2c_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // setup the object
    machine_i2c_obj_t *self = m_new_obj(machine_i2c_obj_t);
    self->base.type = &machine_i2c_type;

    // start the peripheral
    pyb_i2c_init_helper(self, &args[1]);

    return (mp_obj_t)self;
}

STATIC mp_obj_t machine_i2c_obj_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_i2c_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_i2c_init_args[1], args);
    return pyb_i2c_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_obj_init_obj, 1, machine_i2c_obj_init);

STATIC mp_obj_t machine_i2c_scan(mp_obj_t self_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    // 7-bit addresses 0b0000xxx and 0b1111xxx are reserved
    for (int addr = 0x08; addr < 0x78; ++addr) {
        mp_hal_i2c_start(self);
        int ack = mp_hal_i2c_write_byte(self, (addr << 1) | 1);
        if (ack) {
            mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(addr));
        }
        mp_hal_i2c_stop(self);
    }
    return list;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_scan_obj, machine_i2c_scan);

STATIC mp_obj_t machine_i2c_start(mp_obj_t self_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_hal_i2c_start(self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_start_obj, machine_i2c_start);

STATIC mp_obj_t machine_i2c_stop(mp_obj_t self_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_hal_i2c_stop(self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_i2c_stop_obj, machine_i2c_stop);

STATIC mp_obj_t machine_i2c_readinto(mp_obj_t self_in, mp_obj_t buf_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // get the buffer to read into
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);

    // do the read
    uint8_t *dest = bufinfo.buf;
    while (bufinfo.len--) {
        if (!mp_hal_i2c_read_byte(self, dest++, bufinfo.len == 0)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
        }
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(machine_i2c_readinto_obj, machine_i2c_readinto);

STATIC mp_obj_t machine_i2c_write(mp_obj_t self_in, mp_obj_t buf_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // get the buffer to write from
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    // do the write
    uint8_t *src = bufinfo.buf;
    while (bufinfo.len--) {
        if (!mp_hal_i2c_write_byte(self, *src++)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "I2C bus error"));
        }
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(machine_i2c_write_obj, machine_i2c_write);

STATIC mp_obj_t machine_i2c_readfrom(mp_obj_t self_in, mp_obj_t addr_in, mp_obj_t nbytes_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(nbytes_in));
    mp_hal_i2c_read(self, mp_obj_get_int(addr_in), (uint8_t*)vstr.buf, vstr.len);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
MP_DEFINE_CONST_FUN_OBJ_3(machine_i2c_readfrom_obj, machine_i2c_readfrom);

STATIC mp_obj_t machine_i2c_readfrom_into(mp_obj_t self_in, mp_obj_t addr_in, mp_obj_t buf_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);
    mp_hal_i2c_read(self, mp_obj_get_int(addr_in), (uint8_t*)bufinfo.buf, bufinfo.len);
    return mp_obj_new_int(bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_3(machine_i2c_readfrom_into_obj, machine_i2c_readfrom_into);

STATIC mp_obj_t machine_i2c_writeto(mp_obj_t self_in, mp_obj_t addr_in, mp_obj_t buf_in) {
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    mp_hal_i2c_write(self, mp_obj_get_int(addr_in), bufinfo.buf, bufinfo.len);
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(machine_i2c_writeto_obj, machine_i2c_writeto);

STATIC mp_obj_t machine_i2c_readfrom_mem(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_n, ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_memaddr, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_n,       MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        //{ MP_QSTR_addrsize, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} }, TODO
    };
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // create the buffer to store data into
    vstr_t vstr;
    vstr_init_len(&vstr, args[ARG_n].u_int);

    // do the transfer
    mp_hal_i2c_read_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, (uint8_t*)vstr.buf, vstr.len);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_mem_obj, 1, machine_i2c_readfrom_mem);

STATIC mp_obj_t machine_i2c_readfrom_mem_into(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_buf, ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_memaddr, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_buf,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        //{ MP_QSTR_addrsize, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} }, TODO
    };
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the buffer to store data into
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[ARG_buf].u_obj, &bufinfo, MP_BUFFER_WRITE);

    // do the transfer
    mp_hal_i2c_read_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, bufinfo.buf, bufinfo.len);
    return mp_obj_new_int(bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_readfrom_mem_into_obj, 1, machine_i2c_readfrom_mem_into);

STATIC mp_obj_t machine_i2c_writeto_mem(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_addr, ARG_memaddr, ARG_buf, ARG_addrsize };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_addr,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_memaddr, MP_ARG_REQUIRED | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_buf,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        //{ MP_QSTR_addrsize, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} }, TODO
    };
    machine_i2c_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the buffer to write the data from
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[ARG_buf].u_obj, &bufinfo, MP_BUFFER_READ);

    // do the transfer
    mp_hal_i2c_write_mem(self, args[ARG_addr].u_int, args[ARG_memaddr].u_int, bufinfo.buf, bufinfo.len);
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_i2c_writeto_mem_obj, 1, machine_i2c_writeto_mem);

STATIC const mp_rom_map_elem_t machine_i2c_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&machine_i2c_obj_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&machine_i2c_scan_obj) },

    // primitive I2C operations
    // { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&machine_i2c_start_obj) },
    // { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&machine_i2c_stop_obj) },
    // { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&machine_i2c_readinto_obj) },
    // { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&machine_i2c_write_obj) },

    // standard bus operations
    { MP_ROM_QSTR(MP_QSTR_readfrom), MP_ROM_PTR(&machine_i2c_readfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_readfrom_into), MP_ROM_PTR(&machine_i2c_readfrom_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeto), MP_ROM_PTR(&machine_i2c_writeto_obj) },

    // memory operations
    { MP_ROM_QSTR(MP_QSTR_readfrom_mem), MP_ROM_PTR(&machine_i2c_readfrom_mem_obj) },
    { MP_ROM_QSTR(MP_QSTR_readfrom_mem_into), MP_ROM_PTR(&machine_i2c_readfrom_mem_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeto_mem), MP_ROM_PTR(&machine_i2c_writeto_mem_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_MASTER),              MP_OBJ_NEW_SMALL_INT(PYBI2C_MASTER) },  // FIXME
};

STATIC MP_DEFINE_CONST_DICT(machine_i2c_locals_dict, machine_i2c_locals_dict_table);

const mp_obj_type_t machine_i2c_type = {
    { &mp_type_type },
    .name = MP_QSTR_I2C,
    .print = pyb_i2c_print,
    .make_new = pyb_i2c_make_new,
    .locals_dict = (mp_obj_dict_t*)&machine_i2c_locals_dict,
};
