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
#include "mpirq.h"

#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "btm_api.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "util/btdynmem.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define BT_SCAN_QUEUE_SIZE_MAX                              (8)
#define BT_GATTS_QUEUE_SIZE_MAX                             (2)
#define BT_CHAR_VALUE_SIZE_MAX                              (20)

#define MOD_BT_CLIENT_ID                                    (0xEE)
#define MOD_BT_SERVER_APP_ID                                (0x00FF)

#define MOD_BT_GATTC_ADV_EVT                                (0x0001)
#define MOD_BT_GATTS_CONN_EVT                               (0x0002)
#define MOD_BT_GATTS_DISCONN_EVT                            (0x0004)
#define MOD_BT_GATTS_READ_EVT                               (0x0008)
#define MOD_BT_GATTS_WRITE_EVT                              (0x0010)
#define MOD_BT_GATTC_NOTIFY_EVT                             (0x0020)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t         base;
    int32_t               scan_duration;
    int32_t               conn_id;          // current activity connection id
    mp_obj_t              handler;
    mp_obj_t              handler_arg;
    uint32_t              trigger;
    int32_t               events;
    mp_obj_list_t         conn_list;
    mp_obj_list_t         srv_list;
    mp_obj_list_t         char_list;
    uint16_t              gatts_if;
    uint16_t              gatts_conn_id;
    bool                  init;
    bool                  busy;
    bool                  scanning;
    bool                  advertising;
    bool                  controller_active;
} bt_obj_t;

