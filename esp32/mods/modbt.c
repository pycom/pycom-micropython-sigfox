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
#define BT_SCAN_QUEUE_SIZE_MAX                              (8)
#define BT_CHAR_VALUE_SIZE_MAX                              (20)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t         base;
    int32_t               scan_duration;
    int32_t               conn_id;          // current activity connection id
    mp_obj_list_t         conn_list;
    bool                  init;
    bool                  busy;
    bool                  scanning;
} bt_obj_t;

typedef struct {
    mp_obj_base_t         base;
    int32_t               conn_id;
    mp_obj_list_t         srv_list;
} bt_connection_obj_t;

typedef struct {
    mp_obj_base_t         base;
    bt_connection_obj_t   *connection;
    esp_gatt_srvc_id_t    srv_id;
    mp_obj_list_t         char_list;
} bt_srv_obj_t;

typedef struct {
    esp_gatt_id_t           char_id;
    esp_gatt_char_prop_t    char_prop;
} bt_char_t;

typedef struct {
    mp_obj_base_t           base;
    bt_srv_obj_t            *service;
    bt_char_t               characteristic;
    // mp_obj_list_t         desc_list;
} bt_char_obj_t;

typedef struct {
    uint8_t     value[BT_CHAR_VALUE_SIZE_MAX];
    uint16_t    value_type;
    uint16_t    value_len;
} bt_read_value_t;

typedef struct {
    esp_gatt_status_t status;
} bt_write_value_t;

typedef enum {
    E_BT_STACK_MODE_BLE = 0,
    E_BT_STACK_MODE_BT
} bt_mode_t;

typedef union {
    esp_ble_gap_cb_param_t  scan;
    esp_gatt_srvc_id_t      service;
    bt_char_t               characteristic;
    bt_read_value_t         read;
    bt_write_value_t        write;
    int32_t                 conn_id;
} bt_event_result_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;
static QueueHandle_t xScanQueue;

static const mp_obj_type_t mod_bt_connection_type;
static const mp_obj_type_t mod_bt_service_type;
static const mp_obj_type_t mod_bt_characteristic_type;
// static const mp_obj_type_t mod_bt_descriptor_type;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void esp_gap_cb(uint32_t event, void *param);
static void esp_gattc_cb(uint32_t event, void *param);
static void flush_event_queue(void);
static void close_connection(int32_t conn_id);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modbt_init0(void) {
    if (!xScanQueue) {
        xScanQueue = xQueueCreate(BT_SCAN_QUEUE_SIZE_MAX, sizeof(bt_event_result_t));
    } else {
        flush_event_queue();
    }
    mp_obj_list_init((void *)&bt_obj.conn_list, 0);
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

static void close_connection (int32_t conn_id) {
    for (mp_uint_t i = 0; i < bt_obj.conn_list.len; i++) {
        bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(bt_obj.conn_list.items[i]));
        if (connection_obj->conn_id == conn_id) {
            connection_obj->conn_id = -1;
            mp_obj_list_remove((void *)&bt_obj.conn_list, connection_obj);
        }
    }
}

