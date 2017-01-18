/*
 * Copyright (c) 2016, Pycom Limited.
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
#include "pybioctl.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"

/// \moduleref machine
/// \class UART - duplex serial communication bus

/******************************************************************************
 DEFINE CONSTANTS
 *******-***********************************************************************/
#define MACHUART_FRAME_TIME_US(baud)            ((11 * 1000000) / baud)
#define MACHUART_2_FRAMES_TIME_US(baud)         (MACHUART_FRAME_TIME_US(baud) * 2)
#define MACHUART_RX_TIMEOUT_US(baud)            (MACHUART_2_FRAMES_TIME_US(baud) * 8) // we need at least characters in the FIFO

#define MACHUART_TX_WAIT_US(baud)               ((MACHUART_FRAME_TIME_US(baud)) + 1)
#define MACHUART_TX_MAX_TIMEOUT_MS              (5)

#define MACHUART_RX_BUFFER_LEN                  (256)
#define MACHUART_TX_FIFO_LEN                    (126)

// interrupt triggers
#define UART_TRIGGER_RX_ANY                     (0x01)
#define UART_TRIGGER_RX_HALF                    (0x02)
#define UART_TRIGGER_RX_FULL                    (0x04)
#define UART_TRIGGER_TX_DONE                    (0x08)

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool uart_tx_fifo_space (mach_uart_obj_t *self);
static void uart_intr_handler(void *para);

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
struct _mach_uart_obj_t {
    mp_obj_base_t base;
    volatile byte *read_buf;            // read buffer pointer
    uart_config_t config;
    uart_intr_config_t intr_config;
    volatile uint32_t read_buf_head;    // indexes the first empty slot
    volatile uint32_t read_buf_tail;    // indexes the first full slot (not full if equals head)
    uint8_t irq_flags;
    uint8_t uart_id;
};

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static mach_uart_obj_t mach_uart_obj[MACH_NUM_UARTS];
static const mp_obj_t mach_uart_def_pin[MACH_NUM_UARTS][2] = { {&PIN_MODULE_P1,  &PIN_MODULE_P0},
                                                               {&PIN_MODULE_P3,  &PIN_MODULE_P4},
                                                               {&PIN_MODULE_P8,  &PIN_MODULE_P9} };

static const uint32_t mach_uart_pin_af[MACH_NUM_UARTS][4] = { { U0TXD_OUT_IDX, U0RXD_IN_IDX, U0RTS_OUT_IDX, U0CTS_IN_IDX},
                                                              { U1TXD_OUT_IDX, U1RXD_IN_IDX, U1RTS_OUT_IDX, U1CTS_IN_IDX},
                                                              { U2TXD_OUT_IDX, U2RXD_IN_IDX, U2RTS_OUT_IDX, U2CTS_IN_IDX} };

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void uart_init0 (void) {
}

uint32_t uart_rx_any(mach_uart_obj_t *self) {
    if (self->read_buf_tail != self->read_buf_head) {
        // buffering via irq
        return (self->read_buf_head > self->read_buf_tail) ? self->read_buf_head - self->read_buf_tail :
                MACHUART_RX_BUFFER_LEN - self->read_buf_tail + self->read_buf_head;
    }
    return 0;
}

int uart_rx_char(mach_uart_obj_t *self) {
    // disable UART Rx interrupts
    CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(self->uart_id), UART_RXFIFO_TOUT_INT_ENA | UART_RXFIFO_FULL_INT_ENA);
    if (self->read_buf_tail != self->read_buf_head) {
        // buffering via irq
        int data = self->read_buf[self->read_buf_tail];
        self->read_buf_tail = (self->read_buf_tail + 1) % MACHUART_RX_BUFFER_LEN;
        // enable interrupts back
        SET_PERI_REG_MASK(UART_INT_ENA_REG(self->uart_id), UART_RXFIFO_TOUT_INT_ENA | UART_RXFIFO_FULL_INT_ENA);
        return data;
    }
    // enable interrupts back
    SET_PERI_REG_MASK(UART_INT_ENA_REG(self->uart_id), UART_RXFIFO_TOUT_INT_ENA | UART_RXFIFO_FULL_INT_ENA);
    return -1;
}

