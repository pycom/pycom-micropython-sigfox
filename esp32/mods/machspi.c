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

#include "spi.h"
#include "machspi.h"
#include "mpexception.h"
#include "mpsleep.h"
#include "machpin.h"
#include "pins.h"

/// \moduleref pyb
/// \class SPI - a master-driven serial protocol

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct _mach_spi_obj_t {
    mp_obj_base_t base;
    pin_obj_t *pins[3];
    uint baudrate;
    uint config;
    uint spi_num;
    uint bitorder;
    byte polarity;
    byte phase;
    byte submode;
    byte wlen;
} mach_spi_obj_t;

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MACH_SPI_FIRST_BIT_MSB                    0
/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
#if defined(WIPY) || defined(GPY)
STATIC mach_spi_obj_t mach_spi_obj[2] = { {.baudrate = 0}, {.baudrate = 0} };
STATIC const mp_obj_t mach_spi_def_pin[2][3] = { {&PIN_MODULE_P10, &PIN_MODULE_P11, &PIN_MODULE_P14},
                                                 {&PIN_MODULE_P19, &PIN_MODULE_P20, &PIN_MODULE_P21} };
static const uint32_t mach_spi_pin_af[2][3] = { {HSPICLK_OUT_IDX, HSPID_OUT_IDX, HSPIQ_IN_IDX},
                                                {VSPICLK_OUT_IDX, VSPID_OUT_IDX, VSPIQ_IN_IDX} };
#else
STATIC mach_spi_obj_t mach_spi_obj[1] = { {.baudrate = 0} };
STATIC const mp_obj_t mach_spi_def_pin[1][3] = { {&PIN_MODULE_P10, &PIN_MODULE_P11, &PIN_MODULE_P14} };
static const uint32_t mach_spi_pin_af[1][3] = { {HSPICLK_OUT_IDX, HSPID_OUT_IDX, HSPIQ_IN_IDX} };
#endif

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
// only master mode is available for the moment
STATIC void machspi_init (const mach_spi_obj_t *self) {
    if (self->spi_num == SpiNum_SPI2) {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI_RST);
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI_CLK_EN_2);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI_RST_2);
    }

    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = self->submode, .speed = 80000000 / self->baudrate,
                           .bitOrder = self->bitorder, .halfMode = SpiWorkMode_Full};
    spi_init(self->spi_num, &spi_attr);
}

static int spi_master_send_recv_data(spi_num_e spiNum, spi_data_t* pData) {
    char idx = 0;
    if ((spiNum > SpiNum_Max)
        || (NULL == pData)) {
        return -1;
    }
    uint32_t *value;// = pData->rx_data;
    while (READ_PERI_REG(SPI_CMD_REG(spiNum))&SPI_USR);
    // Set command by user.
    if (pData->cmdLen != 0) {
        // Max command length 16 bits.
        SET_PERI_REG_BITS(SPI_USER2_REG(spiNum), SPI_USR_COMMAND_BITLEN,((pData->cmdLen << 3) - 1), SPI_USR_COMMAND_BITLEN_S);
        // Enable command
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_COMMAND);
        // Load command
        spi_master_cfg_cmd(spiNum, pData->cmd);
    } else {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_COMMAND);
        SET_PERI_REG_BITS(SPI_USER2_REG(spiNum), SPI_USR_COMMAND_BITLEN,0, SPI_USR_COMMAND_BITLEN_S);
    }
    // Set Address by user.
    if (pData->addrLen == 0) {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_ADDR);
        SET_PERI_REG_BITS(SPI_USER1_REG(spiNum), SPI_USR_ADDR_BITLEN,0, SPI_USR_ADDR_BITLEN_S);
    } else {
        if (NULL == pData->addr) {
            return -1;
        }
        SET_PERI_REG_BITS(SPI_USER1_REG(spiNum), SPI_USR_ADDR_BITLEN,((pData->addrLen << 3) - 1), SPI_USR_ADDR_BITLEN_S);
        // Enable address
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_ADDR);
        // Load address
        spi_master_cfg_addr(spiNum, *pData->addr);
    }
    value = pData->txData;
    // Set data by user.
    if(pData->txDataLen != 0) {
        if(NULL == value) {
            return -1;
        }
        //CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MISO);
        // Enable MOSI
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MOSI);
        // Load send buffer
        do {
            WRITE_PERI_REG((SPI_W0_REG(spiNum) + (idx << 2)), *value++);
        } while(++idx < ((pData->txDataLen / 4) + ((pData->txDataLen % 4) ? 1 : 0)));
        // Set data send buffer length.Max data length 64 bytes.
        SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, ((pData->txDataLen << 3) - 1),SPI_USR_MOSI_DBITLEN_S);
        SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, ((pData->rxDataLen << 3) - 1),SPI_USR_MISO_DBITLEN_S);
    } else {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MOSI);
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MISO);
        SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN,0, SPI_USR_MOSI_DBITLEN_S);
    }
    // Start send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spiNum))&SPI_USR);
    value = pData->rxData;
    // Read data out
    idx = 0;
    do {
        *value++ =  READ_PERI_REG(SPI_W0_REG(spiNum) + (idx << 2));
    } while (++idx < ((pData->rxDataLen / 4) + ((pData->rxDataLen % 4) ? 1 : 0)));
    return 0;
}

