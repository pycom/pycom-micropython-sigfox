/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/gc.h"
#include <string.h>

#include "esp_ble_mesh_common_api.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/


/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct mod_ble_mesh_obj_t {
    mp_obj_base_t base;
}mod_ble_mesh_obj_t;
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
static bool initialized = false;
static esp_ble_mesh_prov_t *provision_ptr;
static esp_ble_mesh_comp_t *composition_ptr;


// TODO: this should be automatically configured in mod_ble_mesh_init function
static const esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

// TODO: this should be automatically created when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_model()
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);
static esp_ble_mesh_client_t onoff_client;

// TODO: this should be automatically created when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_model()
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);
static esp_ble_mesh_client_t onoff_server;

// TODO: this should be automatically extended when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_model()
static esp_ble_mesh_model_t root_models_server[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_server),
};

// TODO: one root model is enough, this is just for basic testing
static esp_ble_mesh_model_t root_models_client[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
};

// TODO: this should be automatically created when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_element()
static esp_ble_mesh_elem_t elements[1];
// TODO: this should be automatically created when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_element()
static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE BLE MESH CLASS FUNCTIONS
 ******************************************************************************/

// TODO: add parameters
// Initialize the module
STATIC mp_obj_t mod_ble_mesh_init(mp_int_t type) {

    // The BLE Mesh module should be initialized only once
    if(initialized == false) {
        // For now type = 0 means the device is initialized with onoff_server
        if(type == 0) {
            elements[0].location         = 0,
            elements[0].sig_model_count  = sizeof(root_models_server)/sizeof(esp_ble_mesh_model_t);
            elements[0].sig_models       = root_models_server;
            elements[0].vnd_model_count  = 0;
            elements[0].vnd_models       = ESP_BLE_MESH_MODEL_NONE;
        }
        // For now type != 0 means the device is initialized with onoff_client
        else {
            elements[0].location         = 0,
            elements[0].sig_model_count  = sizeof(root_models_client)/sizeof(esp_ble_mesh_model_t);
            elements[0].sig_models       = root_models_client;
            elements[0].vnd_model_count  = 0;
            elements[0].vnd_models       = ESP_BLE_MESH_MODEL_NONE;
        }

        provision_ptr = (esp_ble_mesh_prov_t *)heap_caps_malloc(sizeof(esp_ble_mesh_prov_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        composition_ptr = (esp_ble_mesh_comp_t *)heap_caps_malloc(sizeof(esp_ble_mesh_comp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        //TODO: initialize composition based on input parameters
       composition->cid = 0x02C4;  // CID_ESP=0x02C4
       composition->elements = elements;
       composition->element_count = ARRAY_SIZE(elements);
       //TODO: initialize provision based on input parameters
       /* Disable OOB security for SILabs Android app */
       provision_ptr->uuid = dev_uuid;
       provision_ptr->output_size = 0;
       provision_ptr->output_actions = 0;


        if(provision_ptr != NULL && composition_ptr != NULL) {
            esp_err_t err = esp_ble_mesh_init(provision_ptr, composition_ptr);
            if(err != ESP_OK) {
                // TODO: drop back the error code
                nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "BLE Mesh module could not be initialized!"));
            }
            else {
                initialized = true;
            }
        }
        else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "BLE Mesh module could not be initialized!"));
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "BLE Mesh module is already initialized!"));
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_ble_mesh_init_obj, mod_ble_mesh_init);


STATIC const mp_map_elem_t ble_mesh_locals_dict_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_ble_mesh_init_obj },
};

STATIC MP_DEFINE_CONST_DICT(ble_mesh_locals_dict, ble_mesh_locals_dict_table);

// Sub-module of Bluetooth module (modbt.c)
const mp_obj_type_t mod_ble_mesh = {
    { &mp_type_type },
    .name = MP_QSTR_BLE_Mesh,
    .locals_dict = (mp_obj_t)&ble_mesh_locals_dict,
};