static void esp_gap_cb(uint32_t event, void *param) {
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
        bt_event_result_t bt_event_result;
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        memcpy(&bt_event_result.scan, scan_result, sizeof(esp_ble_gap_cb_param_t));
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            break;
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            // printf("Discovery result\n");
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            if (bt_obj.scan_duration < 0) {
                esp_ble_gap_set_scan_params(&ble_scan_params);
            } else {
                // printf("Scan finished\n");
                bt_obj.scanning = false;
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

static void esp_gattc_cb(uint32_t event, void *param) {
    uint16_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    bt_event_result_t bt_event_result;

    LOG_INFO("esp_gattc_cb, event = %x\n", event);
    switch (event) {
    case ESP_GATTC_REG_EVT:
        status = p_data->reg.status;
        client_if = p_data->reg.gatt_if;
        // printf("status = %x, client_if = %x\n", status, client_if);
        break;
    case ESP_GATTC_OPEN_EVT:
        conn_id = p_data->open.conn_id;
        printf("ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %x\n", conn_id, p_data->open.gatt_if, p_data->open.status);
        if (p_data->open.status == ESP_GATT_OK) {
            printf("Device connected=%d\n", conn_id);
            bt_event_result.conn_id = conn_id;
        } else {
            printf("Connection failed!=%d\n", conn_id);
            bt_event_result.conn_id = -1;
        }
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        if (p_data->read.status == ESP_GATT_OK) {
            memcpy(&bt_event_result.read.value, p_data->read.value, p_data->read.value_len);
            bt_event_result.read.value_len = p_data->read.value_len;
            bt_event_result.read.value_type = p_data->read.value_type;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        } else {
            printf("Error reading BLE characteristic\n");
        }
        bt_obj.busy = false;
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        bt_event_result.write.status = p_data->write.status;
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
        memcpy(&bt_event_result.service, srvc_id, sizeof(esp_gatt_srvc_id_t));
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    }
    case ESP_GATTC_GET_CHAR_EVT:
        if (p_data->get_char.status == ESP_GATT_OK) {
            esp_gatt_id_t *char_id = &p_data->get_char.char_id;
            memcpy(&bt_event_result.characteristic.char_id, char_id, sizeof(esp_gatt_id_t));
            bt_event_result.characteristic.char_prop = p_data->get_char.char_prop;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            esp_ble_gattc_get_characteristic(bt_obj.conn_id, &p_data->get_char.srvc_id, char_id);
            printf("characteristic found=%x\n", p_data->get_char.char_id.uuid.uuid.uuid16);
        } else {
            printf("Error getting BLE characteristic\n");
            bt_obj.busy = false;
        }
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        // printf("SEARCH_CMPL: conn_id = %x, status %d\n", conn_id, p_data->search_cmpl.status);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_CANCEL_OPEN_EVT:
        bt_obj.busy = false;
        // intentional fall through
    case ESP_GATTC_CLOSE_EVT:
        // printf("Connection closed!\n");
        bt_obj.conn_id = -1;
        close_connection(p_data->close.conn_id);
        break;
    default:
        break;
    }
}

static void ble_client_appRegister(void) {
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
    if (bt_obj.scanning || bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    }

    int32_t duration = mp_obj_get_int(timeout);
    if (duration == 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid scan time"));
    }

    bt_obj.scan_duration = duration;
    bt_obj.scanning = true;
    flush_event_queue();
    if (ESP_OK != esp_ble_gap_set_scan_params(&ble_scan_params)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_start_scan_obj, bt_start_scan);

STATIC mp_obj_t bt_isscanning(mp_obj_t self_in) {
    if (bt_obj.scanning) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_isscanning_obj, bt_isscanning);

STATIC mp_obj_t bt_stop_scan(mp_obj_t self_in) {
    if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        bt_obj.scanning = false;
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
    bt_event_result_t bt_event;

    if (bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    } else if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        bt_obj.scanning = false;
        printf("Scanning stopped\n");
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(addr, &bufinfo, MP_BUFFER_READ);

    flush_event_queue();
    bt_obj.busy = true;
    if (ESP_OK != esp_ble_gattc_open(client_if, bufinfo.buf, true)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }

    while (bt_obj.busy) {
        vTaskDelay(5 / portTICK_RATE_MS);
    }

    if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
        if (bt_event.conn_id < 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection refused"));
        }

        // setup the object
        bt_connection_obj_t *conn = m_new_obj(bt_connection_obj_t);
        conn->base.type = (mp_obj_t)&mod_bt_connection_type;
        conn->conn_id = bt_event.conn_id;
        printf("conn id=%d\n", bt_event.conn_id);
        mp_obj_list_append((void *)&bt_obj.conn_list, conn);
        return conn;
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection failed"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_connect_obj, bt_connect);

STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start_scan),              (mp_obj_t)&bt_start_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isscanning),              (mp_obj_t)&bt_isscanning_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop_scan),               (mp_obj_t)&bt_stop_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_adv),                 (mp_obj_t)&bt_read_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_resolve_adv_data),        (mp_obj_t)&bt_resolve_adv_data_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),                 (mp_obj_t)&bt_connect_obj },

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

    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_BROADCAST),          MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_BROADCAST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_READ),               MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_READ) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_WRITE_NR),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_WRITE_NR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_WRITE),              MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_WRITE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_NOTIFY),             MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_NOTIFY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_INDICATE),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_INDICATE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_AUTH),               MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_AUTH) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_EXT_PROP),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_EXT_PROP) },
};
STATIC MP_DEFINE_CONST_DICT(bt_locals_dict, bt_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_bt = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_Bluetooth,
        .make_new = bt_make_new,
        .locals_dict = (mp_obj_t)&bt_locals_dict,
     },
};