STATIC void pybspi_transfer (mach_spi_obj_t *self, const char *txdata, char *rxdata, uint32_t len, uint32_t *txchar) {
    if (!self->baudrate) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    // send and receive the data
    for (int i = 0; i < len; i += self->wlen) {
        uint32_t _rxdata = 0;
        uint32_t _txdata;
        if (txdata) {
            memcpy(&_txdata, &txdata[i], self->wlen);
        } else {
            if (txchar) {
                _txdata = *txchar;
            } else {
                _txdata = 0x55555555;
            }
        }
        spi_data_t spidata = {.cmd = 0, .cmdLen = 0, .addr = NULL, .addrLen = 0,
                                .txData = &_txdata, .txDataLen = self->wlen,
                                .rxData = &_rxdata, .rxDataLen = self->wlen};

        spi_master_send_recv_data(self->spi_num, &spidata);
        if (rxdata) {
            memcpy(&rxdata[i], &_rxdata, self->wlen);
        }
    }
}

static void spi_assign_pins_af (mach_spi_obj_t *self, mp_obj_t *pins) {
    uint32_t spi_idx = self->spi_num - 2;
    for (int i = 0; i < 3; i++) {
        if (pins[i] != mp_const_none) {
            pin_obj_t *pin = pin_find(pins[i]);
            int32_t af_in, af_out, mode;
            if (i == PIN_TYPE_SPI_MISO) {
                af_in = mach_spi_pin_af[spi_idx][i];
                af_out = -1;
                mode = GPIO_MODE_INPUT;
            } else {    // PIN_TYPE_SPI_CLK and PIN_TYPE_SPI_MOSI
                af_in = -1;
                af_out = mach_spi_pin_af[spi_idx][i];
                mode = GPIO_MODE_OUTPUT;
            }
            pin_config(pin, af_in, af_out, mode, MACHPIN_PULL_NONE, 0);
            self->pins[i] = pin;
        }
    }
}

static void spi_deassign_pins_af (mach_spi_obj_t *self) {
    for (int i = 0; i < 3; i++) {
        if (self->pins[i]) {
            // we must set the value to 0 so that when Rx pins are deassigned, their are hardwired to 0
            self->pins[i]->value = 0;
            pin_deassign(self->pins[i]);
            self->pins[i] = MP_OBJ_NULL;
        }
    }
}

