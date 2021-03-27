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
#include "py/mperrno.h"
#include "bufhelper.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "esp_types.h"
#include "esp_attr.h"
#include "gpio.h"

#include "machpin.h"
#include "pins.h"
#include "mpexception.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "machcan.h"
#include "mpexception.h"
#include "machpin.h"
#include "mpirq.h"
#include "CAN.h"
#include "CAN_config.h"

/******************************************************************************
 DECLARE EXPORTED DATA
 ******************************************************************************/
CAN_device_t CAN_cfg = { .rx_queue = NULL };

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MACH_CAN_FORMAT_STD                         (1)
#define MACH_CAN_FORMAT_EXT                         (2)
#define MACH_CAN_FORMAT_BOTH                        (MACH_CAN_FORMAT_STD | MACH_CAN_FORMAT_EXT)

#define MACH_CAN_DEF_RX_QUEUE_LEN                   (128)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
    mp_obj_t handler;
    mp_obj_t handler_arg;
    uint32_t baudrate;
    uint32_t rx_queue_len;
    pin_obj_t *tx;
    pin_obj_t *rx;
    CAN_sw_filters_t swfilters;
    CAN_hw_filters_t hwfilters;
    uint8_t mode;
    uint8_t frame_format;
    uint8_t trigger;
    uint8_t events;
} mach_can_obj_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC mach_can_obj_t mach_can_obj = { .baudrate = 0 };
STATIC const mp_obj_t mach_can_def_pin[2] = {&PIN_MODULE_P22, &PIN_MODULE_P23};

STATIC void can_callback_handler(void *arg);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void can_queue_interrupt(uint32_t events) {
    mach_can_obj.events |= events;
    if (events & mach_can_obj.trigger) {
        mp_irq_queue_interrupt(can_callback_handler, (void *)&mach_can_obj);
    }
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC void can_deassign_pins_af (mach_can_obj_t *self) {
    if (self->baudrate > 0 && self->tx && self->rx) {
        // we must set the value to 1 so that when Rx pins are deassigned, their are hardwired to 1
        self->tx->value = 1;
        self->rx->value = 1;
        pin_deassign(self->tx);
        pin_deassign(self->rx);
        self->tx = MP_OBJ_NULL;
        self->rx = MP_OBJ_NULL;
    }
}

STATIC void can_callback_handler(void *arg) {
    mach_can_obj_t *self = arg;
    if (self->handler && self->handler != mp_const_none) {
        mp_call_function_1(self->handler, self->handler_arg);
    }
}

/******************************************************************************/
/* Micro Python bindings : CAN object                                         */

STATIC void mach_can_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mach_can_obj_t *self = self_in;
    if (self->baudrate > 0) {
        qstr frame_format = MP_QSTR_FORMAT_BOTH;
        switch (self->frame_format) {
            case MACH_CAN_FORMAT_STD:
                frame_format = MP_QSTR_FORMAT_STD;
                break;
            case MACH_CAN_FORMAT_EXT:
                frame_format = MP_QSTR_FORMAT_EXT;
                break;
            default:
                break;
        }
        mp_printf(print, "CAN(0, mode=%d, baudrate=%d, frame_format=%q, rx_queue_len=%d)",
                  self->mode, self->baudrate, frame_format, self->rx_queue_len);
    } else {
        mp_printf(print, "CAN(0)");
    }
}

