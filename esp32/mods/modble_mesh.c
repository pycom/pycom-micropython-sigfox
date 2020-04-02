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
#define MOD_BLE_MESH_SERVER              (0)
#define MOD_BLE_MESH_CLIENT              (1)

#define MOD_BLE_MESH_MODEL_GENERIC       (0)
#define MOD_BLE_MESH_MODEL_SENSORS       (1)
#define MOD_BLE_MESH_MODEL_TIME_SCENES   (2)
#define MOD_BLE_MESH_MODEL_LIGHTNING     (3)

#define MOD_BLE_MESH_MODEL_ONOFF         (0)


/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/

typedef struct mod_ble_mesh_elem_s {
    uint16_t element_addr;
    uint16_t location;
    uint8_t sig_model_count;
    uint8_t vnd_model_count;
    esp_ble_mesh_model_t *sig_models;
    esp_ble_mesh_model_t *vnd_models;
}mod_ble_mesh_elem_t;


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
// TODO: add support for more Elements
static mod_ble_mesh_elem_t mod_ble_mesh_element;
static bool initialized = false;
static esp_ble_mesh_prov_t *provision_ptr;
static esp_ble_mesh_comp_t *composition_ptr;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static void mod_ble_mesh_generic_client_callback(esp_ble_mesh_generic_client_cb_event_t event, esp_ble_mesh_generic_client_cb_param_t *param) {

    // TODO: here the user registered MicroPython API should be called with correc parameters

    printf("mod_ble_mesh_generic_client_callback is called!\n");
    printf("event: %d\n", event);
    printf("error code: %d\n", param->error_code);
    // printf("error code: %d\n", param->status_cb);
    // printf("error code: %d\n", param->params);
}

static void mod_ble_mesh_config_server_callback(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param) {

    // TODO: here the user registered MicroPython API should be called with correc parameters
    printf("mod_ble_mesh_config_server_callback is called!\n");
    printf("event: %d\n", event);
}


/******************************************************************************
 DEFINE BLE MESH CLASS FUNCTIONS
 ******************************************************************************/

