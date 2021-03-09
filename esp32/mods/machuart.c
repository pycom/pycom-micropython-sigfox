/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "readline.h"
#include "serverstask.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "esp_types.h"
#include "esp_attr.h"
#include "esp_intr.h"

#include "rom/ets_sys.h"
#include "soc/uart_struct.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"

#include "uart.h"
#include "machuart.h"
#include "mpexception.h"
#include "utils/interrupt_char.h"
#include "moduos.h"
#include "machpin.h"
#include "pins.h"
#include "periph_ctrl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"

/// \moduleref machine
/// \class UART - duplex serial communication bus
extern void IRAM_ATTR mp_hal_trig_term_sig(void);
/******************************************************************************
 DEFINE CONSTANTS
 *******-***********************************************************************/
#define MACHUART_FRAME_TIME_US(baud)            ((11 * 1000000) / baud)
#define MACHUART_RX_TIMEOUT_US(baud, nb_chars)  (((MACHUART_FRAME_TIME_US(baud)) * nb_chars) + 1)

#define MACHUART_TX_WAIT_US(baud)               ((MACHUART_FRAME_TIME_US(baud)) + 1)
#define MACHUART_TX_MAX_TIMEOUT_MS              (5)

#define MACHUART_RX_BUFFER_LEN                  (4096)
#define MACHUART_TX_FIFO_LEN                    (UART_FIFO_LEN)

// interrupt triggers
#define UART_TRIGGER_RX_ANY                     (0x01)
#define UART_TRIGGER_RX_HALF                    (0x02)
#define UART_TRIGGER_RX_FULL                    (0x04)
#define UART_TRIGGER_TX_DONE                    (0x08)

#define MACH_UART_CHECK_INIT(self)                    \
    if(!(self->init)) {nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "UART not Initialized!"));}

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool uart_tx_fifo_space (mach_uart_obj_t *self);
STATIC mp_obj_t mach_uart_deinit(mp_obj_t self_in);

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
struct _mach_uart_obj_t {
    mp_obj_base_t base;
    uart_dev_t* uart_reg;
    pin_obj_t *pins[4];
    uart_config_t config;
    uint8_t irq_flags;
    uint8_t uart_id;
    uint8_t rx_timeout;
    uint8_t n_pins;
    bool init;
};

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static mach_uart_obj_t mach_uart_obj[MACH_NUM_UARTS] = { {.uart_reg = &UART0}, {.uart_reg = &UART1}, {.uart_reg = &UART2} };
static const mp_obj_t mach_uart_def_pin[MACH_NUM_UARTS][2] = { { &PIN_MODULE_P1,  &PIN_MODULE_P0 },
                                                               { &PIN_MODULE_P3,  &PIN_MODULE_P4 },
                                                               { &PIN_MODULE_P8,  &PIN_MODULE_P9 } };

static const uint32_t mach_uart_pin_af[MACH_NUM_UARTS][4] = { { U0TXD_OUT_IDX, U0RXD_IN_IDX, U0RTS_OUT_IDX, U0CTS_IN_IDX},
                                                              { U1TXD_OUT_IDX, U1RXD_IN_IDX, U1RTS_OUT_IDX, U1CTS_IN_IDX},
                                                              { U2TXD_OUT_IDX, U2RXD_IN_IDX, U2RTS_OUT_IDX, U2CTS_IN_IDX} };

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void uart_init0 (void) {

}

void uart_deinit_all (void) {
    uint32_t num_uarts = MACH_NUM_UARTS;
#if defined(GPY) || defined(FIPY)
    num_uarts -= 1;
#endif
    for (int i = 0; i < num_uarts; i++) {
        mach_uart_deinit(&mach_uart_obj[i]);
    }
}

uint32_t uart_rx_any(mach_uart_obj_t *self) {
    size_t len = 0;
    uart_get_buffered_data_len(self->uart_id, &len);
    return len;
}

int uart_rx_char(mach_uart_obj_t *self) {
    uint8_t rx_byte;
    int32_t len = uart_read_bytes(self->uart_id, &rx_byte, 1, 0);
    if (len > 0) {
        return rx_byte;
    }
    return -1;
}

bool uart_tx_char(mach_uart_obj_t *self, int c) {
    uint32_t timeout = 0;
    char chr = c;
    while (!uart_tx_fifo_space(self)) {
        if (timeout > (MACHUART_TX_MAX_TIMEOUT_MS * 1000)) {
            return false;
        }
        ets_delay_us(MACHUART_TX_WAIT_US(self->config.baud_rate));
        timeout += MACHUART_TX_WAIT_US(self->config.baud_rate);
    }
    uart_tx_chars(self->uart_id, &chr, 1);
    return true;
}

