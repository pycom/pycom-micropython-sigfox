/*
 * Copyright (c) 2021, Pycom Limited.
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
#include "py/stream.h"
#include "modusocket.h"
#include "pycom_config.h"
#include "modbt.h"
#include "mpirq.h"
#include "antenna.h"

#include "esp_bt.h"
#include "common/bt_trace.h"
#include "stack/bt_types.h"
#include "stack/btm_api.h"
#include "bta/bta_api.h"
#include "bta/bta_gatt_api.h"
#include "api/esp_gap_ble_api.h"
#include "api/esp_gattc_api.h"
#include "api/esp_gatts_api.h"
#include "api/esp_gatt_defs.h"
#include "api/esp_bt_main.h"
#include "api/esp_gatt_common_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/opt.h"
#include "lwip/def.h"

#include "mbedtls/sha1.h"
#include "nvs.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define BT_SCAN_QUEUE_SIZE_MAX                              (16)
#define BT_GATTS_QUEUE_SIZE_MAX                             (2)
#define BT_MTU_SIZE_MAX                                     (200)
#define BT_CHAR_VALUE_SIZE_MAX                              (BT_MTU_SIZE_MAX - 3)

#define MOD_BT_CLIENT_APP_ID                                (0)
#define MOD_BT_SERVER_APP_ID                                (1)

#define MOD_BT_GATTC_ADV_EVT                                (0x0001)
#define MOD_BT_GATTS_CONN_EVT                               (0x0002)
#define MOD_BT_GATTS_DISCONN_EVT                            (0x0004)
#define MOD_BT_GATTS_READ_EVT                               (0x0008)
#define MOD_BT_GATTS_WRITE_EVT                              (0x0010)
#define MOD_BT_GATTC_NOTIFY_EVT                             (0x0020)
#define MOD_BT_GATTC_INDICATE_EVT                           (0x0040)
#define MOD_BT_GATTS_SUBSCRIBE_EVT                          (0x0080)
#define MOD_BT_GATTC_MTU_EVT                                (0x0100)
#define MOD_BT_GATTS_MTU_EVT                                (0x0200)
#define MOD_BT_GATTS_CLOSE_EVT                              (0x0400)
#define MOD_BT_NVS_NAMESPACE                                "BT_NVS"
#define MOD_BT_HASH_SIZE                                    (20)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t         base;
    int32_t               scan_duration;
    uint8_t               gatts_mtu;
    mp_obj_t              handler;
    mp_obj_t              handler_arg;
    esp_bd_addr_t         client_bda;
    int32_t               gatts_conn_id;
    uint32_t              trigger;
    int32_t               events;
    uint16_t              gatts_if;
    uint16_t              gattc_if;
    bool                  init;
    bool                  busy;
    bool                  scanning;
    bool                  advertising;
    bool                  controller_active;
    bool                  secure;
    bool                  secure_connections;
    bool                  privacy;
} bt_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_list_t         srv_list;
    esp_bd_addr_t         srv_bda;
    int32_t               conn_id;
    uint16_t              mtu;
    esp_gatt_if_t         gatt_if;
    esp_ble_addr_type_t   addr_type;
} bt_connection_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_list_t         char_list;
    bt_connection_obj_t   *connection;
    esp_gatt_id_t         srv_id;
    uint16_t              start_handle;
    uint16_t              end_handle;
} bt_srv_obj_t;

typedef struct {
    esp_gatt_id_t          srv_id;
    uint16_t               start_handle;
    uint16_t               end_handle;
} bt_srv_t;

typedef struct {
    esp_gatt_id_t           char_id;
    esp_gatt_char_prop_t    char_prop;
} bt_char_t;

typedef struct {
    mp_obj_base_t           base;
    bt_srv_obj_t            *service;
    esp_gattc_char_elem_t   characteristic;
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
    uint32_t                trigger;
    uint32_t                events;
    uint16_t                value_len;
    uint8_t                 value[BT_CHAR_VALUE_SIZE_MAX];
    // mp_obj_list_t         desc_list;
} bt_char_obj_t;

typedef struct {
    uint8_t     value[BT_CHAR_VALUE_SIZE_MAX];
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
} bt_connection_event_t;

typedef struct {
    esp_gatt_status_t status;
} bt_register_for_notify_event_t;

typedef union {
    esp_ble_gap_cb_param_t          scan;
    bt_srv_t                        service;
    bt_read_value_t                 read;
    bt_write_value_t                write;
    bt_connection_event_t           connection;
    bt_register_for_notify_event_t  register_for_notify;
} bt_event_result_t;

typedef union {
    uint16_t service_handle;
    uint16_t char_handle;
    uint16_t char_descr_handle;
    bool     adv_set;
} bt_gatts_event_result_t;

typedef struct {
    mp_obj_base_t         base;
    uint16_t              handle;
    bool                  started;
} bt_gatts_srv_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_t              parent;
    esp_bt_uuid_t         uuid;
    uint32_t              properties;
    uint16_t              handle;
    uint16_t              value_len;
    uint8_t               value[BT_CHAR_VALUE_SIZE_MAX];
    bool                  is_char;
} bt_gatts_attr_obj_t;

typedef struct {
    bt_gatts_attr_obj_t   attr_obj;
    mp_obj_t              handler;
    mp_obj_t              handler_arg;
    uint32_t              trigger;
    uint32_t              events;
    uint32_t              trans_id;
    bool                  read_request;
    uint16_t              config;
} bt_gatts_char_obj_t;

typedef union {
    uint32_t pin;
    uint8_t value[4];
} bt_hash_obj_t;

typedef struct {
    bt_gatts_char_obj_t *chr;
    uint32_t event;
    uint32_t data_length;
    uint8_t* data;
} gatts_char_cbk_arg_t;

typedef struct {
    bt_char_obj_t *chr;
    uint32_t event;
    uint32_t data_length;
    uint8_t* data;
} gattc_char_cbk_arg_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;
static QueueHandle_t xScanQueue;
static QueueHandle_t xGattsQueue;

static esp_ble_adv_data_t adv_data;
static esp_ble_adv_data_t scan_rsp_data;

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

static bool mod_bt_allow_resume_deinit;
static uint16_t mod_bt_gatts_mtu_restore = 0;

static nvs_handle modbt_nvs_handle;
static uint8_t tx_pwr_level_to_dbm[] = {-12, -9, -6, -3, 0, 3, 6, 9};
static EventGroupHandle_t bt_event_group;
static uint16_t bt_conn_mtu = 0;
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void gap_events_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gattc_events_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void close_connection(int32_t conn_id);
static esp_err_t modem_sleep(bool enable);

STATIC void bluetooth_callback_handler(void *arg);
STATIC void gattc_char_callback_handler(void *arg);
STATIC void gatts_char_callback_handler(void *arg);
static mp_obj_t modbt_start_scan(mp_obj_t timeout);
static mp_obj_t modbt_conn_disconnect(mp_obj_t self_in);
static mp_obj_t modbt_connect(mp_obj_t addr, esp_ble_addr_type_t addr_type);

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
    if(!bt_event_group)
    {
        bt_event_group = xEventGroupCreate();
    }
    else
    {
        //Using only specific events in group for now
        xEventGroupClearBits(bt_event_group, MOD_BT_GATTC_MTU_EVT | MOD_BT_GATTS_MTU_EVT | MOD_BT_GATTS_DISCONN_EVT | MOD_BT_GATTS_CLOSE_EVT);
    }
    bt_event_group = xEventGroupCreate();

    if (bt_obj.init) {
        esp_ble_gattc_app_unregister(MOD_BT_CLIENT_APP_ID);
        esp_ble_gatts_app_unregister(MOD_BT_SERVER_APP_ID);
    }

    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(btc_conn_list), 0);
    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_srv_list), 0);
    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_attr_list), 0);

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    mod_bt_allow_resume_deinit = false;
}

void modbt_deinit(bool allow_reconnect)
{
    uint16_t timeout = 0;
    if (bt_obj.init)
    {
        if (bt_obj.scanning) {
            esp_ble_gap_stop_scanning();
            bt_obj.scanning = false;
        }
        /* Allow reconnection flag */
        mod_bt_allow_resume_deinit = allow_reconnect;

        bt_connection_obj_t *connection_obj;

        for (mp_uint_t i = 0; i < MP_STATE_PORT(btc_conn_list).len; i++)
        {
            // loop through the connections
            connection_obj = ((bt_connection_obj_t *)(MP_STATE_PORT(btc_conn_list).items[i]));
            //close connections
            modbt_conn_disconnect(connection_obj);
        }
        while ((MP_STATE_PORT(btc_conn_list).len > 0) && (timeout < 20) && !mod_bt_allow_resume_deinit)
        {
            vTaskDelay (50 / portTICK_PERIOD_MS);
            timeout++;
        }

        if (bt_obj.gatts_conn_id >= 0)
        {
            if (!mod_bt_allow_resume_deinit) {
                bt_obj.advertising  = false;
            }
            esp_ble_gatts_close(bt_obj.gatts_if, bt_obj.gatts_conn_id);
            xEventGroupWaitBits(bt_event_group, MOD_BT_GATTS_DISCONN_EVT | MOD_BT_GATTS_CLOSE_EVT, true, true, 1000/portTICK_PERIOD_MS);
        }

        esp_ble_gattc_app_unregister(MOD_BT_CLIENT_APP_ID);
        esp_ble_gatts_app_unregister(MOD_BT_SERVER_APP_ID);

        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        bt_obj.init = false;
        xEventGroupClearBits(bt_event_group, MOD_BT_GATTC_MTU_EVT | MOD_BT_GATTS_MTU_EVT | MOD_BT_GATTS_DISCONN_EVT | MOD_BT_GATTS_CLOSE_EVT);
    }
}

