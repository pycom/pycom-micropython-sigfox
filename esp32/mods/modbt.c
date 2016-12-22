/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "controller.h"

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "bufhelper.h"
#include "mpexception.h"
#include "modnetwork.h"
#include "pybioctl.h"
#include "modusocket.h"
#include "pycom_config.h"
#include "modbt.h"

#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "btm_api.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define BT_SCAN_QUEUE_SIZE_MAX                              8

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t         base;
    int32_t               scan_duration;
    int32_t               conn_id;
    bool                  init;
    bool                  busy;
} bt_obj_t;

typedef enum {
    E_BT_STACK_MODE_BLE = 0,
    E_BT_STACK_MODE_BT
} bt_mode_t;

typedef union {
    esp_ble_gap_cb_param_t scan;
    esp_gatt_srvc_id_t service;
} bt_event_result_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;
static QueueHandle_t xScanQueue;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void flush_event_queue(void);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modbt_init0(void) {
    if (!xScanQueue) {
        xScanQueue = xQueueCreate(BT_SCAN_QUEUE_SIZE_MAX, sizeof(bt_event_result_t));
    } else {
        flush_event_queue();
    }
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

esp_gatt_if_t client_if;
esp_gatt_status_t status = ESP_GATT_ERROR;
uint16_t simpleClient_id = 0xEE;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = ESP_PUBLIC_ADDR,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

static void flush_event_queue(void) {
    bt_event_result_t bt_event;
    // empty the queue
    while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)0));
}

static void esp_gap_cb(uint32_t event, void *param);

static void esp_gattc_cb(uint32_t event, void *param);

static void esp_gap_cb(uint32_t event, void *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        int32_t duration = bt_obj.scan_duration;
        // the unit of the duration is seconds
        // printf("Start scanning\n");
        if (duration < 0) {
            duration = 0xFFFF;
        }
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            xQueueSend(xScanQueue, (void *)scan_result, (TickType_t)0);
            break;
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            // printf("Discovery result\n");
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            if (bt_obj.scan_duration < 0) {
                esp_ble_gap_set_scan_params(&ble_scan_params);
            } else {
                // printf("Scan finished\n");
                bt_obj.busy = false;
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}


static void esp_gattc_cb(uint32_t event, void *param)
{
    uint16_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    LOG_INFO("esp_gattc_cb, event = %x\n", event);
    switch (event) {
    case ESP_GATTC_REG_EVT:
        status = p_data->reg.status;
        client_if = p_data->reg.gatt_if;
        // printf("status = %x, client_if = %x\n", status, client_if);
        break;
    case ESP_GATTC_OPEN_EVT:
        conn_id = p_data->open.conn_id;
        bt_obj.conn_id = conn_id;
        // printf("ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %x\n", conn_id, p_data->open.gatt_if, p_data->open.status);
        if (p_data->open.status == ESP_GATT_OK) {
            // printf("Device connected\n");
        } else {
            // printf("Connection failed! \n");
            bt_obj.conn_id = -1;
        }
        bt_obj.busy = false;
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
        xQueueSend(xScanQueue, (void *)srvc_id, (TickType_t)0);
        break;
    }
    case ESP_GATTC_GET_CHAR_EVT:
        if (p_data->get_char.status == ESP_GATT_OK) {
            esp_ble_gattc_get_characteristic(bt_obj.conn_id, &p_data->get_char.srvc_id, &p_data->get_char.char_id);
            // printf("characteristic found=%x\n", p_data->get_char.char_id.uuid.uuid.uuid16);
        }
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        // printf("SEARCH_CMPL: conn_id = %x, status %d\n", conn_id, p_data->search_cmpl.status);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_CANCEL_OPEN_EVT:
    case ESP_GATTC_CLOSE_EVT:
        // printf("Connection closed!\n");
        bt_obj.conn_id = -1;
        bt_obj.busy = false;
        break;
    default:
        break;
    }
}

void ble_client_appRegister(void)
{
    LOG_INFO("register callback\n");

    // register the scan callback function to the gap moudule
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        LOG_ERROR("gap register error, error code = %x\n", status);
        return;
    }

    //register the callback function to the gattc module
    if ((status = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK) {
        LOG_ERROR("gattc register error, error code = %x\n", status);
        return;
    }
    esp_ble_gattc_app_register(simpleClient_id);
}

/******************************************************************************/
// Micro Python bindings; BT class

/// \class Bluetooth
static mp_obj_t bt_init_helper(bt_obj_t *self, const mp_arg_val_t *args) {
    if (!self->init) {
        bt_controller_init();
        esp_init_bluetooth();
        esp_enable_bluetooth();
        ble_client_appRegister();
        self->init = true;
    }
    return mp_const_none;
}