STATIC mp_obj_t mach_can_init_helper(mach_can_obj_t *self, const mp_arg_val_t *args) {
    uint32_t mode = args[0].u_int; 
    // verify that the mode is correct
    if (mode != CAN_mode_normal && mode != CAN_mode_listen_only) {
        goto invalid_args;
    }
    self->mode = mode;

    // get the baudrate
    uint32_t speed = args[1].u_int / 1000;
    if (speed > 0 && speed <= CAN_SPEED_1000KBPS) {
        self->baudrate = args[1].u_int;
        CAN_cfg.speed = speed;
    } else {
        goto invalid_args;
    }

    // get the identifier mode
    uint32_t frame_format = args[2].u_int;
    if (frame_format > MACH_CAN_FORMAT_BOTH) {
        goto invalid_args;
    }
    self->frame_format = frame_format;

    // before assigning the baudrate
    can_deassign_pins_af(self);

    // assign the pins
    mp_obj_t pins_o = args[3].u_obj;
    if (pins_o != mp_const_none) {
        mp_obj_t *pins;
        if (pins_o == MP_OBJ_NULL) {
            // use the default pins
            pins = (mp_obj_t *)mach_can_def_pin;
        } else {
            mp_obj_get_array_fixed_n(pins_o, 2, &pins);
        }
        self->tx = pin_find(pins[0]);
        self->rx = pin_find(pins[1]);
    } else {
        goto invalid_args;
    }

    CAN_cfg.tx_pin_id = self->tx->pin_number;
    CAN_cfg.rx_pin_id = self->rx->pin_number;

    if (args[4].u_int > 0) {
        self->rx_queue_len = args[4].u_int;
        if (CAN_cfg.rx_queue) {
            vQueueDelete(CAN_cfg.rx_queue);
            CAN_cfg.rx_queue = NULL;
        }
        // create the CAN RX Queue
        CAN_cfg.rx_queue = xQueueCreate(args[4].u_int, sizeof(CAN_frame_t));
        if (!CAN_cfg.rx_queue) {
            mp_raise_OSError(MP_ENOMEM);
        }
    } else {
        goto invalid_args;
    }

    // start the CAN Module
    CAN_init(mode, frame_format - 1);

    // remove the software filters
    self->swfilters.num_filters = 0;
    CAN_setup_sw_filters(&self->swfilters);

    // set the af values, so that deassign works later on
    if (self->tx && self->rx) {
        self->tx->af_out = CAN_TX_IDX;
        self->tx->af_in = -1;
        self->rx->af_out = -1;
        self->rx->af_in = CAN_RX_IDX;
    }

    return mp_const_none;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t mach_can_init_args[] = {
    { MP_QSTR_bus,                               MP_ARG_INT,        {.u_int = 0} },
    { MP_QSTR_mode,                              MP_ARG_INT,        {.u_int = CAN_mode_normal} },
    { MP_QSTR_baudrate,                          MP_ARG_INT,        {.u_int = CAN_SPEED_500KBPS} },
    { MP_QSTR_frame_format,     MP_ARG_KW_ONLY | MP_ARG_INT,        {.u_int = MACH_CAN_FORMAT_STD} },
    { MP_QSTR_pins,             MP_ARG_KW_ONLY | MP_ARG_OBJ,        {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_rx_queue_len,     MP_ARG_KW_ONLY | MP_ARG_INT,        {.u_int = MACH_CAN_DEF_RX_QUEUE_LEN} },
};

STATIC mp_obj_t mach_can_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_can_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_can_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // setup the object
    mach_can_obj_t *self = &mach_can_obj;
    self->base.type = &mach_can_type;

    // start the peripheral
    mach_can_init_helper(self, &args[1]);

    return self;
}

STATIC mp_obj_t mach_can_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_can_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_can_init_args[1], args);
    mach_can_init_helper(pos_args[0], args);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_can_init_obj, 1, mach_can_init);