void bt_resume(bool reconnect)
{
    if(mod_bt_allow_resume_deinit && !bt_obj.init)
    {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&bt_cfg);

        esp_bt_controller_enable(ESP_BT_MODE_BLE);

        if (ESP_OK != esp_bluedroid_init()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth init failed"));
        }
        if (ESP_OK != esp_bluedroid_enable()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth enable failed"));
        }

        esp_ble_gap_register_callback(gap_events_handler);
        esp_ble_gattc_register_callback(gattc_events_handler);
        esp_ble_gatts_register_callback(gatts_event_handler);

        esp_ble_gattc_app_register(MOD_BT_CLIENT_APP_ID);
        esp_ble_gatts_app_register(MOD_BT_SERVER_APP_ID);

        esp_ble_gatt_set_local_mtu(mod_bt_gatts_mtu_restore);

        // If this list has 0 elements it means there were no active connections
        if(MP_STATE_PORT(btc_conn_list).len > 0 && reconnect)
        {
            /* Enable Scan */
            modbt_start_scan(MP_OBJ_NEW_SMALL_INT(-1));
            mp_hal_delay_ms(50);
            while(!bt_obj.scanning){
                /* Wait for scanning to start */
            }

            /* Re-connect to all previously existing connections */
            // Need to save the old connections into a temporary list because during connect the original list is manipulated (items added)
            mp_obj_list_t btc_conn_list_tmp;
            mp_obj_list_init(&btc_conn_list_tmp, 0);
            for (mp_uint_t i = 0; i < MP_STATE_PORT(btc_conn_list).len; i++) {
                bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(MP_STATE_PORT(btc_conn_list).items[i]));
                mp_obj_list_append(&btc_conn_list_tmp, connection_obj);
            }

            // Connect to the old connections
            for (mp_uint_t i = 0; i < btc_conn_list_tmp.len; i++) {
                bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(btc_conn_list_tmp.items[i]));
                // Initiates re-connection
                bt_connection_obj_t *new_connection_obj = modbt_connect(mp_obj_new_bytes((const byte *)connection_obj->srv_bda, 6), connection_obj->addr_type);
                // If new connection object has been created then overwrite the original one so from the MicroPython code the same reference can be used
                if(new_connection_obj != mp_const_none) {
                    memcpy(connection_obj, new_connection_obj, sizeof(bt_connection_obj_t));
                    // As modbt_connect appends the new connection to the original list, it needs to be removed because it is not needed
                    mp_obj_list_remove((void *)&MP_STATE_PORT(btc_conn_list), new_connection_obj);
                }
                else {
                    // Remove the old connection from the original list because connection could not be established with it
                    mp_obj_list_remove((void *)&MP_STATE_PORT(btc_conn_list), connection_obj);
                }
            }

            /* See if there was an advertisement active before Sleep */
            if(bt_obj.advertising) {
                esp_ble_gap_start_advertising(&bt_adv_params);
            }
        }

        bt_obj.init = true;
        mod_bt_allow_resume_deinit = false;
    }
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static esp_gatt_status_t status = ESP_GATT_ERROR;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

STATIC mp_obj_t bt_nvram_erase (mp_obj_t self_in) {
    if (nvs_open(MOD_BT_NVS_NAMESPACE, NVS_READWRITE, &modbt_nvs_handle) != ESP_OK) {
        mp_printf(&mp_plat_print, "Error opening secure BLE NVS namespace!\n");
    }

    if (ESP_OK != nvs_erase_all(modbt_nvs_handle)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    nvs_commit(modbt_nvs_handle);
    nvs_close(modbt_nvs_handle);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_nvram_erase_obj, bt_nvram_erase);

static void create_hash(uint32_t pin, uint8_t *h_value)
{
    bt_hash_obj_t pin_hash;
    mbedtls_sha1_context sha1_context;

    mbedtls_sha1_init(&sha1_context);
    mbedtls_sha1_starts_ret(&sha1_context);

    pin_hash.pin = pin;
    mbedtls_sha1_update_ret(&sha1_context, pin_hash.value, 4);

    mbedtls_sha1_finish_ret(&sha1_context, h_value);
    mbedtls_sha1_free(&sha1_context);
}

static bool pin_changed(uint32_t new_pin)
{
     bool ret = false;
     uint32_t h_size = MOD_BT_HASH_SIZE;
     uint8_t h_stored[MOD_BT_HASH_SIZE] = {0};
     uint8_t h_created[MOD_BT_HASH_SIZE] = {0};
     const char *key = "bt_pin_hash";
     esp_err_t esp_err = ESP_OK;

     if (nvs_open(MOD_BT_NVS_NAMESPACE, NVS_READWRITE, &modbt_nvs_handle) != ESP_OK) {
        mp_printf(&mp_plat_print, "Error opening secure BLE NVS namespace!\n");
     }
     nvs_get_blob(modbt_nvs_handle, key, h_stored, &h_size);

     create_hash(new_pin, h_created);

     if (memcmp(h_stored, h_created, MOD_BT_HASH_SIZE) != 0) {
         esp_err = nvs_set_blob(modbt_nvs_handle, key, h_created, h_size);
         if (esp_err == ESP_OK) {
            nvs_commit(modbt_nvs_handle);
            ret = true;
          }
     }

     nvs_close(modbt_nvs_handle);

     return ret;
}

static void remove_all_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }
    free(dev_list);
}

static void set_secure_parameters(bool secure_connections) {
    esp_ble_auth_req_t auth_req = secure_connections ? ESP_LE_AUTH_REQ_SC_MITM_BOND : ESP_LE_AUTH_BOND;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));

    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));

    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

static void set_pin(uint32_t new_pin)
{
    if (new_pin > 999999) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Pin code with 1-6 digit length is allowed only!"));
    }

    if (pin_changed(new_pin)) {
       remove_all_bonded_devices();
    }

    uint32_t passkey = new_pin;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    bt_obj.secure = true;
    set_secure_parameters(bt_obj.secure_connections);
}

static void close_connection (int32_t conn_id) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(btc_conn_list).len; i++) {
        bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(MP_STATE_PORT(btc_conn_list).items[i]));
        if (connection_obj->conn_id == conn_id) {
            connection_obj->conn_id = -1;
            mp_obj_list_remove((void *)&MP_STATE_PORT(btc_conn_list), connection_obj);
        }
    }
}

static esp_err_t modem_sleep(bool enable)
{
    esp_err_t err = esp_bt_controller_get_status();

    if(enable)
    {
        /* Enable Modem Sleep */
        err = esp_bt_sleep_enable();
    }
    else
    {
        /* Disable Modem Sleep */
        err = esp_bt_sleep_disable();

        /* Wakeup the modem is it is sleeping */
        if (esp_bt_controller_is_sleeping() && err == ESP_OK)
        {
            esp_bt_controller_wakeup_request();
        }
    }
    return err;
}

static bt_char_obj_t *find_gattc_char (int32_t conn_id, uint16_t char_handle) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(btc_conn_list).len; i++) {
        // search through the connections
        bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(MP_STATE_PORT(btc_conn_list).items[i]));
        if (connection_obj->conn_id == conn_id) {
            // search through the services
            for (mp_uint_t j = 0; j < connection_obj->srv_list.len; j++) {
                // search through the characteristics
                bt_srv_obj_t *srv_obj = ((bt_srv_obj_t *)(connection_obj->srv_list.items[j]));
                for (mp_uint_t j = 0; j < srv_obj->char_list.len; j++) {
                    bt_char_obj_t *char_obj = ((bt_char_obj_t *)(srv_obj->char_list.items[j]));
                    if (char_obj->characteristic.char_handle == char_handle) {
                        return char_obj;
                    }
                }
            }
        }
    }
    return NULL;
}

static bt_gatts_attr_obj_t *find_gatts_attr_by_handle (uint16_t handle) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(bts_attr_list).len; i++) {
        bt_gatts_attr_obj_t *char_obj = ((bt_gatts_attr_obj_t *)(MP_STATE_PORT(bts_attr_list).items[i]));
        if (char_obj->handle == handle) {
            return char_obj;
        }
    }
    return NULL;
}