/******************************************************************************/
/* Micro Python bindings                                                      */
/******************************************************************************/
STATIC void pyb_spi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mach_spi_obj_t *self = self_in;
    if (self->baudrate > 0) {
        mp_printf(print, "SPI(%d, SPI.MASTER, baudrate=%u, bits=%u, polarity=%u, phase=%u, firstbit=SPI.MSB)",
                  self->spi_num - 2, self->baudrate, (self->wlen * 8), self->polarity, self->phase);
    } else {
        mp_print_str(print, "SPI(0)");
    }
}

STATIC mp_obj_t pyb_spi_init_helper(mach_spi_obj_t *self, const mp_arg_val_t *args) {
    // verify that the mode is master
    if (args[0].u_int != SpiMode_Master) {
        goto invalid_args;
    }

    switch (args[2].u_int) {
    case 8:
        break;
    case 16:
        break;
    case 32:
        break;
    default:
        goto invalid_args;
        break;
    }
    self->wlen = args[2].u_int >> 3;

    self->polarity = args[3].u_int;
    self->phase = args[4].u_int;
    if (self->polarity > 1 || self->phase > 1) {
        goto invalid_args;
    }

    self->bitorder = args[5].u_int;
    if ((self->bitorder != SpiBitOrder_MSBFirst) && (self->bitorder != SpiBitOrder_LSBFirst)) {
        goto invalid_args;
    }

    // set the correct submode
    if (self->polarity == 0 && self->phase == 0) {
        self->submode = SpiSubMode_0;
    } else if (self->polarity == 0 && self->phase == 1) {
        self->submode = SpiSubMode_1;
    } else if (self->polarity == 1 && self->phase == 0) {
        self->submode = SpiSubMode_2;
    } else {
        self->submode = SpiSubMode_3;
    }

    self->baudrate = args[1].u_int;
    if (!self->baudrate) {
        goto invalid_args;
    }

    spi_deassign_pins_af(self);
    // assign the pins
    mp_obj_t pins_o = args[6].u_obj;
    if (pins_o != mp_const_none) {
        mp_obj_t *pins;
        if (pins_o == MP_OBJ_NULL) {
            // use the default pins
            pins = (mp_obj_t *)mach_spi_def_pin[self->spi_num - 2];
        } else {
            mp_obj_get_array_fixed_n(pins_o, 3, &pins);
        }
        spi_assign_pins_af (self, pins);
    }

    // init the bus
    machspi_init((const mach_spi_obj_t *)self);

    return mp_const_none;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

static const mp_arg_t pyb_spi_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_mode,                           MP_ARG_INT,  {.u_int = SpiMode_Master} },
    { MP_QSTR_baudrate,     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1000000} },    // 1MHz
    { MP_QSTR_bits,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 8} },
    { MP_QSTR_polarity,     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_phase,        MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_firstbit,     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = SpiBitOrder_MSBFirst} },
    { MP_QSTR_pins,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t pyb_spi_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_spi_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), pyb_spi_init_args, args);

    // check the peripheral id
#if defined(WIPY) || defined(GPY)
    if (args[0].u_int != 0 && args[0].u_int != 1) {
#else
    if (args[0].u_int != 0) {
#endif
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // setup the object
    mach_spi_obj_t *self = &mach_spi_obj[args[0].u_int];
    self->base.type = &mach_spi_type;
    self->spi_num = args[0].u_int + 2;

    // start the peripheral
    pyb_spi_init_helper(self, &args[1]);

    return self;
}

STATIC mp_obj_t pyb_spi_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(pyb_spi_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &pyb_spi_init_args[1], args);
    return pyb_spi_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pyb_spi_init_obj, 1, pyb_spi_init);