bool uart_tx_strn(mach_uart_obj_t *self, const char *str, uint len) {
    bool ret = true;
    uint32_t isrmask = 0;
    pin_obj_t * pin = (pin_obj_t *)((mp_obj_t *)self->pins)[0];

    if (self->n_pins == 1) {
        // make it UART Tx
        pin->value = 1;
        pin_deassign(pin);
        pin->mode = GPIO_MODE_OUTPUT;
        pin->af_out = mach_uart_pin_af[self->uart_id][0];
        gpio_matrix_out(pin->pin_number, pin->af_out, false, false);
        // clear the Tx FIFO empty flag
        self->uart_reg->int_clr.tx_done = 1;

        isrmask = MICROPY_BEGIN_ATOMIC_SECTION();
    }

    for (const char *top = str + len; str < top; str++) {
        if (!uart_tx_char(self, *str)) {
            ret = false;
            break;
        }
    }

    if (self->n_pins == 1) {
        // wait for the TX FIFO to be empty
        while (!self->uart_reg->int_raw.tx_done) {
            ets_delay_us(1);
        }

        // make it an input again
        pin_deassign(pin);
        pin->af_in = mach_uart_pin_af[self->uart_id][1];
        pin_config(pin, pin->af_in, -1, GPIO_MODE_INPUT, MACHPIN_PULL_UP, 1);
        MICROPY_END_ATOMIC_SECTION(isrmask);
        self->uart_reg->int_clr.tx_done = 1;
    }

    return ret;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool uart_tx_fifo_space (mach_uart_obj_t *self) {
    uint32_t tx_fifo_cnt = (self->uart_reg->mem_cnt_status.tx_cnt << 8) + self->uart_reg->status.txfifo_cnt;
    if (tx_fifo_cnt < MACHUART_TX_FIFO_LEN) {
        return true;
    }
    return false;
}

static void uart_deassign_pins_af (mach_uart_obj_t *self) {
    for (int i = 0; i < self->n_pins; i++) {
        if (self->pins[i]) {
            // We must set the value to 1 so that when Rx pins are deassigned, their are hardwired to 1
            self->pins[i]->value = 1;
            pin_deassign(self->pins[i]);
            gpio_pullup_dis(self->pins[i]->pin_number);
            self->pins[i] = MP_OBJ_NULL;
        }
    }
    self->n_pins = 0;
}

static void uart_assign_pins_af (mach_uart_obj_t *self, mp_obj_t *pins, uint32_t n_pins) {
    if (n_pins > 1) {
        for (int i = 0; i < n_pins; i++) {
            if (pins[i] != mp_const_none) {
                pin_obj_t *pin = pin_find(pins[i]);
                int32_t af_in, af_out, mode, pull;
                if (i % 2) {
                    af_in = mach_uart_pin_af[self->uart_id][i];
                    af_out = -1;
                    mode = GPIO_MODE_INPUT;
                    pull = MACHPIN_PULL_UP;
                } else {
                    af_in = -1;
                    af_out = mach_uart_pin_af[self->uart_id][i];
                    mode = GPIO_MODE_OUTPUT;
                    pull = MACHPIN_PULL_UP;
                }
                pin_config(pin, af_in, af_out, mode, pull, 1);
                self->pins[i] = pin;
            }
        }
    } else {
        pin_obj_t *pin = pin_find(pins[0]);
        // make the pin Rx by default
        pin_config(pin, mach_uart_pin_af[self->uart_id][1], -1, GPIO_MODE_INPUT, MACHPIN_PULL_UP, 1);
        self->pins[0] = pin;
    }
    self->n_pins = n_pins;
}

// waits at most timeout microseconds for at least 1 char to become ready for
// reading (from buf or for direct reading).
// returns true if something available, false if not.
STATIC bool uart_rx_wait (mach_uart_obj_t *self) {
    int timeout = MACHUART_RX_TIMEOUT_US(self->config.baud_rate, self->rx_timeout);
    for ( ; ; ) {
        if (uart_rx_any(self)) {
            return true; // we have at least 1 char ready for reading
        }
        if (timeout > 0) {
            ets_delay_us(1);
            timeout--;
        } else {
            return false;
        }
    }
}

STATIC void mach_uart_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    if (self->config.baud_rate > 0) {
        mp_printf(print, "UART(%u, baudrate=%u, bits=", self->uart_id, self->config.baud_rate);
        switch (self->config.data_bits) {
        case UART_DATA_5_BITS:
            mp_print_str(print, "5");
            break;
        case UART_DATA_6_BITS:
            mp_print_str(print, "6");
            break;
        case UART_DATA_7_BITS:
            mp_print_str(print, "7");
            break;
        case UART_DATA_8_BITS:
            mp_print_str(print, "8");
            break;
        default:
            break;
        }
        if (self->config.parity == UART_PARITY_DISABLE) {
            mp_print_str(print, ", parity=None");
        } else {
            mp_printf(print, ", parity=UART.%q", (self->config.parity == UART_PARITY_EVEN) ? MP_QSTR_EVEN : MP_QSTR_ODD);
        }
        if (self->config.stop_bits == UART_STOP_BITS_1_5) {
            mp_printf(print, ", stop=1.5)");
        } else {
            mp_printf(print, ", stop=%u)", (self->config.stop_bits == UART_STOP_BITS_1) ? 1 : 2);
        }
    } else {
        mp_printf(print, "UART(%u)", self->uart_id);
    }
}