static void gap_events_handler (esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        int32_t duration = bt_obj.scan_duration;
        // the unit of the duration is seconds
        if (duration < 0) {
            duration = 0x0FFF;
        }
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.adv_set = true;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GAP_BLE_NC_REQ_EVT:
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
         //printf("BLE paring passkey : %d\n", param->ble_security.key_notif.passkey);
         break;
    }
    case ESP_GAP_BLE_SEC_REQ_EVT: {
        if (bt_obj.secure){
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        }
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        bt_event_result_t bt_event_result;
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        memcpy(&bt_event_result.scan, scan_result, sizeof(esp_ble_gap_cb_param_t));
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            bt_obj.events |= MOD_BT_GATTC_ADV_EVT;
            if (bt_obj.trigger & MOD_BT_GATTC_ADV_EVT) {
                mp_irq_queue_interrupt_non_ISR(bluetooth_callback_handler, (void *)&bt_obj);
            }
            break;
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            if (bt_obj.scan_duration < 0) {
                esp_ble_gap_set_scan_params(&ble_scan_params);
            } else {
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

static void gattc_events_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    int32_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    bt_event_result_t bt_event_result;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        status = p_data->reg.status;
        bt_obj.gattc_if = gattc_if;
        if (bt_obj.secure){
           esp_ble_gap_config_local_privacy(bt_obj.privacy);
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK) {
            bt_event_result.connection.conn_id = -1;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            bt_obj.busy = false;
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
        conn_id = p_data->connect.conn_id;
        bt_event_result.connection.conn_id = conn_id;
        bt_event_result.connection.gatt_if = gattc_if;
        memcpy(bt_event_result.connection.srv_bda, p_data->connect.remote_bda, ESP_BD_ADDR_LEN);
        esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        // connection process and MTU request complete
        bt_obj.busy = false;
        bt_conn_mtu = p_data->cfg_mtu.mtu;
        xEventGroupSetBits(bt_event_group, MOD_BT_GATTC_MTU_EVT);
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        if (p_data->read.status == ESP_GATT_OK) {
            uint16_t read_len = p_data->read.value_len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : p_data->read.value_len;
            memcpy(&bt_event_result.read.value, p_data->read.value, read_len);
            bt_event_result.read.value_len = read_len;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        }
        bt_obj.busy = false;
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        bt_event_result.write.status = p_data->write.status;
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_id_t *srvc_id = (esp_gatt_id_t *)&p_data->search_res.srvc_id;
        memcpy(&bt_event_result.service.srv_id, srvc_id, sizeof(esp_gatt_id_t));
        bt_event_result.service.start_handle = p_data->search_res.start_handle;
        bt_event_result.service.end_handle = p_data->search_res.end_handle;
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        break;
    }
    case ESP_GATTC_READ_DESCR_EVT:
      if (p_data->read.status == ESP_GATT_OK) {
          uint16_t read_len = p_data->read.value_len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : p_data->read.value_len;
          memcpy(&bt_event_result.read.value, p_data->read.value, read_len);
          bt_event_result.read.value_len = read_len;
          xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
      }
      bt_obj.busy = false;
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        bt_event_result.register_for_notify.status = p_data->reg_for_notify.status;
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        break;
    case ESP_GATTC_NOTIFY_EVT: {
        bt_char_obj_t *char_obj;
        char_obj = find_gattc_char (p_data->notify.conn_id, p_data->notify.handle);
        if (char_obj != NULL) {
            // copy the new value into the characteristic
            memcpy(&char_obj->value, p_data->notify.value, p_data->notify.value_len);
            char_obj->value_len = p_data->notify.value_len;

            // register the event
            if (p_data->notify.is_notify) {
                char_obj->events |= MOD_BT_GATTC_NOTIFY_EVT;
            } else {
                char_obj->events |= MOD_BT_GATTC_INDICATE_EVT;
            }

            if ((char_obj->trigger & MOD_BT_GATTC_NOTIFY_EVT) || (char_obj->trigger & MOD_BT_GATTC_INDICATE_EVT)) {

                gattc_char_cbk_arg_t *cbk_arg = malloc(sizeof(gattc_char_cbk_arg_t));

                cbk_arg->chr = char_obj;
                if (p_data->notify.is_notify) {
                    cbk_arg->event =  MOD_BT_GATTC_NOTIFY_EVT;
                } else {
                    cbk_arg->event =  MOD_BT_GATTC_INDICATE_EVT;
                }
                cbk_arg->data_length = p_data->notify.value_len;
                cbk_arg->data = malloc(cbk_arg->data_length);
                memcpy(cbk_arg->data, p_data->notify.value, cbk_arg->data_length);

                mp_irq_queue_interrupt_non_ISR(gattc_char_callback_handler, cbk_arg);
            }
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
    case ESP_GATTC_CANCEL_OPEN_EVT:
        bt_obj.busy = false;
        break;
        // intentional fall through
    case ESP_GATTC_CLOSE_EVT:
    case ESP_GATTC_DISCONNECT_EVT:
        close_connection(p_data->close.conn_id);
        bt_obj.busy = false;
        break;
    default:
        break;
    }
}

// this function will be called by the interrupt thread
STATIC void bluetooth_callback_handler(void *arg) {
    bt_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {
        mp_call_function_1(self->handler, self->handler_arg);
    }
}

// this function will be called by the interrupt thread
STATIC void gattc_char_callback_handler(void *arg) {
    bt_char_obj_t *chr = ((gattc_char_cbk_arg_t *)arg)->chr;

    if (chr->handler && chr->handler != mp_const_none) {

        mp_obj_t tuple[2];
        tuple[0] = mp_obj_new_int(((gattc_char_cbk_arg_t*)arg)->event);
        tuple[1] = mp_const_none;
        if(((gattc_char_cbk_arg_t*)arg)->data_length > 0) {
            tuple[1] = mp_obj_new_bytes(((gattc_char_cbk_arg_t*)arg)->data, ((gattc_char_cbk_arg_t*)arg)->data_length);
            free(((gattc_char_cbk_arg_t*)arg)->data);
        }

        mp_call_function_2(chr->handler, chr->handler_arg, mp_obj_new_tuple(2, tuple));
    }
    free((gattc_char_cbk_arg_t*)arg);
}

// this function will be called by the interrupt thread
STATIC void gatts_char_callback_handler(void *arg) {

    bt_gatts_char_obj_t *chr = ((gatts_char_cbk_arg_t*)arg)->chr;

    if (chr->handler && chr->handler != mp_const_none) {

        mp_obj_t tuple[2];
        tuple[0] = mp_obj_new_int(((gatts_char_cbk_arg_t*)arg)->event);
        tuple[1] = mp_const_none;
        if(((gatts_char_cbk_arg_t*)arg)->data_length > 0) {
            tuple[1] = mp_obj_new_bytes(((gatts_char_cbk_arg_t*)arg)->data, ((gatts_char_cbk_arg_t*)arg)->data_length);
            free(((gatts_char_cbk_arg_t*)arg)->data);
        }

        mp_obj_t r_value = mp_call_function_2(chr->handler, chr->handler_arg, mp_obj_new_tuple(2, tuple));

        if (chr->read_request) {
            uint32_t u_value;
            uint8_t *value;
            uint8_t value_l = 1;

            chr->read_request = false;
            if (r_value != mp_const_none) {
                if (mp_obj_is_integer(r_value)) {
                    u_value = mp_obj_get_int_truncated(r_value);
                    value = (uint8_t *)&u_value;
                    if (u_value > UINT16_MAX) {
                        value_l = 4;
                        u_value = lwip_htonl(u_value);
                    } else if (u_value > UINT8_MAX) {
                        value_l = 2;
                        u_value = lwip_htons(u_value);
                    }
                } else {
                    mp_buffer_info_t bufinfo;
                    mp_get_buffer_raise(r_value, &bufinfo, MP_BUFFER_READ);
                    value = bufinfo.buf;
                    value_l = bufinfo.len;
                }
            } else {
                value = chr->attr_obj.value;
                value_l = chr->attr_obj.value_len;
            }

            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = chr->attr_obj.handle;
            rsp.attr_value.len = value_l;
            memcpy(&rsp.attr_value.value, value, value_l);
            esp_ble_gatts_send_response(bt_obj.gatts_if, bt_obj.gatts_conn_id, chr->trans_id, ESP_GATT_OK, &rsp);
        }
    }
    free((gatts_char_cbk_arg_t*)arg);
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p = (esp_ble_gatts_cb_param_t *)param;

    switch (event) {
    case ESP_GATTS_REG_EVT:
        bt_obj.gatts_if = gatts_if;
        if (bt_obj.secure){
            esp_ble_gap_config_local_privacy(bt_obj.privacy);
        }
        break;
    case ESP_GATTS_READ_EVT: {
        bt_gatts_attr_obj_t *attr_obj = find_gatts_attr_by_handle (p->read.handle);
        if (attr_obj) {
            if (attr_obj->is_char) {
                bt_gatts_char_obj_t *char_obj = (bt_gatts_char_obj_t *)attr_obj;
                char_obj->events |= MOD_BT_GATTS_READ_EVT;
                if (char_obj->trigger & MOD_BT_GATTS_READ_EVT) {
                    char_obj->read_request = true;
                    char_obj->trans_id = p->read.trans_id;

                    gatts_char_cbk_arg_t *cbk_arg = malloc(sizeof(gatts_char_cbk_arg_t));

                    cbk_arg->chr = char_obj;
                    cbk_arg->event = MOD_BT_GATTS_READ_EVT;
                    cbk_arg->data_length = 0;
                    cbk_arg->data = NULL;

                    mp_irq_queue_interrupt_non_ISR(gatts_char_callback_handler, cbk_arg);
                    break;
                }
            }
            // send the response immediately if it's not a characteristic or if there's no callback registered
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = p->read.handle;
            rsp.attr_value.len = attr_obj->value_len;
            memcpy(&rsp.attr_value.value, attr_obj->value, attr_obj->value_len);
            esp_ble_gatts_send_response(gatts_if, p->read.conn_id, p->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (!p->write.is_prep) {
            bt_gatts_attr_obj_t *attr_obj = find_gatts_attr_by_handle (p->write.handle);
            if (attr_obj) {
                // only write up to the maximum allowed size
                uint16_t write_len = p->write.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : p->write.len;
                memcpy(attr_obj->value, p->write.value, write_len);
                attr_obj->value_len = write_len;

                if (attr_obj->is_char) {    // characteristic
                    bt_gatts_char_obj_t *char_obj = (bt_gatts_char_obj_t *)attr_obj;
                    char_obj->events |= MOD_BT_GATTS_WRITE_EVT;
                    if (char_obj->trigger & MOD_BT_GATTS_WRITE_EVT) {

                        gatts_char_cbk_arg_t *cbk_arg = malloc(sizeof(gatts_char_cbk_arg_t));

                        cbk_arg->chr = char_obj;
                        cbk_arg->event = MOD_BT_GATTS_WRITE_EVT;
                        cbk_arg->data_length = write_len;
                        cbk_arg->data = malloc(cbk_arg->data_length);
                        memcpy(cbk_arg->data, p->write.value, cbk_arg->data_length);

                        mp_irq_queue_interrupt_non_ISR(gatts_char_callback_handler, cbk_arg);
                    }
                } else {    // descriptor
                    if (attr_obj->uuid.len == ESP_UUID_LEN_16 && attr_obj->uuid.uuid.uuid16 == GATT_UUID_CHAR_CLIENT_CONFIG) {
                        uint16_t value = param->write.value[1] << 8 | param->write.value[0];
                        bt_gatts_char_obj_t *char_obj = (bt_gatts_char_obj_t *)attr_obj->parent;
                        char_obj->config = value;
                        char_obj->events |= MOD_BT_GATTS_SUBSCRIBE_EVT;
                        if (char_obj->trigger & MOD_BT_GATTS_SUBSCRIBE_EVT) {

                            gatts_char_cbk_arg_t *cbk_arg = malloc(sizeof(gatts_char_cbk_arg_t));

                            cbk_arg->chr = char_obj;
                            cbk_arg->event = MOD_BT_GATTS_SUBSCRIBE_EVT;
                            cbk_arg->data_length = 0;
                            cbk_arg->data = NULL;

                            mp_irq_queue_interrupt_non_ISR(gatts_char_callback_handler, cbk_arg);
                        }

                        if (value == 0x0001) {  // notifications enabled
                            bt_gatts_char_obj_t *char_obj = (bt_gatts_char_obj_t *)attr_obj->parent;
                            // the size of value[] needs to be less than MTU size
                            esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, char_obj->attr_obj.handle, char_obj->attr_obj.value_len, char_obj->attr_obj.value, false);
                        }
                    }
                }
                esp_ble_gatts_send_response(gatts_if, p->write.conn_id, p->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        break;
    }
    case ESP_GATTS_MTU_EVT:
        bt_obj.gatts_mtu = p->mtu.mtu;
        xEventGroupSetBits(bt_event_group, MOD_BT_GATTS_MTU_EVT);
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.service_handle = p->create.service_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.char_handle = p->add_char.attr_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.char_descr_handle = p->add_char_descr.attr_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
        // only allow one connection at a time
        if (bt_obj.gatts_conn_id >= 0) {
            esp_ble_gatts_close(bt_obj.gatts_if, p->connect.conn_id);
        } else {
            memcpy((void *)bt_obj.client_bda, p->connect.remote_bda, ESP_BD_ADDR_LEN);
            bt_obj.gatts_conn_id = p->connect.conn_id;
            bt_obj.events |= MOD_BT_GATTS_CONN_EVT;
            if (bt_obj.trigger & MOD_BT_GATTS_CONN_EVT) {
                mp_irq_queue_interrupt_non_ISR(bluetooth_callback_handler, (void *)&bt_obj);
            }
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        bt_obj.gatts_conn_id = -1;
        xEventGroupClearBits(bt_event_group, MOD_BT_GATTS_MTU_EVT);
        if (bt_obj.advertising) {
            esp_ble_gap_start_advertising(&bt_adv_params);
        }
        bt_obj.events |= MOD_BT_GATTS_DISCONN_EVT;
        xEventGroupSetBits(bt_event_group, MOD_BT_GATTS_DISCONN_EVT);
        if (bt_obj.trigger & MOD_BT_GATTS_DISCONN_EVT) {
            mp_irq_queue_interrupt_non_ISR(bluetooth_callback_handler, (void *)&bt_obj);
        }
        break;
    case ESP_GATTS_CLOSE_EVT:
        xEventGroupSetBits(bt_event_group, MOD_BT_GATTS_CLOSE_EVT);
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
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
    if (!self->init) {

        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&bt_cfg);
        esp_bt_controller_enable(ESP_BT_MODE_BLE);

        if (ESP_OK != esp_bluedroid_init()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth init failed"));
        }
        if (ESP_OK != esp_bluedroid_enable()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth enable failed"));
        }

        esp_ble_gap_register_callback(gap_events_handler);
        esp_ble_gattc_register_callback(gattc_events_handler);
        esp_ble_gatts_register_callback(gatts_event_handler);

        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(btc_conn_list), 0);
        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_srv_list), 0);
        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_attr_list), 0);
        esp_ble_gattc_app_register(MOD_BT_CLIENT_APP_ID);
        esp_ble_gatts_app_register(MOD_BT_SERVER_APP_ID);

        //set MTU
        uint16_t mtu = args[6].u_int;
        if(mtu > BT_MTU_SIZE_MAX)
        {
            esp_ble_gatt_set_local_mtu(BT_MTU_SIZE_MAX);
            mod_bt_gatts_mtu_restore = BT_MTU_SIZE_MAX;
        }
        else
        {
            esp_ble_gatt_set_local_mtu(mtu);
            mod_bt_gatts_mtu_restore = mtu;
        }

        self->init = true;
    }

    // get the antenna type
    uint8_t antenna;
    if (args[1].u_obj == MP_OBJ_NULL) {
        // first gen module, so select the internal antenna
        if (micropy_hw_antenna_diversity_pin_num == MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM) {
            antenna = ANTENNA_TYPE_INTERNAL;
        } else {
            antenna = ANTENNA_TYPE_MANUAL;
        }
    } else if (args[1].u_obj == mp_const_none) {
        antenna = ANTENNA_TYPE_MANUAL;
    } else {
        antenna = mp_obj_get_int(args[1].u_obj);
    }
    antenna_validate_antenna(antenna);
    antenna_select(antenna);

    bt_obj.gatts_conn_id = -1;


    /* Set BLE modem sleep flag*/
    if (args[2].u_obj != MP_OBJ_NULL) {
        esp_err_t err = modem_sleep(mp_obj_is_true(args[2].u_obj));
        if(ESP_OK != err)
        {
            nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(err)));
        }
    }

    bt_obj.secure_connections = args[5].u_bool;

    if (args[3].u_obj != MP_OBJ_NULL){
        bt_obj.secure = true;

        // modified default advertisement parameters for secure
        bt_adv_params.adv_int_min = 0x100;
        bt_adv_params.adv_int_max = 0x100;
        bt_adv_params.own_addr_type = args[4].u_bool ? BLE_ADDR_TYPE_RANDOM : BLE_ADDR_TYPE_PUBLIC;

        set_pin(mp_obj_get_int(args[3].u_obj));
        bt_obj.privacy = args[4].u_bool;
        set_secure_parameters(bt_obj.secure_connections);
    } else {
        bt_obj.secure = false;
    }

    return mp_const_none;
}