typedef struct {
    mp_obj_base_t         base;
    esp_bd_addr_t         srv_bda;
    int32_t               conn_id;
    mp_obj_list_t         srv_list;
    esp_gatt_if_t         gatt_if;
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
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
    uint32_t                trigger;
    uint32_t                events;
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

typedef struct {
    int32_t         conn_id;
    esp_bd_addr_t   srv_bda;
    esp_gatt_if_t   gatt_if;
} bt_connection_event;

typedef union {
    esp_ble_gap_cb_param_t      scan;
    esp_gatt_srvc_id_t          service;
    bt_char_t                   characteristic;
    bt_read_value_t             read;
    bt_write_value_t            write;
    bt_connection_event         connection;
} bt_event_result_t;

typedef union {
    uint16_t service_handle;
    uint16_t char_handle;
    bool     adv_set;
} bt_gatts_event_result_t;

typedef struct {
    mp_obj_base_t         base;
    uint16_t              handle;
} bt_gatts_srv_obj_t;

typedef struct {
    mp_obj_base_t         base;
    bt_gatts_srv_obj_t    *srv;
    mp_obj_t              handler;
    mp_obj_t              handler_arg;
    uint32_t              trigger;
    int32_t               events;
    uint16_t              handle;
    uint16_t              value_len;
    uint8_t               value[20];
    esp_bt_uuid_t         uuid;
    esp_bt_uuid_t         descr_uuid;
} bt_gatts_char_obj_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;
static QueueHandle_t xScanQueue;
static QueueHandle_t xGattsQueue;

static const mp_obj_type_t mod_bt_connection_type;
static const mp_obj_type_t mod_bt_service_type;
static const mp_obj_type_t mod_bt_characteristic_type;
// static const mp_obj_type_t mod_bt_descriptor_type;

static const mp_obj_type_t mod_bt_gatts_service_type;
static const mp_obj_type_t mod_bt_gatts_char_type;

static esp_ble_adv_params_t bt_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void esp_gap_cb(uint32_t event, void *param);
static void esp_gattc_cb(uint32_t event, void *param);
static void close_connection(int32_t conn_id);

STATIC void bluetooth_callback_handler(void *arg);
STATIC void gattc_char_callback_handler(void *arg);
STATIC void gatts_char_callback_handler(void *arg);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modbt_init0(void) {
    if (!xScanQueue) {
        xScanQueue = xQueueCreate(BT_SCAN_QUEUE_SIZE_MAX, sizeof(bt_event_result_t));
    } else {
        xQueueReset(xScanQueue);
    }
    if (!xGattsQueue) {
        xGattsQueue = xQueueCreate(BT_GATTS_QUEUE_SIZE_MAX, sizeof(bt_gatts_event_result_t));
    } else {
        xQueueReset(xGattsQueue);
    }

    if (bt_obj.init) {
        esp_ble_gattc_app_unregister(MOD_BT_CLIENT_ID);
        esp_ble_gatts_app_unregister(MOD_BT_SERVER_APP_ID);
    }

    mp_obj_list_init((mp_obj_t)&bt_obj.srv_list, 0);
    mp_obj_list_init((mp_obj_t)&bt_obj.char_list, 0);
    mp_obj_list_init((void *)&bt_obj.conn_list, 0);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static esp_gatt_if_t client_if;
static esp_gatt_status_t status = ESP_GATT_ERROR;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = ESP_PUBLIC_ADDR,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

static bool bt_match_id (esp_gatt_id_t *uuid_a, esp_gatt_id_t *uuid_b) {
    if (uuid_a->inst_id == uuid_b->inst_id && uuid_a->uuid.len == uuid_b->uuid.len &&
        !memcmp(&uuid_a->uuid.uuid, &uuid_b->uuid.uuid, uuid_a->uuid.len)) {
        return true;
    }
    return false;
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

static bt_char_obj_t *finds_gattc_char (int32_t conn_id, esp_gatt_srvc_id_t *srvc_id, esp_gatt_id_t *char_id) {
    for (mp_uint_t i = 0; i < bt_obj.conn_list.len; i++) {
        bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(bt_obj.conn_list.items[i]));

        if (connection_obj->conn_id == conn_id) {

            for (mp_uint_t j = 0; j < connection_obj->srv_list.len; j++) {
                bt_srv_obj_t *srv_obj = ((bt_srv_obj_t *)(connection_obj->srv_list.items[j]));

                if (bt_match_id(&srv_obj->srv_id.id, &srvc_id->id)) {

                    for (mp_uint_t j = 0; j < srv_obj->char_list.len; j++) {
                        bt_char_obj_t *char_obj = ((bt_char_obj_t *)(srv_obj->char_list.items[j]));
                        if (bt_match_id(&char_obj->characteristic.char_id, char_id)) {
                            return char_obj;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

static bt_gatts_char_obj_t *finds_gatts_char_by_handle (uint16_t handle) {
    for (mp_uint_t i = 0; i < bt_obj.char_list.len; i++) {
        bt_gatts_char_obj_t *char_obj = ((bt_gatts_char_obj_t *)(bt_obj.char_list.items[i]));
        if (char_obj->handle == handle) {
            return char_obj;
        }
    }
    return NULL;
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
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    {
        bt_gatts_event_result_t gatts_event;
        gatts_event.adv_set = true;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
    }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        bt_event_result_t bt_event_result;
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        memcpy(&bt_event_result.scan, scan_result, sizeof(esp_ble_gap_cb_param_t));
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            bt_obj.events |= MOD_BT_GATTC_ADV_EVT;
            if (bt_obj.trigger & MOD_BT_GATTC_ADV_EVT) {
                mp_irq_queue_interrupt(bluetooth_callback_handler, (void *)&bt_obj);
            }
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
        // printf("ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %x\n", conn_id, p_data->open.gatt_if, p_data->open.status);
        if (p_data->open.status == ESP_GATT_OK) {
            // printf("Device connected=%d\n", conn_id);
            bt_event_result.connection.conn_id = conn_id;
            bt_event_result.connection.gatt_if = p_data->open.gatt_if;
            memcpy(bt_event_result.connection.srv_bda, p_data->open.remote_bda, ESP_BD_ADDR_LEN);
        } else {
            // printf("Connection failed!=%d\n", conn_id);
            bt_event_result.connection.conn_id = -1;
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
            // printf("Error reading BLE characteristic\n");
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
            // printf("characteristic found=%x\n", p_data->get_char.char_id.uuid.uuid.uuid16);
        } else {
            // printf("Error getting BLE characteristic\n");
            bt_obj.busy = false;
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
    {
        bt_char_obj_t *char_obj;
        char_obj = finds_gattc_char (p_data->notify.conn_id, &p_data->notify.srvc_id, &p_data->notify.char_id);
        if (char_obj != NULL) {
            char_obj->events |= MOD_BT_GATTC_NOTIFY_EVT;
            if (char_obj->trigger & MOD_BT_GATTC_NOTIFY_EVT) {
                mp_irq_queue_interrupt(gattc_char_callback_handler, char_obj);
            }
        }
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

// this function will be called by the interrupt thread
STATIC void bluetooth_callback_handler(void *arg) {
    bt_obj_t *self = arg;

    if (self->handler != mp_const_none) {
        mp_call_function_1(self->handler, self->handler_arg);
    }
}

// this function will be called by the interrupt thread
STATIC void gattc_char_callback_handler(void *arg) {
    bt_char_obj_t *chr = arg;

    if (chr->handler != mp_const_none) {
        mp_call_function_1(chr->handler, chr->handler_arg);
    }
}

// this function will be called by the interrupt thread
STATIC void gatts_char_callback_handler(void *arg) {
    bt_gatts_char_obj_t *chr = arg;

    if (chr->handler != mp_const_none) {
        mp_call_function_1(chr->handler, chr->handler_arg);
    }
}

static void gatts_event_handler(uint32_t event, void *param)
{
    esp_ble_gatts_cb_param_t *p = (esp_ble_gatts_cb_param_t *)param;

    switch (event) {
    case ESP_GATTS_REG_EVT:
        bt_obj.gatts_if = p->reg.gatt_if;
        break;
    case ESP_GATTS_READ_EVT: {
        bt_gatts_char_obj_t *char_obj = finds_gatts_char_by_handle (p->read.handle);
        if (char_obj) {
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = p->read.handle;
            rsp.attr_value.len = char_obj->value_len;
            memcpy(&rsp.attr_value.value, char_obj->value, char_obj->value_len);
            esp_ble_gatts_send_response(p->read.conn_id, p->read.trans_id, ESP_GATT_OK, &rsp);

            char_obj->events |= MOD_BT_GATTS_READ_EVT;
            if (char_obj->trigger & MOD_BT_GATTS_READ_EVT) {
                mp_irq_queue_interrupt(gatts_char_callback_handler, char_obj);
            }
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        bt_gatts_char_obj_t *char_obj = finds_gatts_char_by_handle (p->write.handle);
        if (char_obj) {
            if (p->write.len <= sizeof(char_obj->value)) {
                memcpy(char_obj->value, p->write.value, p->write.len);
                char_obj->value_len = p->write.len;
            }
            esp_ble_gatts_send_response(p->write.conn_id, p->write.trans_id, ESP_GATT_OK, NULL);

            char_obj->events |= MOD_BT_GATTS_WRITE_EVT;
            if (char_obj->trigger & MOD_BT_GATTS_WRITE_EVT) {
                mp_irq_queue_interrupt(gatts_char_callback_handler, char_obj);
            }
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
    case ESP_GATTS_MTU_EVT:
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
    {
        bt_gatts_event_result_t gatts_event;
        gatts_event.service_handle = p->create.service_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
    }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
    {
        bt_gatts_event_result_t gatts_event;
        gatts_event.char_handle = p->add_char.attr_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
    }
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        // LOG_INFO("ADD_DESCR_EVT, status %d, gatt_if %d,  attr_handle %d, service_handle %d\n",
        //          p->add_char.status, p->add_char.gatt_if, p->add_char.attr_handle, p->add_char.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        // LOG_INFO("SERVICE_START_EVT, status %d, gatt_if %d, service_handle %d\n",
        //          p->start.status, p->start.gatt_if, p->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
        bt_obj.gatts_conn_id = p->connect.conn_id;

        bt_obj.events |= MOD_BT_GATTS_CONN_EVT;
        if (bt_obj.trigger & MOD_BT_GATTS_CONN_EVT) {
            mp_irq_queue_interrupt(bluetooth_callback_handler, (void *)&bt_obj);
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        if (bt_obj.advertising) {
            esp_ble_gap_start_advertising(&bt_adv_params);
        }
        bt_obj.events |= MOD_BT_GATTS_DISCONN_EVT;
        if (bt_obj.trigger & MOD_BT_GATTS_DISCONN_EVT) {
            mp_irq_queue_interrupt(bluetooth_callback_handler, (void *)&bt_obj);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

/******************************************************************************/
// Micro Python bindings; BT class

/// \class Bluetooth
static mp_obj_t bt_init_helper(bt_obj_t *self, const mp_arg_val_t *args) {
    if (!self->controller_active) {
        bt_controller_init();
        self->controller_active = true;
    }

    if (!self->init) {
        if (0 != bluetooth_alloc_memory()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,"Bluetooth memory allocation failed"));
        }

        if (ESP_OK != esp_init_bluetooth()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,"Bluetooth init failed"));
        }
        if (ESP_OK != esp_enable_bluetooth()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,"Bluetooth enable failed"));
        }

        esp_ble_gap_register_callback(esp_gap_cb);
        esp_ble_gattc_register_callback(esp_gattc_cb);
        esp_ble_gatts_register_callback(gatts_event_handler);

        mp_obj_list_init((mp_obj_t)&bt_obj.conn_list, 0);
        mp_obj_list_init((mp_obj_t)&bt_obj.srv_list, 0);
        mp_obj_list_init((mp_obj_t)&bt_obj.char_list, 0);

        self->init = true;
    }
    esp_ble_gattc_app_register(MOD_BT_CLIENT_ID);
    esp_ble_gatts_app_register(MOD_BT_SERVER_APP_ID);
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

STATIC mp_obj_t bt_deinit(mp_obj_t self_in) {
    if (bt_obj.init) {
        esp_disable_bluetooth();
        esp_deinit_bluetooth();
        bluetooth_free_memory();
        bt_obj.init = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_deinit_obj, bt_deinit);

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
    xQueueReset(xScanQueue);
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
        tuple[2] = mp_obj_new_int(bt_event.scan.scan_rst.ble_evt_type & 0x03);    // FIXME
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

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t bt_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    bt_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        self->handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    } else {  // disable the callback
        self->trigger = 0;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_callback_obj, 1, bt_callback);

STATIC mp_obj_t bt_events(mp_obj_t self_in) {
    bt_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_events_obj, bt_events);

STATIC mp_obj_t bt_connect(mp_obj_t self_in, mp_obj_t addr) {
    bt_event_result_t bt_event;

    if (bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    } else if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        bt_obj.scanning = false;
        // printf("Scanning stopped\n");
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(addr, &bufinfo, MP_BUFFER_READ);

    xQueueReset(xScanQueue);
    bt_obj.busy = true;
    if (ESP_OK != esp_ble_gattc_open(client_if, bufinfo.buf, true)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }

    while (bt_obj.busy) {
        mp_hal_delay_ms(5);
    }

    if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
        if (bt_event.connection.conn_id < 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection refused"));
        }

        // setup the object
        bt_connection_obj_t *conn = m_new_obj(bt_connection_obj_t);
        conn->base.type = (mp_obj_t)&mod_bt_connection_type;
        conn->conn_id = bt_event.connection.conn_id;
        memcpy(conn->srv_bda, bt_event.connection.srv_bda, 6);
        // printf("conn id=%d\n", bt_event.conn_id);
        mp_obj_list_append((void *)&bt_obj.conn_list, conn);
        return conn;
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection failed"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_connect_obj, bt_connect);

STATIC mp_obj_t bt_set_advertisement (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_name,                     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_manufacturer_data,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_data,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_uuid,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    esp_ble_adv_data_t adv_data;
    mp_buffer_info_t manuf_bufinfo;
    mp_buffer_info_t srv_bufinfo;
    mp_buffer_info_t uuid_bufinfo;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // device name
    if (args[0].u_obj != mp_const_none) {
        const char *name = mp_obj_str_get_str(args[0].u_obj);
        adv_data.include_name = true;
        esp_ble_gap_set_device_name(name);
    } else {
        adv_data.include_name = false;
        esp_ble_gap_set_device_name("");
    }

    // manufacturer data
    if (args[1].u_obj != mp_const_none) {
        mp_get_buffer_raise(args[1].u_obj, &manuf_bufinfo, MP_BUFFER_READ);
        adv_data.manufacturer_len = manuf_bufinfo.len;
        adv_data.p_manufacturer_data = manuf_bufinfo.buf;
    } else {
        adv_data.manufacturer_len = 0;
        adv_data.p_manufacturer_data =  NULL;
    }

    // service data
    if (args[2].u_obj != mp_const_none) {
        mp_get_buffer_raise(args[2].u_obj, &srv_bufinfo, MP_BUFFER_READ);
        adv_data.service_data_len = srv_bufinfo.len;
        adv_data.p_service_data = srv_bufinfo.buf;
    } else {
        adv_data.service_data_len = 0;
        adv_data.p_service_data = NULL;
    }

    // service uuid
    if (args[3].u_obj != mp_const_none) {
        if (MP_OBJ_IS_SMALL_INT(args[3].u_obj)) {
            int32_t srv_uuid = mp_obj_get_int(args[3].u_obj);
            if (srv_uuid > 0xFFFF) {
                adv_data.service_uuid_len = 4;
            } else {
                adv_data.service_uuid_len = 2;
            }
            adv_data.p_service_uuid = (uint8_t *)&srv_uuid;
        } else {
            mp_get_buffer_raise(args[3].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
            adv_data.service_uuid_len = uuid_bufinfo.len;
            adv_data.p_service_uuid = uuid_bufinfo.buf;
        }
    } else {
        adv_data.service_uuid_len = 0;
        adv_data.p_service_uuid = NULL;
    }

    adv_data.set_scan_rsp = false;
    adv_data.include_txpower = true,
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x00;
    adv_data.flag = 0x2;

    esp_ble_gap_config_adv_data(&adv_data);

    // wait for the advertisement data to be configured
    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_set_advertisement_obj, 1, bt_set_advertisement);

STATIC mp_obj_t bt_advertise(mp_obj_t self_in, mp_obj_t enable) {
    if (mp_obj_is_true(enable)) {
        esp_ble_gap_start_advertising(&bt_adv_params);
        bt_obj.advertising = true;
    } else {
        // FIXME (why is it needed to start and stop?)
        esp_ble_gap_start_advertising(&bt_adv_params);
        esp_ble_gap_stop_advertising();
        bt_obj.advertising = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_advertise_obj, bt_advertise);

STATIC mp_obj_t bt_service (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,                     MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_isprimary,                MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_nbr_chars,                MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    };

    mp_buffer_info_t uuid_bufinfo;
    esp_gatt_srvc_id_t service_id;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // service uuid
    if (MP_OBJ_IS_SMALL_INT(args[0].u_obj)) {
        int32_t srv_uuid = mp_obj_get_int(args[0].u_obj);
        if (srv_uuid > 0xFFFF) {
            service_id.id.uuid.len = 4;
            service_id.id.uuid.uuid.uuid32 = srv_uuid;
        } else {
            service_id.id.uuid.len = 2;
            service_id.id.uuid.uuid.uuid16 = srv_uuid;
        }
    } else {
        mp_get_buffer_raise(args[0].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
        if (uuid_bufinfo.len != 16) {
            goto error;
        }
        service_id.id.uuid.len = uuid_bufinfo.len;
        memcpy(service_id.id.uuid.uuid.uuid128, uuid_bufinfo.buf, sizeof(service_id.id.uuid.uuid.uuid128));
    }

    service_id.is_primary = args[1].u_bool;
    service_id.id.inst_id = 0x00;

    esp_ble_gatts_create_service(bt_obj.gatts_if, &service_id, (args[2].u_int * 3) + 1);

    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    bt_gatts_srv_obj_t *srv = m_new_obj(bt_gatts_srv_obj_t);
    srv->base.type = (mp_obj_t)&mod_bt_gatts_service_type;
    srv->handle = gatts_event.service_handle;

    esp_ble_gatts_start_service(gatts_event.service_handle);

    mp_obj_list_append((mp_obj_t)&bt_obj.srv_list, srv);

    return srv;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_service_obj, 1, bt_service);


STATIC mp_obj_t bt_characteristic (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,                     MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ },
        { MP_QSTR_permissions,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_properties,               MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_value,                    MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    bt_gatts_srv_obj_t *self = pos_args[0];

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    mp_buffer_info_t uuid_bufinfo;
    esp_bt_uuid_t char_uuid;

    // characteristic uuid
    if (MP_OBJ_IS_SMALL_INT(args[0].u_obj)) {
        int32_t srv_uuid = mp_obj_get_int(args[0].u_obj);
        if (srv_uuid > 0xFFFF) {
            char_uuid.len = 4;
            char_uuid.uuid.uuid32 = srv_uuid;
        } else {
            char_uuid.len = 2;
            char_uuid.uuid.uuid16 = srv_uuid;
        }
    } else {
        mp_get_buffer_raise(args[0].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
        if (uuid_bufinfo.len != 16) {
            goto error;
        }
        char_uuid.len = uuid_bufinfo.len;
        memcpy(char_uuid.uuid.uuid128, uuid_bufinfo.buf, sizeof(char_uuid.uuid.uuid128));
    }

    uint32_t permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
    if (args[1].u_obj != mp_const_none) {
        permissions = mp_obj_get_int(args[1].u_obj);
    }

    uint32_t properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    if (args[2].u_obj != mp_const_none) {
        properties = mp_obj_get_int(args[2].u_obj);
    }

    esp_ble_gatts_add_char(self->handle, &char_uuid, permissions, properties);

    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    bt_gatts_char_obj_t *characteristic = m_new_obj(bt_gatts_char_obj_t);
    characteristic->base.type = (mp_obj_t)&mod_bt_gatts_char_type;
    characteristic->handle = gatts_event.char_handle;
    characteristic->srv = self;
    characteristic->trigger = 0;
    characteristic->events = 0;
    memcpy(&characteristic->uuid, &char_uuid, sizeof(char_uuid));

    if (args[3].u_obj != mp_const_none) {
        // characteristic value
        if (MP_OBJ_IS_SMALL_INT(args[3].u_obj)) {
            int32_t value = mp_obj_get_int(args[3].u_obj);
            memcpy(characteristic->value, &value, sizeof(value));
            if (value > 0xFF) {
                characteristic->value_len = 2;
            } else if (value > 0xFFFF) {
                characteristic->value_len = 4;
            } else {
                characteristic->value_len = 1;
            }
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[3].u_obj, &value_bufinfo, MP_BUFFER_READ);
            memcpy(characteristic->value, value_bufinfo.buf, value_bufinfo.len);
            characteristic->value_len = value_bufinfo.len;
        }
    } else {
        characteristic->value[0] = 0;
        characteristic->value_len = 1;
    }

    mp_obj_list_append((mp_obj_t)&bt_obj.char_list, characteristic);

    // characteristic->descr_uuid.len = ESP_UUID_LEN_16;
    // characteristic->descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
    // esp_ble_gatts_add_char_descr(self->handle, &characteristic->descr_uuid,
    //                              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);

    return characteristic;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_characteristic_obj, 1, bt_characteristic);


STATIC const mp_map_elem_t bt_gatts_service_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_characteristic),          (mp_obj_t)&bt_characteristic_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_gatts_service_locals_dict, bt_gatts_service_locals_dict_table);

static const mp_obj_type_t mod_bt_gatts_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSService,
    .locals_dict = (mp_obj_t)&bt_gatts_service_locals_dict,
};

STATIC mp_obj_t bt_characteristic_value (mp_uint_t n_args, const mp_obj_t *args) {
    bt_gatts_char_obj_t *self = args[0];
    if (n_args == 1) {
        // get
        return mp_obj_new_bytes(self->value, self->value_len);
    } else {
        // set
        if (MP_OBJ_IS_SMALL_INT(args[1])) {
            int32_t value = mp_obj_get_int(args[1]);
            memcpy(self->value, &value, sizeof(value));
            if (value > 0xFF) {
                self->value_len = 2;
            } else if (value > 0xFFFF) {
                self->value_len = 4;
            } else {
                self->value_len = 1;
            }
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[1], &value_bufinfo, MP_BUFFER_READ);
            memcpy(self->value, value_bufinfo.buf, value_bufinfo.len);
            self->value_len = value_bufinfo.len;
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bt_characteristic_value_obj, 1, 2, bt_characteristic_value);

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t bt_characteristic_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    bt_gatts_char_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        self->handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    } else {
        self->trigger = 0;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_characteristic_callback_obj, 1, bt_characteristic_callback);

STATIC mp_obj_t bt_characteristic_events(mp_obj_t self_in) {
    bt_gatts_char_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_characteristic_events_obj, bt_characteristic_events);

STATIC const mp_map_elem_t bt_gatts_char_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),          (mp_obj_t)&bt_characteristic_value_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),       (mp_obj_t)&bt_characteristic_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),         (mp_obj_t)&bt_characteristic_events_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_gatts_char_locals_dict, bt_gatts_char_locals_dict_table);

static const mp_obj_type_t mod_bt_gatts_char_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSCharacteristic,
    .locals_dict = (mp_obj_t)&bt_gatts_char_locals_dict,
};


STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                  (mp_obj_t)&bt_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start_scan),              (mp_obj_t)&bt_start_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isscanning),              (mp_obj_t)&bt_isscanning_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop_scan),               (mp_obj_t)&bt_stop_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_adv),                 (mp_obj_t)&bt_read_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_resolve_adv_data),        (mp_obj_t)&bt_resolve_adv_data_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),                 (mp_obj_t)&bt_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_advertisement),       (mp_obj_t)&bt_set_advertisement_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_advertise),               (mp_obj_t)&bt_advertise_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_service),                 (mp_obj_t)&bt_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&bt_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),                  (mp_obj_t)&bt_events_obj },

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

    { MP_OBJ_NEW_QSTR(MP_QSTR_NEW_ADV_EVENT),           MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_ADV_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_CONNECTED),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_CONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_DISCONNECTED),     MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_DISCONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_READ_EVENT),         MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_READ_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_WRITE_EVENT),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_WRITE_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_NOTIFY_EVENT),       MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_NOTIFY_EVT) },
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
        xQueueReset(xScanQueue);
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
    .name = MP_QSTR_GATTCConnection,
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
        xQueueReset(xScanQueue);
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
    .name = MP_QSTR_GATTCService,
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
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        bt_obj.conn_id = self->service->connection->conn_id;

        if (ESP_OK != esp_ble_gattc_read_char (self->service->connection->conn_id,
                                               &self->service->srv_id,
                                               &self->characteristic.char_id,
                                               ESP_GATT_AUTH_REQ_NONE)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        while (bt_obj.busy) {
            mp_hal_delay_ms(5);
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
        xQueueReset(xScanQueue);
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
            mp_hal_delay_ms(5);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            if (bt_event.write.status != ESP_GATT_OK) {
                goto error;
            }
        } else {
            goto error;
        }
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_char_write_obj, bt_char_write);

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t bt_char_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    bt_char_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        self->handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }

        if (self->service->connection->conn_id > 0) {
            if (ESP_OK != esp_ble_gattc_register_for_notify (self->service->connection->gatt_if,
                                                             self->service->connection->srv_bda,
                                                             &self->service->srv_id,
                                                             &self->characteristic.char_id)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }  else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
        }
    } else {
        self->trigger = 0;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_char_callback_obj, 1, bt_char_callback);

STATIC const mp_map_elem_t bt_characteristic_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_char_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_char_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_properties),              (mp_obj_t)&bt_char_properties_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                    (mp_obj_t)&bt_char_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),                   (mp_obj_t)&bt_char_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&bt_char_callback_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_descriptors),             (mp_obj_t)&bt_char_descriptors_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_characteristic_locals_dict, bt_characteristic_locals_dict_table);

static const mp_obj_type_t mod_bt_characteristic_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCCharacteristic,
    .locals_dict = (mp_obj_t)&bt_characteristic_locals_dict,
};

// static const mp_obj_type_t mod_bt_descriptor_type = {
    // { &mp_type_type },
    // .name = MP_QSTR_BT_DESCRIPTOR,
    // .locals_dict = (mp_obj_t)&bt_descriptor_locals_dict,
// };