STATIC mp_obj_t mach_can_deinit(mp_obj_t self_in) {
    mach_can_obj_t *self = self_in;

    // stop the can controller
    CAN_stop();

    // de-assign the pins
    can_deassign_pins_af(self);

    if (CAN_cfg.rx_queue) {
        vQueueDelete(CAN_cfg.rx_queue);
        CAN_cfg.rx_queue = NULL;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_can_deinit_obj, mach_can_deinit);

STATIC mp_obj_t mach_can_send(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,           MP_ARG_REQUIRED | MP_ARG_INT,  },
        { MP_QSTR_data,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_rtr,          MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_extended,     MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    CAN_frame_t tx_frame;

    // get the buffer to send from
    mp_buffer_info_t bufinfo;
    uint8_t data[1];
    if (args[1].u_obj == mp_const_none) {
        bufinfo.len = 0;
    } else {
        pyb_buf_get_for_send(args[1].u_obj, &bufinfo, data);
    }

    uint32_t msg_id = args[0].u_int;
    if (msg_id > 2047 && !args[3].u_bool) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid message id %d", msg_id));
    }

    if (args[2].u_bool) {
        if (bufinfo.len > 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "data given for RTR message"));
        }
    } else if (bufinfo.len == 0 || bufinfo.len > 8) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid data length %d", bufinfo.len));
    }

    tx_frame.FIR.B.DLC = bufinfo.len;
    tx_frame.FIR.B.RTR = args[2].u_bool ? CAN_RTR : CAN_no_RTR;
    tx_frame.FIR.B.FF = args[3].u_bool ? CAN_frame_ext : CAN_frame_std;
    tx_frame.MsgID = msg_id;
    memcpy(tx_frame.data.u8, bufinfo.buf, bufinfo.len);

    CAN_write_frame(&tx_frame);

    // return the number of bytes sent
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_can_send_obj, 1, mach_can_send);

STATIC mp_obj_t mach_can_recv(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_timeout,      MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };

    static const qstr can_recv_info_fields[] = {
        MP_QSTR_id, MP_QSTR_data, MP_QSTR_rtr, MP_QSTR_extended
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint64_t timeout = 0;
    if (args[0].u_obj == mp_const_none) {
        timeout = portMAX_DELAY;
    } else {
        if (args[0].u_obj != MP_OBJ_NULL) {
            timeout = mp_obj_get_float(args[0].u_obj) * 1000;
            if (timeout < 0 || timeout > portMAX_DELAY) {
                timeout = portMAX_DELAY;
            }
        }
    }

    CAN_frame_t rx_frame;
    MP_THREAD_GIL_EXIT();
    if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, (uint32_t)timeout * portTICK_PERIOD_MS) == pdTRUE) {
        MP_THREAD_GIL_ENTER();
        mp_obj_t tuple[4];
        tuple[0] = mp_obj_new_int(rx_frame.MsgID);
        if (rx_frame.FIR.B.RTR == CAN_RTR) {
            tuple[1] = mp_const_empty_bytes;
            tuple[2] = mp_const_true;
        } else {
            tuple[1] = mp_obj_new_bytes((const byte *)rx_frame.data.u8, rx_frame.FIR.B.DLC);
            tuple[2] = mp_const_false;
        }
        tuple[3] = rx_frame.FIR.B.FF ? mp_const_true : mp_const_false;

        // return the attribute tuple
        return mp_obj_new_attrtuple(can_recv_info_fields, 4, tuple);
    }
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_can_recv_obj, 1, mach_can_recv);

STATIC mp_obj_t mach_can_soft_filter(mp_obj_t self_in, mp_obj_t mode_o, mp_obj_t filters_l) {
    mach_can_obj_t *self = self_in;

    uint32_t mode = mp_obj_get_int(mode_o);

    if (mode > CAN_FILTER_MASK) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid filter mode"));
    }

    self->swfilters.mode = mode;

    mp_obj_t *filters;
    mp_uint_t n_filters;
    if (filters_l != mp_const_none) {
        mp_obj_get_array(filters_l, &n_filters, &filters);
        if (n_filters < 1 || n_filters > 32) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "between 1 and 32 software filters are possible"));
        }
        for (int i = 0; i < n_filters; i++) {
            if (mode == CAN_FILTER_LIST) {
                self->swfilters.fromto[i][0] = mp_obj_get_int(filters[i]);
                self->swfilters.fromto[i][1] = mp_obj_get_int(filters[i]);
            } else {
                mp_obj_t *fromto;
                mp_obj_get_array_fixed_n(filters[i], 2, &fromto);
                // in case of filter/mask mode, first index is the filter, the second is the mask
                self->swfilters.fromto[i][0] = mp_obj_get_int(fromto[0]);
                self->swfilters.fromto[i][1] = mp_obj_get_int(fromto[1]);
            }
        }
        self->swfilters.num_filters = n_filters;
    } else {
        self->swfilters.num_filters = 0;
    }

    CAN_setup_sw_filters(&self->swfilters);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mach_can_soft_filter_obj, mach_can_soft_filter);