STATIC const mp_arg_t bt_init_args[] = {
    { MP_QSTR_id,                                     MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,                 MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_BT_STACK_MODE_BLE} },
    { MP_QSTR_antenna,              MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
    { MP_QSTR_modem_sleep,          MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
    { MP_QSTR_pin,                  MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
    { MP_QSTR_privacy,              MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true} },
    { MP_QSTR_secure_connections,   MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true} },
    { MP_QSTR_mtu,                  MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = BT_MTU_SIZE_MAX} },

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

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    if (args[4].u_obj != MP_OBJ_NULL) {
       if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
          nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError,"Secure BLE not available for 512K RAM devices"));
       }
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

mp_obj_t bt_deinit(mp_obj_t self_in) {

    MP_THREAD_GIL_EXIT();
    modbt_deinit(false);
    MP_THREAD_GIL_ENTER();
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

static mp_obj_t modbt_start_scan(mp_obj_t timeout)
{
    return bt_start_scan(NULL, timeout);
}

STATIC mp_obj_t bt_isscanning(mp_obj_t self_in) {
    if (bt_obj.scanning) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_isscanning_obj, bt_isscanning);

STATIC mp_obj_t bt_modem_sleep(mp_uint_t n_args, const mp_obj_t *args) {

    bt_obj_t *self = args[0];
    /* Modem sleep APIs shall not be called before bt_controller_enable() */
    if(self->init)
    {
        if(n_args > 1)
        {
            if(ESP_OK != modem_sleep(mp_obj_is_true(args[1])))
            {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
        else
        {
            /* return modem sleep status */
            return mp_obj_new_bool(esp_bt_controller_is_sleeping());
        }
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "BLE module not initialized"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bt_modem_sleep_obj, 1, 2, bt_modem_sleep);

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
        tuple[4] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.ble_adv, sizeof(bt_event.scan.scan_rst.ble_adv));

        return mp_obj_new_attrtuple(bt_scan_info_fields, 5, tuple);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_read_scan_obj, bt_read_scan);

STATIC mp_obj_t bt_get_advertisements(mp_obj_t self_in) {
    bt_event_result_t bt_event;

    STATIC const qstr bt_scan_info_fields[] = {
        MP_QSTR_mac, MP_QSTR_addr_type, MP_QSTR_adv_type, MP_QSTR_rssi, MP_QSTR_data,
    };

    mp_obj_t advs = mp_obj_new_list(0, NULL);
    while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)0)) {
        mp_obj_t tuple[5];
        tuple[0] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.bda, 6);
        tuple[1] = mp_obj_new_int(bt_event.scan.scan_rst.ble_addr_type);
        tuple[2] = mp_obj_new_int(bt_event.scan.scan_rst.ble_evt_type & 0x03);    // FIXME
        tuple[3] = mp_obj_new_int(bt_event.scan.scan_rst.rssi);
        tuple[4] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.ble_adv, sizeof(bt_event.scan.scan_rst.ble_adv));

        mp_obj_list_append(advs, mp_obj_new_attrtuple(bt_scan_info_fields, 5, tuple));
    }
    return advs;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_get_advertisements_obj, bt_get_advertisements);

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
                return mp_obj_new_str((char *)data, data_len);
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
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    mp_irq_add(self, args[1].u_obj);

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