STATIC mp_obj_t bt_conn_isconnected(mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;

    if (self->conn_id > 0) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_isconnected_obj, bt_conn_isconnected);

STATIC mp_obj_t bt_conn_disconnect(mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;

    if (self->conn_id > 0) {
        esp_ble_gattc_close(self->conn_id);
        self->conn_id = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_disconnect_obj, bt_conn_disconnect);

STATIC mp_obj_t bt_conn_services (mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->conn_id > 0) {
        flush_event_queue();
        bt_obj.busy = true;
        bt_obj.conn_id = self->conn_id;
        mp_obj_list_init(&self->srv_list, 0);
        if (ESP_OK != esp_ble_gattc_search_service(self->conn_id, NULL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        while (bt_obj.busy || xQueuePeek(xScanQueue, &bt_event, 0)) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
                bt_srv_obj_t *srv = m_new_obj(bt_srv_obj_t);
                srv->base.type = (mp_obj_t)&mod_bt_service_type;
                srv->connection = self;
                memcpy(&srv->srv_id, &bt_event.service, sizeof(esp_gatt_srvc_id_t));
                mp_obj_list_append(&self->srv_list, srv);
            }
        }
        return &self->srv_list;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_services_obj, bt_conn_services);

STATIC const mp_map_elem_t bt_connection_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),             (mp_obj_t)&bt_conn_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),              (mp_obj_t)&bt_conn_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_services),                (mp_obj_t)&bt_conn_services_obj },

};
STATIC MP_DEFINE_CONST_DICT(bt_connection_locals_dict, bt_connection_locals_dict_table);

static const mp_obj_type_t mod_bt_connection_type = {
     { &mp_type_type },
    .name = MP_QSTR_BluetoothConnection,
    .locals_dict = (mp_obj_t)&bt_connection_locals_dict,
};

STATIC mp_obj_t bt_srv_isprimary(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->srv_id.is_primary) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_isprimary_obj, bt_srv_isprimary);

STATIC mp_obj_t bt_srv_uuid(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->srv_id.id.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->srv_id.id.uuid.uuid.uuid16);
    } else if (self->srv_id.id.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->srv_id.id.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->srv_id.id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_uuid_obj, bt_srv_uuid);

STATIC mp_obj_t bt_srv_instance(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;
    return mp_obj_new_int(self->srv_id.id.inst_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_instance_obj, bt_srv_instance);

STATIC mp_obj_t bt_srv_characteristics(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->connection->conn_id > 0) {
        flush_event_queue();
        bt_obj.busy = true;
        bt_obj.conn_id = self->connection->conn_id;
        mp_obj_list_init(&self->char_list, 0);
        if (ESP_OK != esp_ble_gattc_get_characteristic(self->connection->conn_id, &self->srv_id, NULL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        while (bt_obj.busy || xQueuePeek(xScanQueue, &bt_event, 0)) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
                bt_char_obj_t *chr = m_new_obj(bt_char_obj_t);
                chr->base.type = (mp_obj_t)&mod_bt_characteristic_type;
                chr->service = self;
                memcpy(&chr->characteristic.char_id, &bt_event.characteristic.char_id, sizeof(esp_gatt_id_t));
                chr->characteristic.char_prop = bt_event.characteristic.char_prop;
                mp_obj_list_append(&self->char_list, chr);
            }
        }
        return &self->char_list;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_characteristics_obj, bt_srv_characteristics);

STATIC const mp_map_elem_t bt_service_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_isprimary),               (mp_obj_t)&bt_srv_isprimary_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_srv_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_srv_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_characteristics),         (mp_obj_t)&bt_srv_characteristics_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_service_locals_dict, bt_service_locals_dict_table);