STATIC IRAM_ATTR void UARTRxCallback(int uart_id, int rx_byte) {
    if (MP_STATE_PORT(mp_os_stream_o) && MP_STATE_PORT(mp_os_stream_o) == &mach_uart_obj[uart_id]) {
        if (mp_interrupt_char == rx_byte) {
            // raise an exception when interrupts are finished
            mp_keyboard_interrupt();
        } else if (mp_reset_char == rx_byte) {
            servers_reset_and_safe_boot();
        }
    }
    // Trigger SIGTERM signal if needed
    if (mp_interrupt_char == rx_byte) {
        // raise an exception when interrupts are finished
        mp_hal_trig_term_sig();
    }
}

STATIC mp_obj_t mach_uart_init_helper(mach_uart_obj_t *self, const mp_arg_val_t *args) {
    // get the baudrate
    if (args[0].u_int <= 0) {
        goto error;
    }

    uint32_t baudrate = args[0].u_int;
    uint32_t data_bits;
    switch (args[1].u_int) {
    case 5:
        data_bits = UART_DATA_5_BITS;
        break;
    case 6:
        data_bits = UART_DATA_6_BITS;
        break;
    case 7:
        data_bits = UART_DATA_7_BITS;
        break;
    case 8:
        data_bits = UART_DATA_8_BITS;
        break;
    default:
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid bits size %d", args[1].u_int));
        break;
    }

    // parity
    uint32_t parity;
    if (args[2].u_obj == mp_const_none) {
        parity = UART_PARITY_DISABLE;
    } else {
        parity = mp_obj_get_int(args[2].u_obj);
        if (parity != UART_PARITY_ODD && parity != UART_PARITY_EVEN) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid parity %d", parity));
        }
    }

    // stop bits
    uint32_t stop_bits;
    float _stop;
    if (args[3].u_obj == mp_const_none) {
        _stop = 1.0;
    } else {
        _stop = mp_obj_get_float(args[3].u_obj);
    }
    if (_stop >= 1.5 && _stop < 2.0) {
        stop_bits = UART_STOP_BITS_1_5;
    } else {
        switch ((int)_stop) {
        case 1:
            stop_bits = UART_STOP_BITS_1;
            break;
        case 2:
            stop_bits = UART_STOP_BITS_2;
            break;
        default:
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid stop bits %f", _stop));
            break;
        }
    }

    // Get the size of the RX buffer
    int rx_buffer_size = args[6].u_int;
    // Check is needed because return value of uart_driver_install is not checked
    if(!(rx_buffer_size > UART_FIFO_LEN)) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid RX buffer size, should be > 128 bytes"));
    }

    if (self->config.baud_rate > 0) {
        // uninstall the driver
        uart_driver_delete(self->uart_id);
    }

    // de-assign the pins
    uart_deassign_pins_af (self);

    // assign the pins
    mp_obj_t pins_o = args[4].u_obj;
    uint32_t flowcontrol = UART_HW_FLOWCTRL_DISABLE;
    if (pins_o != mp_const_none) {
        mp_obj_t *pins;
        mp_uint_t n_pins = 2;
        if (pins_o == MP_OBJ_NULL) {
            // use the default pins
            pins = (mp_obj_t *)mach_uart_def_pin[self->uart_id];
        } else {
            mp_obj_get_array(pins_o, &n_pins, &pins);
            if (n_pins != 1 && n_pins != 2 && n_pins != 4) {
                goto error;
            }
            if (n_pins == 4) {
                if (pins[PIN_TYPE_UART_RTS] != mp_const_none && pins[PIN_TYPE_UART_RXD] == mp_const_none) {
                    goto error;  // RTS pin given in TX only mode
                } else if (pins[PIN_TYPE_UART_CTS] != mp_const_none && pins[PIN_TYPE_UART_TXD] == mp_const_none) {
                    goto error;  // CTS pin given in RX only mode
                } else {
                    if (pins[PIN_TYPE_UART_RTS] != mp_const_none) {
                        flowcontrol |= UART_HW_FLOWCTRL_RTS;
                    }
                    if (pins[PIN_TYPE_UART_CTS] != mp_const_none) {
                        flowcontrol |= UART_HW_FLOWCTRL_CTS;
                    }
                }
            }
        }
        uart_assign_pins_af (self, pins, n_pins);
    }

    self->rx_timeout = args[5].u_int;

    self->base.type = &mach_uart_type;
    self->config.baud_rate = baudrate;
    self->config.data_bits = data_bits;
    self->config.parity = parity;
    self->config.stop_bits = stop_bits;
    self->config.flow_ctrl = flowcontrol;
    self->config.rx_flow_ctrl_thresh = 64;
    uart_param_config(self->uart_id, &self->config);

    // install the UART driver
    uart_driver_install(self->uart_id, rx_buffer_size, 0, 0, NULL, 0, UARTRxCallback);

    // disable the delay between transfers
    self->uart_reg->idle_conf.tx_idle_num = 0;

    // configure the rx timeout threshold
    self->uart_reg->conf1.rx_tout_thrhd = self->rx_timeout & UART_RX_TOUT_THRHD_V;

    // Init Done
    self->init = true;

    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t mach_uart_init_args[] = {
    { MP_QSTR_id,                              MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_baudrate,                        MP_ARG_INT,  {.u_int = 9600} },
    { MP_QSTR_bits,                            MP_ARG_INT,  {.u_int = 8} },
    { MP_QSTR_parity,                          MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_stop,                            MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_pins,           MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_timeout_chars,  MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 2} },
    { MP_QSTR_rx_buffer_size, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = MACHUART_RX_BUFFER_LEN} }
};
STATIC mp_obj_t mach_uart_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_uart_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_uart_init_args, args);

    // work out the uart id
    uint uart_id = mp_obj_get_int(args[0].u_obj);