static mp_obj_t bt_connect_helper(mp_obj_t addr, TickType_t timeout, esp_ble_addr_type_t addr_type){

    bt_event_result_t bt_event;
    EventBits_t uxBits;

    if (bt_obj.busy) {
        // Only drop exception if not called from bt_resume() API, otherwise return with mp_const_none on error
        if(mod_bt_allow_resume_deinit == false) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
        }
        else {
            return mp_const_none;
        }
    }

    if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        mp_hal_delay_ms(50);
        bt_obj.scanning = false;
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(addr, &bufinfo, MP_BUFFER_READ);

    xQueueReset(xScanQueue);
    bt_obj.busy = true;

    /* Initiate a background connection, esp_ble_gattc_open returns immediately */
    if (ESP_OK != esp_ble_gattc_open(bt_obj.gattc_if, bufinfo.buf, addr_type, true)) {
        // Only drop exception if not called from bt_resume() API, otherwise return with mp_const_none on error
        if(mod_bt_allow_resume_deinit == false) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        else {
            return mp_const_none;
        }
    }

    MP_THREAD_GIL_EXIT();
    if (xQueueReceive(xScanQueue, &bt_event, timeout) == pdTRUE)
    {
        MP_THREAD_GIL_ENTER();

        if (bt_event.connection.conn_id < 0) {
            // Only drop exception if not called from bt_resume() API, otherwise return with mp_const_none on error
            if(mod_bt_allow_resume_deinit == false) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection refused"));
            }
            else {
                return mp_const_none;
            }
        }

        // setup the object
        bt_connection_obj_t *conn = m_new_obj(bt_connection_obj_t);
        conn->base.type = (mp_obj_t)&mod_bt_connection_type;
        conn->conn_id = bt_event.connection.conn_id;
        conn->gatt_if = bt_event.connection.gatt_if;
        conn->addr_type = addr_type;

        MP_THREAD_GIL_EXIT();
        uxBits = xEventGroupWaitBits(bt_event_group, MOD_BT_GATTC_MTU_EVT, true, true, 1000/portTICK_PERIOD_MS);
        MP_THREAD_GIL_ENTER();

        if(uxBits & MOD_BT_GATTC_MTU_EVT)
        {
            conn->mtu = bt_conn_mtu;
        }
        memcpy(conn->srv_bda, bt_event.connection.srv_bda, 6);
        mp_obj_list_append((void *)&MP_STATE_PORT(btc_conn_list), conn);

        return conn;
    }
    else
    {
        MP_THREAD_GIL_ENTER();

        (void)esp_ble_gap_disconnect(bufinfo.buf);

        // Only drop exception if not called from bt_resume() API, otherwise return with mp_const_none on error
        if(mod_bt_allow_resume_deinit == false) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
        }
        else {
            return mp_const_none;
        }
    }
    return mp_const_none;
}


STATIC mp_obj_t bt_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_addr,         MP_ARG_REQUIRED | MP_ARG_OBJ,   },
            { MP_QSTR_timeout,      MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj = MP_OBJ_NULL} },
            { MP_QSTR_addr_type,    MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj = MP_OBJ_NULL}},
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    mp_obj_t addr = args[0].u_obj;

    /* Timeout parameter is in milliseconds */
    TickType_t timeout;
    if(args[1].u_obj == MP_OBJ_NULL){
        timeout = portMAX_DELAY;
    }
    else
    {
        if(MP_OBJ_IS_SMALL_INT(args[1].u_obj) == true) {
            timeout = mp_obj_get_int(args[1].u_obj);
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "If timeout is specified it must be a valid integer number"));
        }
    }

    /* addr_type parameter */
    uint32_t addr_type;
    if(args[2].u_obj == MP_OBJ_NULL){
        addr_type = BLE_ADDR_TYPE_PUBLIC;
    }
    else
    {
        if(MP_OBJ_IS_SMALL_INT(args[2].u_obj) == true) {
            addr_type = mp_obj_get_int(args[2].u_obj);
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "If addr_type is specified it must be a valid integer number"));
        }
    }


    return bt_connect_helper(addr, timeout, addr_type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_connect_obj, 1, bt_connect);

static mp_obj_t modbt_connect(mp_obj_t addr, esp_ble_addr_type_t addr_type)
{
    return bt_connect_helper(addr, portMAX_DELAY, addr_type);
}


STATIC mp_obj_t bt_set_advertisement_params (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_adv_int_min,              MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_adv_int_max,              MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_adv_type,                 MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_own_addr_type,            MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_channel_map,              MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_adv_filter_policy,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // adv_int_min
    if (args[0].u_obj != MP_OBJ_NULL) {
        bt_adv_params.adv_int_min = (uint16_t) mp_obj_get_int(args[0].u_obj);
    }

    // adv_int_max
    if (args[1].u_obj != MP_OBJ_NULL) {
        bt_adv_params.adv_int_max = (uint16_t) mp_obj_get_int(args[1].u_obj);
    }

    // adv_type
    if (args[2].u_obj != MP_OBJ_NULL) {
        bt_adv_params.adv_type = (esp_ble_adv_type_t) mp_obj_get_int(args[2].u_obj);
    }

    // own_addr_type
    if (args[3].u_obj != MP_OBJ_NULL) {
        bt_adv_params.own_addr_type = (esp_ble_addr_type_t) mp_obj_get_int(args[3].u_obj);
    }

    // channel_map
    if (args[4].u_obj != MP_OBJ_NULL) {
        bt_adv_params.channel_map = (esp_ble_adv_channel_t) mp_obj_get_int(args[4].u_obj);
    }

    // adv_filter_policy
    if (args[5].u_obj != MP_OBJ_NULL) {
        bt_adv_params.adv_filter_policy = (esp_ble_adv_filter_t) mp_obj_get_int(args[5].u_obj);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_set_advertisement_params_obj, 1, bt_set_advertisement_params);


STATIC mp_obj_t bt_set_advertisement (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_name,                     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_manufacturer_data,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_data,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_uuid,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

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
        esp_ble_gap_set_device_name(" ");
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
        if (mp_obj_is_integer(args[3].u_obj)) {
            uint32_t srv_uuid = mp_obj_get_int_truncated(args[3].u_obj);
            uint8_t uuid_buf[16] = {0};
            memcpy(uuid_buf, (uint8_t *)&srv_uuid, sizeof(uuid_buf));
            adv_data.service_uuid_len = 16;
            adv_data.p_service_uuid = (uint8_t *)&srv_uuid;
        } else {
            mp_get_buffer_raise(args[3].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
            adv_data.service_uuid_len = uuid_bufinfo.len;
            adv_data.p_service_uuid = uuid_bufinfo.buf;
            if (adv_data.service_uuid_len % 16) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "incorrect service UUID length"));
            }
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
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    // copy all the info to the scan response
    memcpy(&scan_rsp_data, &adv_data, sizeof(esp_ble_adv_data_t));
    scan_rsp_data.set_scan_rsp = true;
    // do not include the name or the tx power in the scan response
    scan_rsp_data.include_name = false;
    scan_rsp_data.include_txpower = false;
    // do not include the service uuid or service data in the advertisement, only in the scan response
    adv_data.manufacturer_len = 0;
    adv_data.p_manufacturer_data =  NULL;
    adv_data.service_data_len = 0;
    adv_data.p_service_data = NULL;
    adv_data.service_uuid_len = 0;
    adv_data.p_service_uuid = NULL;
    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_config_adv_data(&scan_rsp_data);

    // wait for the advertisement data to be configured
    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_set_advertisement_obj, 1, bt_set_advertisement);


