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
#define MODBT_MAX_SCAN_RESULTS                  16

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    uint8_t mac[6];
    char name[MOD_BT_MAX_ADVERTISEMENT_LEN + 1];
} bt_scan_result_t;

typedef struct {
  mp_obj_base_t         base;
  uint32_t              scan_duration;
  bool                  init;
  bool                  scan_complete;
  uint8_t               nbr_scan_results;
  bt_scan_result_t      *scan_result[MODBT_MAX_SCAN_RESULTS];
} bt_obj_t;

typedef enum {
    E_BT_STACK_MODE_BLE = 0,
    E_BT_STACK_MODE_BT
} bt_mode_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modbt_init0(void) {
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


static void esp_gap_cb(uint32_t event, void *param);

static void esp_gattc_cb(uint32_t event, void *param);

static void esp_gap_cb(uint32_t event, void *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second
        LOG_INFO("Start scanning\n");
        esp_ble_gap_start_scanning(bt_obj.scan_duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            if (bt_obj.nbr_scan_results < MODBT_MAX_SCAN_RESULTS) {
                // first check that it's not a repeated result
                for (int i = 0; i < bt_obj.nbr_scan_results; i++) {
                    if (!memcmp((void *)bt_obj.scan_result[i]->mac, (void *)scan_result->scan_rst.bda, 6)) {
                        return;
                    }
                }

                bt_obj.scan_result[bt_obj.nbr_scan_results] = pvPortMalloc(sizeof(bt_scan_result_t));
                if (!bt_obj.scan_result[bt_obj.nbr_scan_results]) {
                    return;
                }

                adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                    ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                if (adv_name_len > 0 && adv_name) {
                    if (adv_name_len > MOD_BT_MAX_ADVERTISEMENT_LEN) {
                        adv_name_len = MOD_BT_MAX_ADVERTISEMENT_LEN;
                    }
                    memcpy((char *)bt_obj.scan_result[bt_obj.nbr_scan_results]->name, (char *)adv_name, adv_name_len);
                    bt_obj.scan_result[bt_obj.nbr_scan_results]->name[adv_name_len] = '\0';
                } else {
                    bt_obj.scan_result[bt_obj.nbr_scan_results]->name[0] = '\0';
                }
                memcpy((void *)bt_obj.scan_result[bt_obj.nbr_scan_results]->mac, scan_result->scan_rst.bda, 6);
                bt_obj.nbr_scan_results++;
            }
            break;
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            LOG_INFO("Discovery result\n");
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            LOG_INFO("Scan finished\n");
            bt_obj.scan_complete = true;
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
        LOG_INFO("status = %x, client_if = %x\n", status, client_if);
        break;
    case ESP_GATTC_OPEN_EVT:
        conn_id = p_data->open.conn_id;
        LOG_INFO("ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d\n", conn_id, p_data->open.gatt_if, p_data->open.status);
        esp_ble_gattc_search_service(conn_id, NULL);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
        conn_id = p_data->open.conn_id;
        LOG_INFO("SEARCH RES: conn_id = %x\n", conn_id);
        if (srvc_id->id.uuid.len == ESP_UUID_LEN_16) {
            LOG_INFO("UUID16: %x\n", srvc_id->id.uuid.uuid.uuid16);
        } else if (srvc_id->id.uuid.len == ESP_UUID_LEN_32) {
            LOG_INFO("UUID32: %x\n", srvc_id->id.uuid.uuid.uuid32);
        } else if (srvc_id->id.uuid.len == ESP_UUID_LEN_128) {
            LOG_INFO("UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n", srvc_id->id.uuid.uuid.uuid128[0],
                     srvc_id->id.uuid.uuid.uuid128[1], srvc_id->id.uuid.uuid.uuid128[2], srvc_id->id.uuid.uuid.uuid128[3],
                     srvc_id->id.uuid.uuid.uuid128[4], srvc_id->id.uuid.uuid.uuid128[5], srvc_id->id.uuid.uuid.uuid128[6],
                     srvc_id->id.uuid.uuid.uuid128[7], srvc_id->id.uuid.uuid.uuid128[8], srvc_id->id.uuid.uuid.uuid128[9],
                     srvc_id->id.uuid.uuid.uuid128[10], srvc_id->id.uuid.uuid.uuid128[11], srvc_id->id.uuid.uuid.uuid128[12],
                     srvc_id->id.uuid.uuid.uuid128[13], srvc_id->id.uuid.uuid.uuid128[14], srvc_id->id.uuid.uuid.uuid128[15]);
        } else {
            LOG_ERROR("UNKNOWN LEN %d\n", srvc_id->id.uuid.len);
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        conn_id = p_data->search_cmpl.conn_id;
        LOG_INFO("SEARCH_CMPL: conn_id = %x, status %d\n", conn_id, p_data->search_cmpl.status);
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

STATIC mp_obj_t bt_scan_devices(mp_obj_t self_in, mp_obj_t duration) {
    STATIC const qstr bt_scan_info_fields[] = {
        MP_QSTR_name, MP_QSTR_mac
    };

    uint32_t _duration = mp_obj_get_int(duration);
    if (_duration <= 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid scan time"));
    }
    bt_obj.scan_duration = _duration;

    bt_obj.scan_complete = false;
    bt_obj.nbr_scan_results = 0;
    esp_ble_gap_set_scan_params(&ble_scan_params);

    while (!bt_obj.scan_complete) {
        vTaskDelay(50 / portTICK_RATE_MS);
    }

    mp_obj_t devices = mp_const_none;
    devices = mp_obj_new_list(0, NULL);
    for (int i = 0; i < bt_obj.nbr_scan_results; i++) {
        mp_obj_t tuple[2];
        uint32_t name_len = strlen((char *)bt_obj.scan_result[i]->name);
        if (name_len > 0) {
            tuple[0] = mp_obj_new_str((const char *)bt_obj.scan_result[i]->name, name_len, false);
        } else {
            tuple[0] = mp_const_none;
        }
        tuple[1] = mp_obj_new_bytes((const byte *)bt_obj.scan_result[i]->mac, 6);

        // release the memory used by the scan result
        vPortFree(bt_obj.scan_result[i]);

        // add the network to the list
        mp_obj_list_append(devices, mp_obj_new_attrtuple(bt_scan_info_fields, 2, tuple));
    }

    return devices;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_scan_devices_obj, bt_scan_devices);

STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_scan_devices),        (mp_obj_t)&bt_scan_devices_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_scan_beacons),        (mp_obj_t)&bt_scan_beacons_obj },
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