// TODO: add parameters
// Initialize the module
STATIC mp_obj_t mod_ble_mesh_init() {

    // The BLE Mesh module should be initialized only once
    if(initialized == false) {

        provision_ptr = (esp_ble_mesh_prov_t *)heap_caps_malloc(sizeof(esp_ble_mesh_prov_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        composition_ptr = (esp_ble_mesh_comp_t *)heap_caps_malloc(sizeof(esp_ble_mesh_comp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        //TODO: initialize composition based on input parameters
        composition_ptr->cid = 0x02C4;  // CID_ESP=0x02C4
        // TODO: add support for more Elements
        // TODO: check this cast, it might not be good
        composition_ptr->elements = (esp_ble_mesh_elem_t *)&mod_ble_mesh_element;
        composition_ptr->element_count = 1;

        //TODO: initialize provision based on input parameters
        /* Disable OOB security for SILabs Android app */
        uint8_t dev_uuid[16] = { 0xdd, 0xdd };
        provision_ptr->uuid = dev_uuid; // From the example
        provision_ptr->output_size = 0;
        provision_ptr->output_actions = 0;

        if(provision_ptr != NULL && composition_ptr != NULL) {

            // TODO: save the user registered MicroPython function somewhere
            esp_ble_mesh_register_generic_client_callback(mod_ble_mesh_generic_client_callback);
            esp_ble_mesh_register_config_server_callback(mod_ble_mesh_config_server_callback);

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

// TODO: add parameters for configuring the Configuration Server Model
STATIC mp_obj_t mod_ble_mesh_create_element() {


    //TODO: add support for several more elements
    mod_ble_mesh_element.location         = 0;
    mod_ble_mesh_element.sig_model_count  = 0;
    mod_ble_mesh_element.sig_models       = NULL;
    mod_ble_mesh_element.vnd_model_count  = 0;
    mod_ble_mesh_element.vnd_models       = NULL;


    // Add the mandatory Configuration Server Model
    esp_ble_mesh_cfg_srv_t* configuration_server_model_ptr = (esp_ble_mesh_cfg_srv_t *)heap_caps_malloc(sizeof(esp_ble_mesh_cfg_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // TODO: fetch the parameters via the API
    // Configure the Configuration Server Model
    configuration_server_model_ptr->relay = ESP_BLE_MESH_RELAY_DISABLED;
    configuration_server_model_ptr->beacon = ESP_BLE_MESH_BEACON_ENABLED;
    configuration_server_model_ptr->friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED;
    configuration_server_model_ptr->gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED;
    configuration_server_model_ptr->default_ttl = 7;
    configuration_server_model_ptr->net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20); /* 3 transmissions with 20ms interval */
    configuration_server_model_ptr->relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20);

    // Prepare temp model
    esp_ble_mesh_model_t tmp_model = ESP_BLE_MESH_MODEL_CFG_SRV(configuration_server_model_ptr);

    // Allocate memory for the model
    mod_ble_mesh_element.sig_models = (esp_ble_mesh_model_t*)heap_caps_malloc(sizeof(esp_ble_mesh_model_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // Copy the already prepared element to the new location
    memcpy(mod_ble_mesh_element.sig_models, &tmp_model, sizeof(esp_ble_mesh_model_t));
    // The first model has been added
    mod_ble_mesh_element.sig_model_count = 1;


    // TODO: return with the Element MicroPython object
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_ble_mesh_create_element_obj, mod_ble_mesh_create_element);


STATIC mp_obj_t mod_ble_mesh_add_model(mp_obj_t model_type_in, mp_obj_t model_functionality_in, mp_obj_t mode_server_client_in) {

    mp_int_t type = mp_obj_get_int(model_type_in);
    mp_int_t func = mp_obj_get_int(model_functionality_in);
    mp_int_t server_client = mp_obj_get_int(mode_server_client_in);

    // Start preparing the Model
    esp_ble_mesh_model_t tmp_model;
    // Server Type
    if(server_client == MOD_BLE_MESH_SERVER) {
        if(type == MOD_BLE_MESH_MODEL_GENERIC) {
            if(func == MOD_BLE_MESH_MODEL_ONOFF) {
                ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_srv_pub, 2 + 3, ROLE_NODE);
                esp_ble_mesh_gen_onoff_srv_t onoff_server = {
                    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
                    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
                };
                esp_ble_mesh_model_t gen_onoff_srv_mod = ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_srv_pub, &onoff_server);
                memcpy(&tmp_model, &gen_onoff_srv_mod, sizeof(gen_onoff_srv_mod));
            }
            else {
                //TODO: Add support for more functionality
            }
        }
        else {
            //TODO: Add support for more types
        }
    }
    // Client Type
    else {
        if(type == MOD_BLE_MESH_MODEL_GENERIC) {
            if(func == MOD_BLE_MESH_MODEL_ONOFF) {
                ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub, 2 + 1, ROLE_NODE);
                esp_ble_mesh_client_t onoff_client;
                esp_ble_mesh_model_t gen_onoff_cli_mod = ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub, &onoff_client);
                memcpy(&tmp_model, &gen_onoff_cli_mod, sizeof(gen_onoff_cli_mod));
            }
            else {
                //TODO: Add support for more functionality
            }
        }
        else {
            //TODO: Add support for more types
        }
    }


    //TODO: support adding more Elements, for now use the 1 we have
    mod_ble_mesh_element.sig_models = (esp_ble_mesh_model_t*)heap_caps_realloc(mod_ble_mesh_element.sig_models, (mod_ble_mesh_element.sig_model_count+1) * sizeof(esp_ble_mesh_model_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    memcpy(&mod_ble_mesh_element.sig_models[mod_ble_mesh_element.sig_model_count], &tmp_model, sizeof(tmp_model));
    mod_ble_mesh_element.sig_model_count++;

    //TODO: return with Model MicroPython object
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_ble_mesh_add_model_obj, mod_ble_mesh_add_model);


STATIC const mp_map_elem_t ble_mesh_locals_dict_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_ble_mesh_init_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_create_element),                  (mp_obj_t)&mod_ble_mesh_create_element_obj },
        // This should be submethod of the Element class
        { MP_OBJ_NEW_QSTR(MP_QSTR_add_model),                       (mp_obj_t)&mod_ble_mesh_add_model_obj },

        // Constants of Server-Client
        { MP_OBJ_NEW_QSTR(MP_QSTR_SERVER),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_SERVER) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_CLIENT) },
        // Constants of Model types
        { MP_OBJ_NEW_QSTR(MP_QSTR_GENERIC),                        MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_GENERIC) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_SENSORS),                        MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_SENSORS) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_TIME_SCENES),                    MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_TIME_SCENES) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LIGHTNING),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_LIGHTNING) },
        // Constants of functionality behind a model
        { MP_OBJ_NEW_QSTR(MP_QSTR_ONOFF),                          MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_ONOFF) },

};

STATIC MP_DEFINE_CONST_DICT(ble_mesh_locals_dict, ble_mesh_locals_dict_table);

// Sub-module of Bluetooth module (modbt.c)
const mp_obj_type_t mod_ble_mesh = {
    { &mp_type_type },
    .name = MP_QSTR_BLE_Mesh,
    .locals_dict = (mp_obj_t)&ble_mesh_locals_dict,
};