STATIC mp_obj_t bt_set_advertisement_raw(mp_obj_t self_in, mp_obj_t raw_data) {
    mp_buffer_info_t bufinfo;
    uint32_t data_len;
    uint8_t  data[30] = {0};

    if (raw_data != mp_const_none) {
        mp_get_buffer_raise(raw_data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len < 31) {
            memcpy(data, (uint8_t *)bufinfo.buf, bufinfo.len);
            data_len = bufinfo.len;
        } else {
            memcpy(data, (uint8_t *)bufinfo.buf, sizeof(data));
            data_len = sizeof(data);
        }

        esp_ble_gap_config_adv_data_raw(data, data_len);

        // wait for the advertisement data to be configured
        bt_gatts_event_result_t gatts_event;
        xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_set_advertisement_raw_obj, bt_set_advertisement_raw);


STATIC mp_obj_t bt_set_pin(mp_obj_t self_in, mp_obj_t arg) {
    set_pin(mp_obj_get_int(arg));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_set_pin_obj, bt_set_pin);


STATIC mp_obj_t bt_advertise(mp_obj_t self_in, mp_obj_t enable) {
    if (mp_obj_is_true(enable)) {
        // some sensible time to wait for the advertisement configuration to complete
        mp_hal_delay_ms(50);
        esp_ble_gap_start_advertising(&bt_adv_params);
        bt_obj.advertising = true;
    } else {
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
        { MP_QSTR_start,                    MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_buffer_info_t uuid_bufinfo;
    esp_gatt_srvc_id_t service_id;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // service uuid
    if (mp_obj_is_integer(args[0].u_obj)) {
        uint32_t srv_uuid = mp_obj_get_int_truncated(args[0].u_obj);
        if (srv_uuid > UINT16_MAX) {
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

    if (args[3].u_bool) {
        esp_ble_gatts_start_service(gatts_event.service_handle);
        srv->started = true;
    }

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_srv_list), srv);

    return srv;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_service_obj, 1, bt_service);

STATIC mp_obj_t bt_service_start(mp_obj_t self_in) {
    bt_gatts_srv_obj_t *self = self_in;
    if (!self->started) {
        if (ESP_OK != esp_ble_gatts_start_service(self->handle)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        self->started = true;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_service_start_obj, bt_service_start);

STATIC mp_obj_t bt_service_stop(mp_obj_t self_in) {
    bt_gatts_srv_obj_t *self = self_in;
    if (self->started) {
        if (ESP_OK != esp_ble_gatts_stop_service(self->handle)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        self->started = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_service_stop_obj, bt_service_stop);

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
    if (mp_obj_is_integer(args[0].u_obj)) {
        uint32_t srv_uuid = mp_obj_get_int_truncated(args[0].u_obj);
        if (srv_uuid > UINT16_MAX) {
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

    uint32_t permissions = 0;
    if (args[1].u_obj != mp_const_none) {
        permissions = mp_obj_get_int(args[1].u_obj);
    } else {
        if (!bt_obj.secure){
            permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
        } else {
            permissions = ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED;
        }
    }

    uint32_t properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    if (args[2].u_obj != mp_const_none) {
        properties = mp_obj_get_int(args[2].u_obj);
    }

    bt_gatts_char_obj_t *characteristic = m_new_obj(bt_gatts_char_obj_t);
    characteristic->attr_obj.base.type = (mp_obj_t)&mod_bt_gatts_char_type;
    characteristic->attr_obj.parent = self;
    characteristic->attr_obj.is_char = true;
    characteristic->attr_obj.properties = properties;
    characteristic->trigger = 0;
    characteristic->events = 0;

    if (args[3].u_obj != mp_const_none) {
        // characteristic value
        if (mp_obj_is_integer(args[3].u_obj)) {
            uint32_t value = mp_obj_get_int_truncated(args[3].u_obj);
            if (value > UINT16_MAX) {
                characteristic->attr_obj.value_len = 4;
                value = lwip_htonl(value);
            } else if (value > UINT8_MAX) {
                characteristic->attr_obj.value_len = 2;
                value = lwip_htons(value);
            } else {
                characteristic->attr_obj.value_len = 1;
            }
            memcpy(characteristic->attr_obj.value, &value, sizeof(value));
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[3].u_obj, &value_bufinfo, MP_BUFFER_READ);
            uint16_t write_len = value_bufinfo.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : value_bufinfo.len;
            memcpy(characteristic->attr_obj.value, value_bufinfo.buf, write_len);
            characteristic->attr_obj.value_len = write_len;
        }
    } else {
        characteristic->attr_obj.value[0] = 0;
        characteristic->attr_obj.value_len = 1;
    }

    esp_ble_gatts_add_char(self->handle, &char_uuid, permissions, properties, NULL, NULL);

    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    characteristic->attr_obj.handle = gatts_event.char_handle;
    memcpy(&characteristic->attr_obj.uuid, &char_uuid, sizeof(char_uuid));

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_attr_list), characteristic);

    bt_gatts_attr_obj_t *descriptor = m_new_obj(bt_gatts_attr_obj_t);
    descriptor->uuid.len = ESP_UUID_LEN_16;
    descriptor->uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

    esp_ble_gatts_add_char_descr(self->handle, &descriptor->uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);

    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    descriptor->base.type = (mp_obj_t)&mod_bt_gatts_char_type;
    descriptor->handle = gatts_event.char_descr_handle;
    descriptor->parent = characteristic;
    descriptor->is_char = false;
    descriptor->value_len = 2;
    descriptor->value[0] = 0;
    descriptor->value[1] = 0;

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_attr_list), descriptor);

    return characteristic;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_characteristic_obj, 1, bt_characteristic);


STATIC const mp_map_elem_t bt_gatts_service_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_characteristic),          (mp_obj_t)&bt_characteristic_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start),                   (mp_obj_t)&bt_service_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop),                    (mp_obj_t)&bt_service_stop_obj },
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
        return mp_obj_new_bytes(self->attr_obj.value, self->attr_obj.value_len);
    } else {
        // set
        if (mp_obj_is_integer(args[1])) {
            uint32_t value = mp_obj_get_int_truncated(args[1]);
            memcpy(self->attr_obj.value, &value, sizeof(value));
            if (value > 0xFF) {
                self->attr_obj.value_len = 2;
            } else if (value > 0xFFFF) {
                self->attr_obj.value_len = 4;
            } else {
                self->attr_obj.value_len = 1;
            }
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[1], &value_bufinfo, MP_BUFFER_READ);
            uint8_t value_len = value_bufinfo.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : value_bufinfo.len;
            memcpy(self->attr_obj.value, value_bufinfo.buf, value_len);
            self->attr_obj.value_len = value_len;
        }

        bool confirm = self->attr_obj.properties & ESP_GATT_CHAR_PROP_BIT_INDICATE;
        if (ESP_OK != esp_ble_gatts_send_indicate(bt_obj.gatts_if, bt_obj.gatts_conn_id, self->attr_obj.handle,
                                                  self->attr_obj.value_len, self->attr_obj.value, confirm)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Erorr while sending BLE indication/notification"));
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
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    mp_irq_add(self, args[1].u_obj);

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

STATIC mp_obj_t bt_characteristic_config(mp_obj_t self_in) {
    bt_gatts_char_obj_t *self = self_in;
    return mp_obj_new_int(self->config);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_characteristic_config_obj, bt_characteristic_config);

STATIC const mp_map_elem_t bt_gatts_char_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),          (mp_obj_t)&bt_characteristic_value_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),       (mp_obj_t)&bt_characteristic_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),         (mp_obj_t)&bt_characteristic_events_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_config),         (mp_obj_t)&bt_characteristic_config_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_gatts_char_locals_dict, bt_gatts_char_locals_dict_table);

static const mp_obj_type_t mod_bt_gatts_char_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSCharacteristic,
    .locals_dict = (mp_obj_t)&bt_gatts_char_locals_dict,
};