bool uart_tx_char(mach_uart_obj_t *self, int c) {
    uint32_t timeout = 0;
    while (!uart_tx_fifo_space(self)) {
        if (timeout++ > ((MACHUART_TX_MAX_TIMEOUT_MS * 1000) / MACHUART_TX_WAIT_US(self->config.baud_rate))) {
            return false;
        }
        ets_delay_us(MACHUART_TX_WAIT_US(self->config.baud_rate));
    }
    WRITE_PERI_REG(UART_FIFO_AHB_REG(self->uart_id), c);
    return true;
}

bool uart_tx_strn(mach_uart_obj_t *self, const char *str, uint len) {
    for (const char *top = str + len; str < top; str++) {
        if (!uart_tx_char(self, *str)) {
            return false;
        }
    }
    return true;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool uart_tx_fifo_space (mach_uart_obj_t *self) {
    uint32_t tx_fifo_cnt = READ_PERI_REG(UART_STATUS_REG(self->uart_id)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
    if ((tx_fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < MACHUART_TX_FIFO_LEN) {
        return true;
    }
    return false;
}

static void uart_assign_pins_af (mp_obj_t *pins, uint32_t n_pins, uint32_t uart_id) {
    for (int i = 0; i < n_pins; i++) {
        if (pins[i] != mp_const_none) {
            pin_obj_t *pin = pin_find(pins[i]);
            int32_t af_in, af_out, mode;
            if (i % 2) {
                af_in = mach_uart_pin_af[uart_id][i];
                af_out = -1;
                mode = GPIO_MODE_INPUT;
            } else {
                af_in = -1;
                af_out = mach_uart_pin_af[uart_id][i];
                mode = GPIO_MODE_OUTPUT;
            }
            pin_config(pin, af_in, af_out, mode, MACHPIN_PULL_UP, 1);
        }
    }
}

STATIC IRAM_ATTR void UARTGenericIntHandler(uint32_t uart_id, uint32_t status) {
    mach_uart_obj_t *self = &mach_uart_obj[uart_id];

    while (status) {
        if (UART_FRM_ERR_INT_ST == (status & UART_FRM_ERR_INT_ST)) {
            // frame error
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_id), UART_FRM_ERR_INT_CLR);
        } else if (UART_RXFIFO_FULL_INT_ST == (status & UART_RXFIFO_FULL_INT_ST) ||
                   UART_RXFIFO_TOUT_INT_ST == (status & UART_RXFIFO_TOUT_INT_ST)) {
            // Rx data present
            while (READ_PERI_REG(UART_STATUS_REG(uart_id)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
                int data = READ_PERI_REG(UART_FIFO_AHB_REG(uart_id)) & 0xFF;
                if (MP_STATE_PORT(mp_os_stream_o) && MP_STATE_PORT(mp_os_stream_o) == self && data == mp_interrupt_char) {
                    // raise an exception when interrupts are finished
                    mp_keyboard_interrupt();
                } else { // there's always a read buffer available
                    uint16_t next_head = (self->read_buf_head + 1) % MACHUART_RX_BUFFER_LEN;
                    if (next_head != self->read_buf_tail) {
                        // only store data if there's room in the buffer
                        self->read_buf[self->read_buf_head] = data;
                        self->read_buf_head = next_head;
                    }
                }
            }
            // clear the interrupt
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_id), UART_RXFIFO_TOUT_INT_CLR | UART_RXFIFO_FULL_INT_ST);
        } else if (UART_TXFIFO_EMPTY_INT_ST == (status & UART_TXFIFO_EMPTY_INT_ST)) {
            // Tx FIFO empty
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_id), UART_TXFIFO_EMPTY_INT_CLR);
            // this one needs to be re-enabled every time we send
            CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(uart_id), UART_TXFIFO_EMPTY_INT_ENA);
        } else if (UART_RXFIFO_OVF_INT_ST == (status & UART_RXFIFO_OVF_INT_ST)) {
            WRITE_PERI_REG(UART_INT_CLR_REG(uart_id), UART_RXFIFO_OVF_INT_CLR);
        }
        status = READ_PERI_REG(UART_INT_ST_REG(uart_id)) ;
    }