static const mp_obj_type_t mod_bt_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_BluetoothService,
    .locals_dict = (mp_obj_t)&bt_service_locals_dict,
};

STATIC mp_obj_t bt_char_uuid(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;

    if (self->characteristic.char_id.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->characteristic.char_id.uuid.uuid.uuid16);
    } else if (self->characteristic.char_id.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->characteristic.char_id.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->characteristic.char_id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_uuid_obj, bt_char_uuid);

STATIC mp_obj_t bt_char_instance(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_int(self->characteristic.char_id.inst_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_instance_obj, bt_char_instance);

STATIC mp_obj_t bt_char_properties(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_int(self->characteristic.char_prop);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_properties_obj, bt_char_properties);

STATIC mp_obj_t bt_char_read(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->service->connection->conn_id > 0) {
        flush_event_queue();
        bt_obj.busy = true;
        bt_obj.conn_id = self->service->connection->conn_id;

        if (ESP_OK != esp_ble_gattc_read_char (self->service->connection->conn_id,
                                               &self->service->srv_id,
                                               &self->characteristic.char_id,
                                               ESP_GATT_AUTH_REQ_NONE)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        while (bt_obj.busy) {
            vTaskDelay(5 / portTICK_RATE_MS);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            return mp_obj_new_bytes(bt_event.read.value, bt_event.read.value_len);
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_read_obj, bt_char_read);

STATIC mp_obj_t bt_char_write(mp_obj_t self_in, mp_obj_t value) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(value, &bufinfo, MP_BUFFER_READ);

    if (self->service->connection->conn_id > 0) {
        flush_event_queue();
        bt_obj.busy = true;
        bt_obj.conn_id = self->service->connection->conn_id;

        if (ESP_OK != esp_ble_gattc_write_char (self->service->connection->conn_id,
                                                &self->service->srv_id,
                                                &self->characteristic.char_id,
                                                bufinfo.len,
                                                bufinfo.buf,
                                                ESP_GATT_WRITE_TYPE_NO_RSP,
                                                ESP_GATT_AUTH_REQ_NONE)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }

        while (bt_obj.busy) {
            vTaskDelay(5 / portTICK_RATE_MS);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            if (bt_event.write.status != ESP_GATT_OK) {
                goto error;
            }
        } else {
            goto error;
        }
    }
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_char_write_obj, bt_char_write);

STATIC const mp_map_elem_t bt_characteristic_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_char_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_char_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_properties),              (mp_obj_t)&bt_char_properties_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                    (mp_obj_t)&bt_char_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),                   (mp_obj_t)&bt_char_write_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_descriptors),             (mp_obj_t)&bt_char_descriptors_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_subscribe),               (mp_obj_t)&bt_char_subscribe_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_characteristic_locals_dict, bt_characteristic_locals_dict_table);

static const mp_obj_type_t mod_bt_characteristic_type = {
    { &mp_type_type },
    .name = MP_QSTR_BluetoothCharacteristic,
    .locals_dict = (mp_obj_t)&bt_characteristic_locals_dict,
};

// static const mp_obj_type_t mod_bt_descriptor_type = {
    // { &mp_type_type },
    // .name = MP_QSTR_BT_DESCRIPTOR,
    // .locals_dict = (mp_obj_t)&bt_descriptor_locals_dict,
// };
