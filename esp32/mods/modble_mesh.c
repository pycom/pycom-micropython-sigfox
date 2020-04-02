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

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MOD_BLE_MESH_SERVER         (0)
#define MOD_BLE_MESH_CLIENT         (1)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/

// List type for the models
typedef struct mod_ble_mesh_models_list_s {
    struct mod_ble_mesh_models_list_s* next;
    esp_ble_mesh_model_t model;
}mod_ble_mesh_models_list_t;

// List tipe for the elements
typedef struct mod_ble_mesh_elements_list_s {
    struct mod_ble_mesh_elements_list_s* next;
    esp_ble_mesh_elem_t* element;
    mod_ble_mesh_models_list_t* models_list;
}mod_ble_mesh_elements_list_t;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
static mod_ble_mesh_elements_list_t mod_ble_mesh_elements = {NULL, {0, 0, 0, NULL, NULL}, NULL};
static bool initialized = false;
static esp_ble_mesh_prov_t *provision_ptr;
static esp_ble_mesh_comp_t *composition_ptr;

// TODO: this should be automatically configured in mod_ble_mesh_init() function
static esp_ble_mesh_cfg_srv_t configuration_server_model = {
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
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_srv_pub, 2 + 1, ROLE_NODE);
static esp_ble_mesh_client_t onoff_server;

// TODO: this should be automatically extended when the relevant MicroPython API is called, e.g.: mod_ble_mesh_add_model()
static esp_ble_mesh_model_t root_models_server[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&configuration_server_model),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_srv_pub, &onoff_server),
};

// TODO: one root model is enough, this is just for basic testing
static esp_ble_mesh_model_t root_models_client[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&configuration_server_model),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client),
};



/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE BLE MESH CLASS FUNCTIONS
 ******************************************************************************/

// TODO: add parameters
// Initialize the module
STATIC mp_obj_t mod_ble_mesh_init(mp_obj_t type_in) {

    // The BLE Mesh module should be initialized only once
    if(initialized == false) {
        // For now type = 0 means the device is initialized with onoff_server
//        mp_int_t type = mp_obj_get_int(type_in);
//        if(type == 0) {
//            esp_ble_mesh_elem_t tmp_elem = ESP_BLE_MESH_ELEMENT(0, root_models_server, ESP_BLE_MESH_MODEL_NONE);
//            memcpy(elements, &tmp_elem, sizeof(tmp_elem));
//        }
//        // For now type != 0 means the device is initialized with onoff_client
//        else {
//            esp_ble_mesh_elem_t tmp_elem = ESP_BLE_MESH_ELEMENT(0, root_models_client, ESP_BLE_MESH_MODEL_NONE);
//            memcpy(elements, &tmp_elem, sizeof(tmp_elem));
//        }
//
//        provision_ptr = (esp_ble_mesh_prov_t *)heap_caps_malloc(sizeof(esp_ble_mesh_prov_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
//        composition_ptr = (esp_ble_mesh_comp_t *)heap_caps_malloc(sizeof(esp_ble_mesh_comp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
//
//       //TODO: initialize composition based on input parameters
//       composition_ptr->cid = 0x02C4;  // CID_ESP=0x02C4
//       composition_ptr->elements = elements;
//       composition_ptr->element_count = ARRAY_SIZE(elements);
//       //TODO: initialize provision based on input parameters
//       /* Disable OOB security for SILabs Android app */
//       uint8_t dev_uuid[16] = { 0xdd, 0xdd };
//       provision_ptr->uuid = dev_uuid; // From the example
//       provision_ptr->output_size = 0;
//       provision_ptr->output_actions = 0;


//        if(provision_ptr != NULL && composition_ptr != NULL) {
//            esp_err_t err = esp_ble_mesh_init(provision_ptr, composition_ptr);
//            if(err != ESP_OK) {
//                // TODO: drop back the error code
//                nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "BLE Mesh module could not be initialized!"));
//            }
//            else {
//                initialized = true;
//            }
//        }
//        else {
//            nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "BLE Mesh module could not be initialized!"));
//        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "BLE Mesh module is already initialized!"));
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_ble_mesh_init_obj, mod_ble_mesh_init);

STATIC mp_obj_t mod_ble_mesh_add_model(mp_obj_t model_type_in, mp_obj_t model_functionality_in, mp_obj_t mode_server_client_in) {

    mp_int_t type = mp_obj_get_int(model_type_in);
    mp_int_t func = mp_obj_get_int(model_functionality_in);
    mp_int_t server_client = mp_obj_get_int(mode_server_client_in);

    //TODO: support more Elements

    // Check if Configuration Server Model has already been added, if not then add it before adding any additional Models
    if(mod_ble_mesh_elements.element == NULL) {

       // ESP_BLE_MESH_MODEL_CFG_SRV(&configuration_server_model);
    }

    if(type == MOD_BLE_MESH_SERVER) {

    }
    else {

    }

    return mp_const_none;



}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_ble_mesh_add_model_obj, mod_ble_mesh_add_model);


STATIC const mp_map_elem_t ble_mesh_locals_dict_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_ble_mesh_init_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_add_model),                       (mp_obj_t)&mod_ble_mesh_add_model_obj },

        // Constants of Server-Client
        { MP_OBJ_NEW_QSTR(MP_QSTR_SERVER),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_SERVER) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_CLIENT) },
        // Constants of Model types
        { MP_OBJ_NEW_QSTR(MP_QSTR_GENERIC),                        MP_OBJ_NEW_SMALL_INT(0) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_SENSORS),                        MP_OBJ_NEW_SMALL_INT(1) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_TIME_SCENES),                    MP_OBJ_NEW_SMALL_INT(2) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LIGHTNING),                      MP_OBJ_NEW_SMALL_INT(3) },
        // Constants of functionality behind a model
        { MP_OBJ_NEW_QSTR(MP_QSTR_ONOFF),                          MP_OBJ_NEW_SMALL_INT(0) },

};

STATIC MP_DEFINE_CONST_DICT(ble_mesh_locals_dict, ble_mesh_locals_dict_table);

// Sub-module of Bluetooth module (modbt.c)
const mp_obj_type_t mod_ble_mesh = {
    { &mp_type_type },
    .name = MP_QSTR_BLE_Mesh,
    .locals_dict = (mp_obj_t)&ble_mesh_locals_dict,
};