//    // check the flags to see if the user handler should be called
//    if ((self->irq_trigger & self->irq_flags) && self->irq_enabled) {
//        // call the user defined handler
//        mp_irq_handler(mp_irq_find(self));
//    }

    // clear the flags
    self->irq_flags = 0;
}

static IRAM_ATTR void uart_intr_handler(void *para) {
    uint32_t status;

    if ((status = READ_PERI_REG(UART_INT_ST_REG(UART_NUM_0)))) {
        UARTGenericIntHandler(UART_NUM_0, status);
    }
    if ((status = READ_PERI_REG(UART_INT_ST_REG(UART_NUM_1)))) {
        UARTGenericIntHandler(UART_NUM_1, status);
    }
    if ((status = READ_PERI_REG(UART_INT_ST_REG(UART_NUM_2)))) {
        UARTGenericIntHandler(UART_NUM_2, status);
    }
}


// waits at most timeout microseconds for at least 1 char to become ready for
// reading (from buf or for direct reading).
// returns true if something available, false if not.
STATIC bool uart_rx_wait (mach_uart_obj_t *self) {
    int timeout = MACHUART_RX_TIMEOUT_US(self->config.baud_rate);
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

esp_err_t machuart_isr_register(uart_port_t uart_num, void (*fn)(void*), void * arg)
{
    int32_t source;
    switch(uart_num) {
    case 1:
        source = ETS_UART1_INTR_SOURCE;
        break;
    case 2:
        source = ETS_UART2_INTR_SOURCE;
        break;
    case 0:
        default:
        source = ETS_UART0_INTR_SOURCE;
        break;
    }
    return esp_intr_alloc(source, 0, fn, arg, NULL);
}

STATIC mp_obj_t mach_uart_init_helper(mach_uart_obj_t *self, const mp_arg_val_t *args) {
    uint32_t uart_id = self->uart_id;

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
        uint parity = mp_obj_get_int(args[2].u_obj);
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
    if (_stop == 1.5) {
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

    // register it with the sleep module
    // pyb_sleep_add ((const mp_obj_t)self, (WakeUpCB_t)uart_init);
    // enable the callback
    // uart_irq_new (self, UART_TRIGGER_RX_ANY, INT_PRIORITY_LVL_3, mp_const_none);
    // disable the irq (from the user point of view)
    // uart_irq_disable(self);

    uint32_t intr_mask = UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA |
                         UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA;

    // disable interrupts on the current UART before re-configuring
    // UART_IntrConfig() will enable them again
    CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(uart_id), UART_INTR_MASK);

    // uart_reset_fifo(uart_id);

    self->uart_id = uart_id;
    self->base.type = &mach_uart_type;
    self->config.baud_rate = baudrate;
    self->config.data_bits = data_bits;
    self->config.parity = parity;
    self->config.stop_bits = stop_bits;
    self->config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    self->config.rx_flow_ctrl_thresh = 64;
    uart_param_config(uart_id, &self->config);

    // re-allocate the read buffer after resetting the uart
    self->read_buf_head = 0;
    self->read_buf_tail = 0;
    self->read_buf = MP_OBJ_NULL; // free the read buffer before allocating again
    MP_STATE_PORT(uart_buf[uart_id]) = m_new(byte, MACHUART_RX_BUFFER_LEN);
    self->read_buf = (volatile byte *)MP_STATE_PORT(uart_buf[uart_id]);

    // interrupts are enabled here
    machuart_isr_register(uart_id, uart_intr_handler, NULL);

    self->intr_config.intr_enable_mask = intr_mask;
    self->intr_config.rx_timeout_thresh = 10;
    self->intr_config.txfifo_empty_intr_thresh = 2;
    self->intr_config.rxfifo_full_thresh = 20;
    uart_intr_config(uart_id, &self->intr_config);

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
            if (n_pins != 2 && n_pins != 4) {
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
        uart_assign_pins_af (pins, n_pins, self->uart_id);
    }

    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t mach_uart_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_baudrate,                       MP_ARG_INT,  {.u_int = 9600} },
    { MP_QSTR_bits,                           MP_ARG_INT,  {.u_int = 8} },
    { MP_QSTR_parity,                         MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_stop,                           MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_pins,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t mach_uart_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_uart_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_uart_init_args, args);

    // work out the uart id
    uint uart_id = mp_obj_get_int(args[0].u_obj);

    if (uart_id > MACH_UART_2) {
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

    // invalidate the baudrate
    self->config.baud_rate = 0;
    // free the read buffer
    m_del(byte, (void *)self->read_buf, MACHUART_RX_BUFFER_LEN);
    // disable the interrupt
    CLEAR_PERI_REG_MASK(UART_INT_ENA_REG(self->uart_id), UART_INTR_MASK);
    // enable the clock to the peripheral
    if (self->uart_id == MACH_UART_0) {
        SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART_RST);
        CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART_CLK_EN);
    } else if (self->uart_id == MACH_UART_1) {
        SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART1_RST);
        CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART1_CLK_EN);
    } else {
        SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);
        CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_uart_deinit_obj, mach_uart_deinit);