STATIC mp_obj_t bt_gatts_disconnect_client(mp_obj_t self_in) {
    if (bt_obj.gatts_conn_id >= 0) {
        esp_ble_gatts_close(bt_obj.gatts_if, bt_obj.gatts_conn_id);
        esp_ble_gap_disconnect((void *)bt_obj.client_bda);
        bt_obj.gatts_conn_id = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_gatts_disconnect_client_obj, bt_gatts_disconnect_client);

STATIC mp_obj_t bt_tx_power(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t allowed_args[] = {
            { MP_QSTR_type,      MP_ARG_REQUIRED | MP_ARG_INT,   {.u_obj = mp_const_none} },
            { MP_QSTR_level,     MP_ARG_INT,                     {.u_obj = mp_const_none} }
        };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    mp_int_t type = args[0].u_int;

     // Do not accept "Connection Handlers 1-8", we do not support different connections in parallel
    if(type == ESP_BLE_PWR_TYPE_CONN_HDL0 ||
       (type > ESP_BLE_PWR_TYPE_CONN_HDL8 && type  < ESP_BLE_PWR_TYPE_NUM)) {

        // If "level" is not specified, return with the TX Power marked in "type" parameter
        if(args[1].u_obj == mp_const_none)
        {
            esp_power_level_t ret = esp_ble_tx_power_get(type);
            if(ret < 0) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "TX Power level could not be get, error code: %d", ret));
                // Just for the compiler
                return mp_const_none;
            }
            else {
                return mp_obj_new_int(tx_pwr_level_to_dbm[ret]);
            }
        }
        else {

            mp_int_t level = args[1].u_int;

            if(level >= (sizeof(tx_pwr_level_to_dbm)/sizeof(tx_pwr_level_to_dbm[0]))) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid value as \"level\", must be between 0-7: %d", type));
            }

            esp_power_level_t ret = esp_ble_tx_power_set(type, level);

            if(ret != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "TX Power level could not be set, error code: %d", ret));
                // Just for the compiler
                return mp_const_none;
            }
            else {
                return mp_const_none;
            }
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid value as \"type\": %d", type));
        // Just for the compiler
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_tx_power_obj, 2, bt_tx_power);

STATIC mp_obj_t bt_conn_get_mtu(mp_obj_t self_in) {

    bt_connection_obj_t * self =  (bt_connection_obj_t *)self_in;

    if(self->conn_id >= 0)
    {
        return mp_obj_new_int(self->mtu);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_get_mtu_obj, bt_conn_get_mtu);

STATIC mp_obj_t bt_gatts_get_mtu(mp_obj_t self_in) {

    bt_obj_t * self =  (bt_obj_t *)self_in;
    EventBits_t uxBits;

    if(self->gatts_conn_id >= 0)
    {
        MP_THREAD_GIL_EXIT();
        uxBits = xEventGroupWaitBits(bt_event_group, MOD_BT_GATTS_MTU_EVT, true, true, 1000/portTICK_PERIOD_MS);
        MP_THREAD_GIL_ENTER();
        if(uxBits & MOD_BT_GATTS_MTU_EVT)
        {
            return mp_obj_new_int(self->gatts_mtu);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_gatts_get_mtu_obj, bt_gatts_get_mtu);

STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                  (mp_obj_t)&bt_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start_scan),              (mp_obj_t)&bt_start_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isscanning),              (mp_obj_t)&bt_isscanning_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop_scan),               (mp_obj_t)&bt_stop_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_adv),                 (mp_obj_t)&bt_read_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_advertisements),      (mp_obj_t)&bt_get_advertisements_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_resolve_adv_data),        (mp_obj_t)&bt_resolve_adv_data_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),                 (mp_obj_t)&bt_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_advertisement_params),(mp_obj_t)&bt_set_advertisement_params_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_advertisement),       (mp_obj_t)&bt_set_advertisement_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_advertisement_raw),   (mp_obj_t)&bt_set_advertisement_raw_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_pin),                 (mp_obj_t)&bt_set_pin_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_advertise),               (mp_obj_t)&bt_advertise_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_service),                 (mp_obj_t)&bt_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&bt_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),                  (mp_obj_t)&bt_events_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect_client),       (mp_obj_t)&bt_gatts_disconnect_client_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_modem_sleep),             (mp_obj_t)&bt_modem_sleep_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tx_power),                (mp_obj_t)&bt_tx_power_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_gatts_mtu),               (mp_obj_t)&bt_gatts_get_mtu_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvram_erase),             (mp_obj_t)&bt_nvram_erase_obj },


    // exceptions
    { MP_OBJ_NEW_QSTR(MP_QSTR_timeout),                 (mp_obj_t)&mp_type_TimeoutError },

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

    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_READ),               MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_READ) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_READ_ENCRYPTED),     MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_READ_ENCRYPTED) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_READ_ENC_MITM),      MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_READ_ENC_MITM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_WRITE),              MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_WRITE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_WRITE_ENCRYPTED),    MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_WRITE_ENCRYPTED) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_WRITE_ENC_MITM),     MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_WRITE_ENC_MITM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_WRITE_SIGNED),       MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_WRITE_SIGNED) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PERM_WRITE_SIGNED_MITM),  MP_OBJ_NEW_SMALL_INT(ESP_GATT_PERM_WRITE_SIGNED_MITM) },

    // Defined at https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_CONFIG_NOTIFY),      MP_OBJ_NEW_SMALL_INT(1 << 0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_CONFIG_INDICATE),    MP_OBJ_NEW_SMALL_INT(1 << 1) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_NEW_ADV_EVENT),           MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_ADV_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_CONNECTED),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_CONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_DISCONNECTED),     MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_DISCONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_READ_EVENT),         MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_READ_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_WRITE_EVENT),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_WRITE_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_NOTIFY_EVENT),       MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_NOTIFY_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_SUBSCRIBE_EVENT),    MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_SUBSCRIBE_EVT) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_INDICATE_EVENT),     MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_INDICATE_EVT) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_MAN_ANT),                 MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_MANUAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_INT_ANT),                 MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_INTERNAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EXT_ANT),                 MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_EXTERNAL) },

    // Constants for bt_set_advertisement_params API
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TYPE_IND),                         MP_OBJ_NEW_SMALL_INT(ADV_TYPE_IND) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_HIGH),             MP_OBJ_NEW_SMALL_INT(ADV_TYPE_DIRECT_IND_HIGH) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TYPE_SCAN_IND),                    MP_OBJ_NEW_SMALL_INT(ADV_TYPE_SCAN_IND) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TYPE_NONCONN_IND),                 MP_OBJ_NEW_SMALL_INT(ADV_TYPE_NONCONN_IND) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_LOW),              MP_OBJ_NEW_SMALL_INT(ADV_TYPE_DIRECT_IND_LOW) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_BLE_ADDR_TYPE_PUBLIC),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_BLE_ADDR_TYPE_RANDOM),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RANDOM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_BLE_ADDR_TYPE_RPA_PUBLIC),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_BLE_ADDR_TYPE_RPA_RANDOM),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_RANDOM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_CHNL_37),                          MP_OBJ_NEW_SMALL_INT(ADV_CHNL_37) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_CHNL_38),                          MP_OBJ_NEW_SMALL_INT(ADV_CHNL_38) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_CHNL_39),                          MP_OBJ_NEW_SMALL_INT(ADV_CHNL_39) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_CHNL_ALL),                         MP_OBJ_NEW_SMALL_INT(ADV_CHNL_ALL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY),    MP_OBJ_NEW_SMALL_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY),   MP_OBJ_NEW_SMALL_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST),   MP_OBJ_NEW_SMALL_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST),  MP_OBJ_NEW_SMALL_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST) },

    // Constants for setting TX Power
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_CONN),             MP_OBJ_NEW_SMALL_INT(ESP_BLE_PWR_TYPE_CONN_HDL0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_ADV),              MP_OBJ_NEW_SMALL_INT(ESP_BLE_PWR_TYPE_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_SCAN),             MP_OBJ_NEW_SMALL_INT(ESP_BLE_PWR_TYPE_SCAN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_DEFAULT),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_PWR_TYPE_DEFAULT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_N12),              MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_N12) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_N9),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_N9) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_N6),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_N6) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_N3),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_N3) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_0),                MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_N0) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_P3),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_P3) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_P6),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_P6) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PWR_P9),               MP_OBJ_NEW_SMALL_INT(ESP_PWR_LVL_P9) },

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

    if (self->conn_id >= 0) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_isconnected_obj, bt_conn_isconnected);