/// \method deinit()
/// Turn off the spi bus.
STATIC mp_obj_t pyb_spi_deinit(mp_obj_t self_in) {
    mach_spi_obj_t *self = self_in;
    if (self->baudrate > 0) {
        self->baudrate = 0;
        spi_deassign_pins_af(self);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pyb_spi_deinit_obj, pyb_spi_deinit);

STATIC mp_obj_t pyb_spi_write (mp_obj_t self_in, mp_obj_t buf) {
    // parse args
    mach_spi_obj_t *self = self_in;

    // get the buffer to send from
    mp_buffer_info_t bufinfo;
    uint8_t data[1];
    pyb_buf_get_for_send(buf, &bufinfo, data);

    // just send
    pybspi_transfer(self, (const char *)bufinfo.buf, NULL, bufinfo.len, NULL);

    // return the number of bytes written
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(pyb_spi_write_obj, pyb_spi_write);

STATIC mp_obj_t pyb_spi_read(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_nbytes,    MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_write,     MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0x00} },
    };

    // parse args
    mach_spi_obj_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // get the buffer to receive into
    vstr_t vstr;
    pyb_buf_get_for_recv(args[0].u_obj, &vstr);

    // just receive
    uint32_t write = args[1].u_int;
    pybspi_transfer(self, NULL, vstr.buf, vstr.len, &write);

    // return the received data
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pyb_spi_read_obj, 1, pyb_spi_read);

STATIC mp_obj_t pyb_spi_readinto(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_buf,       MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_write,     MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0x00} },
    };

    // parse args
    mach_spi_obj_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // get the buffer to receive into
    vstr_t vstr;
    pyb_buf_get_for_recv(args[0].u_obj, &vstr);

    // just receive
    uint32_t write = args[1].u_int;
    pybspi_transfer(self, NULL, vstr.buf, vstr.len, &write);

    // return the number of bytes received
    return mp_obj_new_int(vstr.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(pyb_spi_readinto_obj, 1, pyb_spi_readinto);

STATIC mp_obj_t pyb_spi_write_readinto (mp_obj_t self, mp_obj_t writebuf, mp_obj_t readbuf) {
    // get buffers to write from/read to
    mp_buffer_info_t bufinfo_write;
    uint8_t data_send[1];
    mp_buffer_info_t bufinfo_read;

    if (writebuf == readbuf) {
        // same object for writing and reading, it must be a r/w buffer
        mp_get_buffer_raise(writebuf, &bufinfo_write, MP_BUFFER_RW);
        bufinfo_read = bufinfo_write;
    } else {
        // get the buffer to write from
        pyb_buf_get_for_send(writebuf, &bufinfo_write, data_send);

        // get the read buffer
        mp_get_buffer_raise(readbuf, &bufinfo_read, MP_BUFFER_WRITE);
        if (bufinfo_read.len != bufinfo_write.len) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
        }
    }

    // send and receive
    pybspi_transfer(self, (const char *)bufinfo_write.buf, bufinfo_read.buf, bufinfo_write.len, NULL);

    // return the number of transferred bytes
    return mp_obj_new_int(bufinfo_write.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(pyb_spi_write_readinto_obj, pyb_spi_write_readinto);

STATIC const mp_map_elem_t pyb_spi_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&pyb_spi_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&pyb_spi_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),               (mp_obj_t)&pyb_spi_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                (mp_obj_t)&pyb_spi_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readinto),            (mp_obj_t)&pyb_spi_readinto_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write_readinto),      (mp_obj_t)&pyb_spi_write_readinto_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_MASTER),              MP_OBJ_NEW_SMALL_INT(SpiMode_Master) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MSB),                 MP_OBJ_NEW_SMALL_INT(SpiBitOrder_MSBFirst) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LSB),                 MP_OBJ_NEW_SMALL_INT(SpiBitOrder_LSBFirst) },
};

STATIC MP_DEFINE_CONST_DICT(pyb_spi_locals_dict, pyb_spi_locals_dict_table);

const mp_obj_type_t mach_spi_type = {
    { &mp_type_type },
    .name = MP_QSTR_SPI,
    .print = pyb_spi_print,
    .make_new = pyb_spi_make_new,
    .locals_dict = (mp_obj_t)&pyb_spi_locals_dict,
};