STATIC const mp_arg_t bt_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_BT_STACK_MODE_BLE} },
};
STATIC mp_obj_t bt_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(bt_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), bt_init_args, args);

    // setup the object
    bt_obj_t *self = (bt_obj_t *)&bt_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_bt;

    // give it to the sleep module
    //pyb_sleep_set_wlan_obj(self); // FIXME

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // run the constructor if the peripehral is not initialized or extra parameters are given
    if (n_kw > 0 || !self->init) {
        // start the peripheral
        bt_init_helper(self, &args[1]);
        // register it as a network card
        // mod_network_register_nic(self);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t bt_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(bt_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &bt_init_args[1], args);
    return bt_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_init_obj, 1, bt_init);

STATIC mp_obj_t bt_start_scan(mp_obj_t self_in, mp_obj_t timeout) {
    if (bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    }

    int32_t duration = mp_obj_get_int(timeout);
    if (duration == 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid scan time"));
    }

    bt_obj.scan_duration = duration;
    bt_obj.busy = true;
    flush_event_queue();
    esp_ble_gap_set_scan_params(&ble_scan_params);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_start_scan_obj, bt_start_scan);

STATIC mp_obj_t bt_stop_scan(mp_obj_t self_in) {
    if (bt_obj.busy) {
        esp_ble_gap_stop_scanning();
        bt_obj.busy = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_stop_scan_obj, bt_stop_scan);

STATIC mp_obj_t bt_read_scan(mp_obj_t self_in) {
    bt_event_result_t bt_event;

    STATIC const qstr bt_scan_info_fields[] = {
        MP_QSTR_mac, MP_QSTR_addr_type, MP_QSTR_adv_type, MP_QSTR_rssi, MP_QSTR_data,
    };

    if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)0)) {
        mp_obj_t tuple[5];
        tuple[0] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.bda, 6);
        tuple[1] = mp_obj_new_int(bt_event.scan.scan_rst.ble_addr_type);
        tuple[2] = mp_obj_new_int(bt_event.scan.scan_rst.ble_evt_type & 0x03);    // Why?
        tuple[3] = mp_obj_new_int(bt_event.scan.scan_rst.rssi);
        tuple[4] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.ble_adv, ESP_BLE_ADV_DATA_LEN_MAX);

        return mp_obj_new_attrtuple(bt_scan_info_fields, 5, tuple);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_read_scan_obj, bt_read_scan);