#if defined(GPY) || defined(FIPY)
    if (uart_id > MACH_UART_1) {
#else
    if (uart_id > MACH_UART_2) {
#endif
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // get the correct uart instance
    mach_uart_obj_t *self = &mach_uart_obj[uart_id];
    self->base.type = &mach_uart_type;
    self->uart_id = uart_id;

    // start the peripheral
    mach_uart_init_helper(self, &args[1]);

    return self;
}

STATIC mp_obj_t mach_uart_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_uart_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_uart_init_args[1], args);
    return mach_uart_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_uart_init_obj, 1, mach_uart_init);

STATIC mp_obj_t mach_uart_deinit(mp_obj_t self_in) {
    mach_uart_obj_t *self = self_in;

    if (self->config.baud_rate > 0) {
        // invalidate the baudrate
        self->config.baud_rate = 0;
        // detach the pins
        uart_deassign_pins_af(self);
        // uninstall the driver
        uart_driver_delete(self->uart_id);
    }

    self->init = false;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_uart_deinit_obj, mach_uart_deinit);

STATIC mp_obj_t mach_uart_any(mp_obj_t self_in) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    return mp_obj_new_int(uart_rx_any(self));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_uart_any_obj, mach_uart_any);

STATIC mp_obj_t mach_uart_wait_tx_done(mp_obj_t self_in, mp_obj_t timeout_ms) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    TickType_t timeout_ticks = mp_obj_get_int_truncated(timeout_ms) / portTICK_PERIOD_MS;
    return uart_wait_tx_done(self->uart_id, timeout_ticks) == ESP_OK ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mach_uart_wait_tx_done_obj, mach_uart_wait_tx_done);