// STATIC mp_obj_t mach_can_hard_filter(mp_obj_t self_in, mp_obj_t filters_l) {
//     mach_can_obj_t *self = self_in;

//     mp_obj_t *filters;
//     mp_uint_t n_filters;
//     if (filters_l != mp_const_none) {
//         mp_obj_get_array(filters_l, &n_filters, &filters);
//         if (n_filters < 1 || n_filters > 2) {
//             nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "between 1 and 2 hardware filters are possible"));
//         }
//         for (int i = 0; i < n_filters; i++) {
//             mp_obj_t *fromto;
//             mp_obj_get_array_fixed_n(filters[i], 2, &fromto);
//             self->hwfilters.codemask[i][0] = mp_obj_get_int_truncated(fromto[0]);
//             self->hwfilters.codemask[i][1] = mp_obj_get_int_truncated(fromto[1]);
//         }
//         self->hwfilters.num_filters = n_filters;
//     } else {
//         self->hwfilters.num_filters = 0;
//     }

//     CAN_setup_hw_filters(&self->hwfilters);

//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_2(mach_can_hard_filter_obj, mach_can_hard_filter);

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t mach_can_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    mach_can_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        self->handler = args[1].u_obj;
        mp_irq_add(self, args[1].u_obj);
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    } else {
        self->trigger = 0;
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_can_callback_obj, 1, mach_can_callback);

STATIC mp_obj_t mach_can_events(mp_obj_t self_in) {
    mach_can_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_can_events_obj, mach_can_events);

STATIC const mp_map_elem_t mach_can_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&mach_can_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&mach_can_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),                (mp_obj_t)&mach_can_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recv),                (mp_obj_t)&mach_can_recv_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_soft_filter),         (mp_obj_t)&mach_can_soft_filter_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_hard_filter),         (mp_obj_t)&mach_can_hard_filter_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),            (mp_obj_t)&mach_can_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),              (mp_obj_t)&mach_can_events_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_NORMAL),              MP_OBJ_NEW_SMALL_INT(CAN_mode_normal) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SILENT),              MP_OBJ_NEW_SMALL_INT(CAN_mode_listen_only) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_FORMAT_STD),          MP_OBJ_NEW_SMALL_INT(MACH_CAN_FORMAT_STD) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FORMAT_EXT),          MP_OBJ_NEW_SMALL_INT(MACH_CAN_FORMAT_EXT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FORMAT_BOTH),         MP_OBJ_NEW_SMALL_INT(MACH_CAN_FORMAT_BOTH) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_RX_FRAME),            MP_OBJ_NEW_SMALL_INT(CAN_RX_FRAME_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RX_FIFO_NOT_EMPTY),   MP_OBJ_NEW_SMALL_INT(CAN_FIFO_NOT_EMPTY_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RX_FIFO_OVERRUN),     MP_OBJ_NEW_SMALL_INT(CAN_RX_FIFO_OVERRUN_EVENT) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_LIST),         MP_OBJ_NEW_SMALL_INT(CAN_FILTER_LIST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_RANGE),        MP_OBJ_NEW_SMALL_INT(CAN_FILTER_RANGE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_MASK),         MP_OBJ_NEW_SMALL_INT(CAN_FILTER_MASK) },
};

STATIC MP_DEFINE_CONST_DICT(mach_can_locals_dict, mach_can_locals_dict_table);

const mp_obj_type_t mach_can_type = {
    { &mp_type_type },
    .name = MP_QSTR_CAN,
    .print = mach_can_print,
    .make_new = mach_can_make_new,
    .locals_dict = (mp_obj_t)&mach_can_locals_dict,
};