STATIC mp_obj_t bt_conn_disconnect(mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;

    if (self->conn_id >= 0) {
        esp_ble_gattc_close(bt_obj.gattc_if, self->conn_id);
        esp_ble_gap_disconnect(self->srv_bda);
        /* Only reset Conn Id if it is needed that the connection should be established again after wakeup
         * otherwise this connection will be completely removed in close_connection() call triggered by ESP_GATTC_DISCONNECT_EVT event */
        if(mod_bt_allow_resume_deinit)
        {
            self->conn_id = -1;
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_disconnect_obj, bt_conn_disconnect);

static mp_obj_t modbt_conn_disconnect(mp_obj_t self_in)
{
    return bt_conn_disconnect(self_in);
}

STATIC mp_obj_t bt_conn_services (mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->conn_id >= 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        mp_obj_list_init(&self->srv_list, 0);

        if (ESP_OK != esp_ble_gattc_search_service(bt_obj.gattc_if, self->conn_id, NULL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }

        while (bt_obj.busy) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)0)) {
                bt_srv_obj_t *srv = m_new_obj(bt_srv_obj_t);
                srv->base.type = (mp_obj_t)&mod_bt_service_type;
                srv->connection = self;
                memcpy(&srv->srv_id, &bt_event.service.srv_id, sizeof(esp_gatt_id_t));
                srv->start_handle = bt_event.service.start_handle;
                srv->end_handle = bt_event.service.end_handle;
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
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_mtu),                 (mp_obj_t)&bt_conn_get_mtu_obj },

};
STATIC MP_DEFINE_CONST_DICT(bt_connection_locals_dict, bt_connection_locals_dict_table);

static const mp_obj_type_t mod_bt_connection_type = {
     { &mp_type_type },
    .name = MP_QSTR_GATTCConnection,
    .locals_dict = (mp_obj_t)&bt_connection_locals_dict,
};

STATIC mp_obj_t bt_srv_isprimary(mp_obj_t self_in) {
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_isprimary_obj, bt_srv_isprimary);

STATIC mp_obj_t bt_srv_uuid(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->srv_id.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->srv_id.uuid.uuid.uuid16);
    } else if (self->srv_id.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->srv_id.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->srv_id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_uuid_obj, bt_srv_uuid);

STATIC mp_obj_t bt_srv_instance(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;
    return mp_obj_new_int(self->srv_id.inst_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_instance_obj, bt_srv_instance);

STATIC mp_obj_t bt_srv_characteristics(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->connection->conn_id >= 0) {
        mp_obj_list_init(&self->char_list, 0);


        uint16_t attr_count  = 0;
        esp_ble_gattc_get_attr_count(bt_obj.gattc_if,
                                     self->connection->conn_id,
                                     ESP_GATT_DB_CHARACTERISTIC,
                                     self->start_handle,
                                     self->end_handle,
                                     0,
                                     &attr_count);

        if (attr_count > 0) {
            esp_gattc_char_elem_t *char_elems = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * attr_count);
            if (!char_elems) {
                mp_raise_OSError(MP_ENOMEM);
            } else {
                esp_ble_gattc_get_all_char(bt_obj.gattc_if,
                                           self->connection->conn_id,
                                           self->start_handle,
                                           self->end_handle,
                                           char_elems,
                                           &attr_count,
                                           0);
                if (attr_count > 0) {
                    for (int i = 0; i < attr_count; ++i) {
                        bt_char_obj_t *chr = m_new_obj(bt_char_obj_t);
                        chr->base.type = (mp_obj_t)&mod_bt_characteristic_type;
                        chr->service = self;
                        memcpy(&chr->characteristic, &char_elems[i], sizeof(esp_gattc_char_elem_t));
                        chr->value[0] = 0;
                        chr->value_len = 1;
                        mp_obj_list_append(&self->char_list, chr);
                    }
                }
                free(char_elems);
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

    if (self->characteristic.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->characteristic.uuid.uuid.uuid16);
    } else if (self->characteristic.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->characteristic.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->characteristic.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_uuid_obj, bt_char_uuid);

STATIC mp_obj_t bt_char_instance(mp_obj_t self_in) {
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_instance_obj, bt_char_instance);

STATIC mp_obj_t bt_char_properties(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_int(self->characteristic.properties);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_properties_obj, bt_char_properties);

STATIC mp_obj_t bt_char_read(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->service->connection->conn_id >= 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;

        if (ESP_OK != esp_ble_gattc_read_char (bt_obj.gattc_if, self->service->connection->conn_id,
                                               self->characteristic.char_handle,
                                               ESP_GATT_AUTH_REQ_NONE)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        while (bt_obj.busy) {
            mp_hal_delay_ms(5);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            memcpy(self->value, bt_event.read.value, bt_event.read.value_len);
            self->value_len = bt_event.read.value_len;
            return mp_obj_new_bytes(bt_event.read.value, bt_event.read.value_len);
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_read_obj, bt_char_read);

STATIC mp_obj_t bt_char_read_descriptor(mp_obj_t self_in, mp_obj_t uuid) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    uint16_t descr_uuid_value = mp_obj_get_int(uuid);

    if (self->service->connection->conn_id >= 0) {
        xQueueReset(xScanQueue);

        esp_gattc_descr_elem_t format_descriptor;
        uint16_t count = 1;
        esp_bt_uuid_t descr_uuid = {.len = ESP_UUID_LEN_16, .uuid.uuid16 = descr_uuid_value};
        esp_gatt_status_t ret_val = esp_ble_gattc_get_descr_by_uuid(bt_obj.gattc_if,
                                                                    self->service->connection->conn_id,
                                                                    self->service->start_handle,
                                                                    self->service->end_handle,
                                                                    self->characteristic.uuid,
                                                                    descr_uuid,
                                                                    &format_descriptor,
                                                                    &count);
        if(ret_val == ESP_OK && count == 1) {
            bt_obj.busy = true;

            if (ESP_OK != esp_ble_gattc_read_char_descr(bt_obj.gattc_if,
                                                        self->service->connection->conn_id,
                                                        format_descriptor.handle,
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
            return mp_const_none; // Descriptor not found, no read
        }
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_char_read_descriptor_obj, bt_char_read_descriptor);

STATIC mp_obj_t bt_char_write(mp_obj_t self_in, mp_obj_t value) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(value, &bufinfo, MP_BUFFER_READ);

    if (self->service->connection->conn_id >= 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;

        if (ESP_OK != esp_ble_gattc_write_char (bt_obj.gattc_if, self->service->connection->conn_id,
                                                self->characteristic.char_handle,
                                                bufinfo.len,
                                                bufinfo.buf,
                                                ESP_GATT_WRITE_TYPE_RSP,
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
    bt_event_result_t bt_event;

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        uint32_t trigger = mp_obj_get_int(args[0].u_obj);
        if (trigger != MOD_BT_GATTC_NOTIFY_EVT) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid trigger"));
        }
        self->trigger = trigger;
        self->handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }

        if (self->service->connection->conn_id >= 0) {
            if (ESP_OK != esp_ble_gattc_register_for_notify (self->service->connection->gatt_if,
                                                             self->service->connection->srv_bda,
                                                             self->characteristic.char_handle)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }

            if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)(2500 / portTICK_RATE_MS))) {
                if (bt_event.register_for_notify.status != ESP_GATT_OK) {
                    goto error;
                }
            } else {
                goto error;
            }

            uint16_t attr_count = 0;
            uint16_t notify_en = 1;
            esp_ble_gattc_get_attr_count(bt_obj.gattc_if,
                                         self->service->connection->conn_id,
                                         ESP_GATT_DB_DESCRIPTOR,
                                         self->service->start_handle,
                                         self->service->end_handle,
                                         self->characteristic.char_handle,
                                         &attr_count);
            if (attr_count > 0) {
                esp_gattc_descr_elem_t *descr_elems = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * attr_count);
                if (!descr_elems) {
                    mp_raise_OSError(MP_ENOMEM);
                } else {
                    esp_ble_gattc_get_all_descr(bt_obj.gattc_if,
                                                self->service->connection->conn_id,
                                                self->characteristic.char_handle,
                                                descr_elems,
                                                &attr_count,
                                                0);
                    for (int i = 0; i < attr_count; ++i) {
                        if (descr_elems[i].uuid.len == ESP_UUID_LEN_16 && descr_elems[i].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                            esp_ble_gattc_write_char_descr (bt_obj.gattc_if,
                                                            self->service->connection->conn_id,
                                                            descr_elems[i].handle,
                                                            sizeof(notify_en),
                                                            (uint8_t *)&notify_en,
                                                            ESP_GATT_WRITE_TYPE_RSP,
                                                            ESP_GATT_AUTH_REQ_NONE);

                            break;
                        }
                    }
                    free(descr_elems);
                }
            }
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
        }
    } else {
        self->trigger = 0;
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    mp_irq_add(self, args[1].u_obj);

    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_char_callback_obj, 1, bt_char_callback);

STATIC mp_obj_t bt_char_value(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_bytes(self->value, self->value_len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_value_obj, bt_char_value);

STATIC const mp_map_elem_t bt_characteristic_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_char_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_char_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_properties),              (mp_obj_t)&bt_char_properties_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                    (mp_obj_t)&bt_char_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read_descriptor),         (mp_obj_t)&bt_char_read_descriptor_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),                   (mp_obj_t)&bt_char_write_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&bt_char_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),                   (mp_obj_t)&bt_char_value_obj },
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