STATIC mp_obj_t mach_uart_sendbreak(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mach_uart_sendbreak_args[] = {
        { MP_QSTR_bits,                            MP_ARG_INT,  {.u_int = 13} }
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mach_uart_sendbreak_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_uart_sendbreak_args[0], args);

    mach_uart_obj_t *self = pos_args[0];
    mp_int_t bits = args[0].u_int;

    MACH_UART_CHECK_INIT(self)
    pin_obj_t * pin = (pin_obj_t *)((mp_obj_t *)self->pins)[0];

    uint32_t isrmask = MICROPY_BEGIN_ATOMIC_SECTION();

    // only if the bus is initialized
    if (self->config.baud_rate > 0) {
        uint32_t delay = (((bits + 1) * 1000000) / self->config.baud_rate) & 0x7FFF;

        if (self->n_pins == 1) {
            // make it UART Tx
            pin->value = 1;
            pin_deassign(pin);
            pin->mode = GPIO_MODE_OUTPUT;
            pin->af_out = mach_uart_pin_af[self->uart_id][0];
            gpio_matrix_out(pin->pin_number, pin->af_out, false, false);
        }

        WRITE_PERI_REG(UART_CONF0_REG(self->uart_id), READ_PERI_REG(UART_CONF0_REG(self->uart_id)) | UART_TXD_INV);
        ets_delay_us((delay > 0) ? delay : 1);
        WRITE_PERI_REG(UART_CONF0_REG(self->uart_id), READ_PERI_REG(UART_CONF0_REG(self->uart_id)) & (~UART_TXD_INV));

        if (self->n_pins == 1) {
            // make it an input again
            pin_deassign(pin);
            pin->af_in = mach_uart_pin_af[self->uart_id][1];
            pin_config(pin, pin->af_in, -1, GPIO_MODE_INPUT, MACHPIN_PULL_UP, 1);
        }
    }

    MICROPY_END_ATOMIC_SECTION(isrmask);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_uart_sendbreak_obj, 1, mach_uart_sendbreak);

STATIC const mp_map_elem_t mach_uart_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),            (mp_obj_t)&mach_uart_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),          (mp_obj_t)&mach_uart_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_any),             (mp_obj_t)&mach_uart_any_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wait_tx_done),    (mp_obj_t)&mach_uart_wait_tx_done_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendbreak),       (mp_obj_t)&mach_uart_sendbreak_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_irq),         (mp_obj_t)&pyb_uart_irq_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_read),            (mp_obj_t)&mp_stream_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline),        (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readinto),        (mp_obj_t)&mp_stream_readinto_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),           (mp_obj_t)&mp_stream_write_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVEN),            MP_OBJ_NEW_SMALL_INT(UART_PARITY_EVEN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ODD),             MP_OBJ_NEW_SMALL_INT(UART_PARITY_ODD) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_RX_ANY),      MP_OBJ_NEW_SMALL_INT(2) },
};
STATIC MP_DEFINE_CONST_DICT(mach_uart_locals_dict, mach_uart_locals_dict_table);

STATIC mp_uint_t mach_uart_read(mp_obj_t self_in, void *buf_in, mp_uint_t size, int *errcode) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    byte *buf = buf_in;

    // make sure we want at least 1 char
    if (size == 0) {
        return 0;
    }

    // wait for first char to become available
    if (!uart_rx_wait(self)) {
        // return EAGAIN error to indicate non-blocking (then read() method returns None)
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    // read the data
    byte *orig_buf = buf;
    for ( ; ; ) {
        *buf++ = uart_rx_char(self);
        if (--size == 0 || !uart_rx_wait(self)) {
            // return number of bytes read
            return buf - orig_buf;
        }
    }
}

STATIC mp_uint_t mach_uart_write(mp_obj_t self_in, const void *buf_in, mp_uint_t size, int *errcode) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    const char *buf = buf_in;

    // write the data
    if (!uart_tx_strn(self, buf, size)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return size;
}

STATIC mp_uint_t mach_uart_ioctl(mp_obj_t self_in, mp_uint_t request, mp_uint_t arg, int *errcode) {
    mach_uart_obj_t *self = self_in;
    MACH_UART_CHECK_INIT(self)
    mp_uint_t ret;

    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        if ((flags & MP_STREAM_POLL_RD) && uart_rx_any(self)) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && uart_tx_fifo_space(self)) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else {
        *errcode = EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

STATIC const mp_stream_p_t uart_stream_p = {
    .read = mach_uart_read,
    .write = mach_uart_write,
    .ioctl = mach_uart_ioctl,
    .is_text = false,
};

//STATIC const mp_irq_methods_t uart_irq_methods = {
//    .init = pyb_uart_irq,
//    .enable = uart_irq_enable,
//    .disable = uart_irq_disable,
//    .flags = uart_irq_flags
//};

const mp_obj_type_t mach_uart_type = {
    { &mp_type_type },
    .name = MP_QSTR_UART,
    .print = mach_uart_print,
    .make_new = mach_uart_make_new,
    .getiter = mp_identity_getiter,
    .iternext = mp_stream_unbuffered_iter,
    .protocol = &uart_stream_p,
    .locals_dict = (mp_obj_t)&mach_uart_locals_dict,
};