STATIC mp_obj_t mach_uart_any(mp_obj_t self_in) {
    mach_uart_obj_t *self = self_in;
    return mp_obj_new_int(uart_rx_any(self));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_uart_any_obj, mach_uart_any);

STATIC const mp_map_elem_t mach_uart_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),        (mp_obj_t)&mach_uart_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),      (mp_obj_t)&mach_uart_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_any),         (mp_obj_t)&mach_uart_any_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_sendbreak),   (mp_obj_t)&pyb_uart_sendbreak_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_irq),         (mp_obj_t)&pyb_uart_irq_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_read),        (mp_obj_t)&mp_stream_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readall),     (mp_obj_t)&mp_stream_readall_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline),    (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readinto),    (mp_obj_t)&mp_stream_readinto_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),       (mp_obj_t)&mp_stream_write_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVEN),        MP_OBJ_NEW_SMALL_INT(UART_PARITY_EVEN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ODD),         MP_OBJ_NEW_SMALL_INT(UART_PARITY_ODD) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_RX_ANY),      MP_OBJ_NEW_SMALL_INT(2) },
};
STATIC MP_DEFINE_CONST_DICT(mach_uart_locals_dict, mach_uart_locals_dict_table);

STATIC mp_uint_t mach_uart_read(mp_obj_t self_in, void *buf_in, mp_uint_t size, int *errcode) {
    mach_uart_obj_t *self = self_in;
    byte *buf = buf_in;

    // make sure we want at least 1 char
    if (size == 0) {
        return 0;
    }

    // wait for first char to become available
    if (!uart_rx_wait(self)) {
        // return EAGAIN error to indicate non-blocking (then read() method returns None)
        *errcode = EAGAIN;
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
    const char *buf = buf_in;

    // write the data
    if (!uart_tx_strn(self, buf, size)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return size;
}

STATIC mp_uint_t mach_uart_ioctl(mp_obj_t self_in, mp_uint_t request, mp_uint_t arg, int *errcode) {
    mach_uart_obj_t *self = self_in;
    mp_uint_t ret;

    if (request == MP_IOCTL_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        if ((flags & MP_IOCTL_POLL_RD) && uart_rx_any(self)) {
            ret |= MP_IOCTL_POLL_RD;
        }
        if ((flags & MP_IOCTL_POLL_WR) && uart_tx_fifo_space(self)) {
            ret |= MP_IOCTL_POLL_WR;
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
    .getiter = mp_identity,
    .iternext = mp_stream_unbuffered_iter,
    .protocol = &uart_stream_p,
    .locals_dict = (mp_obj_t)&mach_uart_locals_dict,
};