STATIC mp_obj_t bt_resolve_adv_data(mp_obj_t self_in, mp_obj_t adv_data, mp_obj_t data_type) {
    mp_buffer_info_t bufinfo;
    uint8_t data_len;
    uint8_t *data;

    uint8_t type = mp_obj_get_int(data_type);
    mp_get_buffer_raise(adv_data, &bufinfo, MP_BUFFER_READ);
    data = esp_ble_resolve_adv_data(bufinfo.buf, type, &data_len);

    if (data) {
        switch(type) {
            case ESP_BLE_AD_TYPE_FLAG:
                return mp_obj_new_int(*(int8_t *)data);
            case ESP_BLE_AD_TYPE_16SRV_PART:
            case ESP_BLE_AD_TYPE_16SRV_CMPL:
            case ESP_BLE_AD_TYPE_32SRV_PART:
            case ESP_BLE_AD_TYPE_32SRV_CMPL:
            case ESP_BLE_AD_TYPE_128SRV_PART:
            case ESP_BLE_AD_TYPE_128SRV_CMPL:
                return mp_obj_new_bytes(data, data_len);
            case ESP_BLE_AD_TYPE_NAME_SHORT:
            case ESP_BLE_AD_TYPE_NAME_CMPL:
                return mp_obj_new_str((char *)data, data_len, false);
            case ESP_BLE_AD_TYPE_TX_PWR:
                return mp_obj_new_int(*(int8_t *)data);
            case ESP_BLE_AD_TYPE_DEV_CLASS:
                break;
            case ESP_BLE_AD_TYPE_SERVICE_DATA:
                return mp_obj_new_bytes(data, data_len);
            case ESP_BLE_AD_TYPE_APPEARANCE:
                break;
            case ESP_BLE_AD_TYPE_ADV_INT:
                break;
            case ESP_BLE_AD_TYPE_32SERVICE_DATA:
                break;
            case ESP_BLE_AD_TYPE_128SERVICE_DATA:
                break;
            case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE:
                return mp_obj_new_bytes(data, data_len);
            default:
                break;
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(bt_resolve_adv_data_obj, bt_resolve_adv_data);

STATIC mp_obj_t bt_connect(mp_obj_t self_in, mp_obj_t addr) {
    if (bt_obj.busy) {
        esp_ble_gap_stop_scanning();
        bt_obj.busy = false;
    }

    if (bt_obj.conn_id > 0) {
        esp_ble_gattc_close(bt_obj.conn_id);
        vTaskDelay(150);
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(addr, &bufinfo, MP_BUFFER_READ);
    esp_ble_gattc_open(client_if, bufinfo.buf, true);
    bt_obj.busy = true;

    while (bt_obj.busy) {
        vTaskDelay(50);
    }

    if (bt_obj.conn_id < 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection refused"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_connect_obj, bt_connect);

STATIC mp_obj_t bt_isconnected(mp_obj_t self_in) {
    if (bt_obj.conn_id > 0) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_isconnected_obj, bt_isconnected);

STATIC mp_obj_t bt_disconnect(mp_obj_t self_in) {
    if (bt_obj.conn_id > 0) {
        esp_ble_gattc_close(bt_obj.conn_id);
        bt_obj.conn_id = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_disconnect_obj, bt_disconnect);

STATIC mp_obj_t bt_get_services (mp_obj_t self_in) {
    bt_event_result_t bt_event;

    if (bt_obj.conn_id > 0) {
        flush_event_queue();
        bt_obj.busy = true;
        esp_ble_gattc_search_service(bt_obj.conn_id, NULL);
        mp_obj_t services = mp_obj_new_list(0, NULL);
        while (bt_obj.busy || xQueuePeek(xScanQueue, &bt_event, 0)) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)50)) {
                mp_obj_t srv;
                if (bt_event.service.id.uuid.len == ESP_UUID_LEN_16) {
                    srv = mp_obj_new_int(bt_event.service.id.uuid.uuid.uuid16);
                } else if (bt_event.service.id.uuid.len == ESP_UUID_LEN_32) {
                    srv = mp_obj_new_int(bt_event.service.id.uuid.uuid.uuid32);
                } else {
                    srv = mp_obj_new_bytes(bt_event.service.id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
                }
                mp_obj_list_append(services, srv);
            }
        }
        return services;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "no device connected"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_get_services_obj, bt_get_services);

STATIC mp_obj_t bt_get_characteristics (mp_obj_t self_in, mp_obj_t service) {
    // bt_event_result_t bt_event;
    esp_gatt_srvc_id_t srvc_id;

    if (MP_OBJ_IS_STR_OR_BYTES(service)) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(service, &bufinfo, MP_BUFFER_READ);
        srvc_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(srvc_id.id.uuid.uuid.uuid128, bufinfo.buf, ESP_UUID_LEN_128);
        esp_ble_gattc_get_characteristic(bt_obj.conn_id, &srvc_id, NULL);
    } else {
        uint32_t id = mp_obj_get_int(service);
        if (id > 0xFFFF) {
            srvc_id.id.uuid.len = ESP_UUID_LEN_32;
            srvc_id.id.uuid.uuid.uuid32 = id;
        } else {
            srvc_id.id.uuid.len = ESP_UUID_LEN_16;
            srvc_id.id.uuid.uuid.uuid16 = id;
        }
        esp_ble_gattc_get_characteristic(bt_obj.conn_id, &srvc_id, NULL);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_get_characteristics_obj, bt_get_characteristics);



STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start_scan),              (mp_obj_t)&bt_start_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop_scan),               (mp_obj_t)&bt_stop_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_adv),                 (mp_obj_t)&bt_read_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_resolve_adv_data),        (mp_obj_t)&bt_resolve_adv_data_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),                 (mp_obj_t)&bt_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),             (mp_obj_t)&bt_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),              (mp_obj_t)&bt_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_services),            (mp_obj_t)&bt_get_services_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_get_characteristics),     (mp_obj_t)&bt_get_characteristics_obj },

    // constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_CONN_ADV),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_CONN_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CONN_DIR_ADV),            MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_CONN_DIR_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_DISC_ADV),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_DISC_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_NON_CONN_ADV),            MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_NON_CONN_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SCAN_RSP),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_SCAN_RSP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PUBLIC_ADDR),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RANDOM_ADDR),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RANDOM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PUBLIC_RPA_ADDR),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RANDOM_RPA_ADDR),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_RANDOM) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FLAG),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_FLAG) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_16SRV_PART),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_16SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_T16SRV_CMPL),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_16SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SRV_PART),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SRV_CMPL),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SRV_PART),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SRV_CMPL),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_NAME_SHORT),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_NAME_SHORT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_NAME_CMPL),           MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_NAME_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TX_PWR),              MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_TX_PWR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_DEV_CLASS),           MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_DEV_CLASS) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_SERVICE_DATA),        MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_APPEARANCE),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_APPEARANCE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_ADV_INT),             MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_ADV_INT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SERVICE_DATA),      MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SERVICE_DATA),     MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_MANUFACTURER_DATA),   MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE) },

};
STATIC MP_DEFINE_CONST_DICT(bt_locals_dict, bt_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_bt = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_BT,
        .make_new = bt_make_new,
        .locals_dict = (mp_obj_t)&bt_locals_dict,
     },
};

///******************************************************************************/
//// Micro Python bindings; BT socket

