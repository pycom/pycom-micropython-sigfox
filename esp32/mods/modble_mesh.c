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
#include <stdbool.h>
#include <math.h>

#include "esp_bt_device.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"
#include "esp_ble_mesh_networking_api.h"

#include "mpirq.h"

// Created for first Sensor Model release
// TODO: Allocate dynamically

NET_BUF_SIMPLE_DEFINE_STATIC(sensor_data, 2);

static esp_ble_mesh_sensor_state_t sensor_states[1] = {
    [0] = {
        // Dummy property for first release
        .sensor_property_id = 0x0056,
        .descriptor.positive_tolerance = ESP_BLE_MESH_SENSOR_UNSPECIFIED_POS_TOLERANCE,
        .descriptor.negative_tolerance = ESP_BLE_MESH_SENSOR_UNSPECIFIED_NEG_TOLERANCE,
        .descriptor.sampling_function = ESP_BLE_MESH_SAMPLE_FUNC_UNSPECIFIED,
        .descriptor.measure_period = ESP_BLE_MESH_SENSOR_NOT_APPL_MEASURE_PERIOD,
        .descriptor.update_interval = ESP_BLE_MESH_SENSOR_NOT_APPL_UPDATE_INTERVAL,
        .sensor_data.format = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A,
        .sensor_data.length = 1, /* 1 represents the length is 2 -> 16 bit representation */
        .sensor_data.raw_value = &sensor_data,
    },
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(sensor_pub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_srv_t sensor_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(sensor_states),
    .states = sensor_states,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(sensor_setup_pub, 20, ROLE_NODE);
static esp_ble_mesh_sensor_srv_t sensor_setup_server = {
    .rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    .state_count = ARRAY_SIZE(sensor_states),
    .states = sensor_states,
};

static esp_ble_mesh_client_t sensor_client;

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define MOD_BLE_MESH_SERVER                 (0)
#define MOD_BLE_MESH_CLIENT                 (1)

#define MOD_BLE_MESH_PROV_ADV               (1)
#define MOD_BLE_MESH_PROV_GATT              (2)
#define MOD_BLE_MESH_PROV_NONE              (4)

#define MOD_BLE_MESH_NO_OOB                 (0)
#define MOD_BLE_MESH_INPUT_OOB              (1)
#define MOD_BLE_MESH_OUTPUT_OOB             (2)

#define MOD_BLE_MESH_RELAY                  (1)
#define MOD_BLE_MESH_GATT_PROXY             (2)
#define MOD_BLE_MESH_LOW_POWER              (4)
#define MOD_BLE_MESH_FRIEND                 (8)

// For Configuration Model
#define MOD_BLE_MESH_MODEL_NONE             (4)

// Model Groups
#define MOD_BLE_MESH_GROUP_GENERIC          (0)
#define MOD_BLE_MESH_GROUP_SENSOR           (1)
#define MOD_BLE_MESH_GROUP_TIME             (2)
#define MOD_BLE_MESH_GROUP_LIGHTNING        (3)

// Model Sensor/Client
#define MOD_BLE_MESH_SERVER                 (0)
#define MOD_BLE_MESH_CLIENT                 (1)

// Model Type
#define MOD_BLE_MESH_MODEL_GEN_ONOFF        (0)
#define MOD_BLE_MESH_MODEL_GEN_LEVEL        (1)
#define MOD_BLE_MESH_MODEL_SENSOR           (2)
#define MOD_BLE_MESH_MODEL_SENSOR_SETUP     (3)
#define MOD_BLE_MESH_MODEL_CONFIGURATION    (4)

// States
#define MOD_BLE_MESH_STATE_ONOFF            (0)
#define MOD_BLE_MESH_STATE_LEVEL            (3)
#define MOD_BLE_MESH_STATE_LEVEL_DELTA      (7)
#define MOD_BLE_MESH_STATE_LEVEL_MOVE       (11)
#define MOD_BLE_MESH_STATE_SEN_DESCRIPTOR   (14)
#define MOD_BLE_MESH_STATE_SEN              (18)
#define MOD_BLE_MESH_STATE_SEN_COLUMN       (22)
#define MOD_BLE_MESH_STATE_SEN_SERIES       (26)
#define MOD_BLE_MESH_STATE_SEN_SET_CADENCE  (29)
#define MOD_BLE_MESH_STATE_SEN_SET_SETTINGS (33)
#define MOD_BLE_MESH_STATE_SEN_SET_SETTING  (37)

// Requests
#define MOD_BLE_MESH_REQ_GET                (0)
#define MOD_BLE_MESH_REQ_SET                (1)
#define MOD_BLE_MESH_REQ_SET_UNACK          (2)
#define MOD_BLE_MESH_REQ_STATE              (3)

#define MOD_BLE_ADDR_ALL_NODES              (0xFFFF)
#define MOD_BLE_ADDR_PUBLISH                (0x0000)

#define MOD_BLE_MESH_DEFAULT_NAME           "PYCOM-ESP-BLE-MESH"

#define MOD_BLE_MESH_STATE_LOCATION_IN_SRV_T_TYPE (sizeof(esp_ble_mesh_model_t*) + sizeof(esp_ble_mesh_server_rsp_ctrl_t))

// Define macros from latest ESP-IDF
#define ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID(_len, _id) \
        ((((_id) & BIT_MASK(11)) << 5) | (((_len) & BIT_MASK(4)) << 1) | ESP_BLE_MESH_SENSOR_DATA_FORMAT_A)

#define ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(_len, _id) \
        (((_id) << 8) | (((_len) & BIT_MASK(7)) << 1) | ESP_BLE_MESH_SENSOR_DATA_FORMAT_B)

#define ESP_BLE_MESH_GET_SENSOR_DATA_PROPERTY_ID(_data, _fmt)   \
            (((_fmt) == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) ? ((((_data)[1]) << 3) | (((_data)[0]) >> 5)) : ((((_data)[2]) << 8) | ((_data)[1])))

#define ESP_BLE_MESH_GET_SENSOR_DATA_FORMAT(_data)      (((_data)[0]) & BIT_MASK(1))

#define ESP_BLE_MESH_GET_SENSOR_DATA_LENGTH(_data, _fmt)    \
            (((_fmt) == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) ? ((((_data)[0]) >> 1) & BIT_MASK(4)) : ((((_data)[0]) >> 1) & BIT_MASK(7)))

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/

// TYPE : SIZE table, can be addressed with (model_type + state + request).
static const uint8_t opcode_table[] = {
        // GENERIC ONOFF ONOFF
        BLE_MESH_MODEL_OP_GEN_ONOFF_GET,
        BLE_MESH_MODEL_OP_GEN_ONOFF_SET,
        BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK,
        BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
        // GENERIC LEVEL LEVEL
        BLE_MESH_MODEL_OP_GEN_LEVEL_GET,
        BLE_MESH_MODEL_OP_GEN_LEVEL_SET,
        BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK,
        BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS,
        // GENERIC LEVEL DELTA
        -1,
        BLE_MESH_MODEL_OP_GEN_DELTA_SET,
        BLE_MESH_MODEL_OP_GEN_DELTA_SET_UNACK,
        -1,
        // GENERIC LEVEL MOVE
        -1,
        BLE_MESH_MODEL_OP_GEN_MOVE_SET,
        BLE_MESH_MODEL_OP_GEN_MOVE_SET_UNACK,
        -1,
        // SENSOR SENSOR_DESCRIPTOR
        BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET,
        -1,
        -1,
        BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_STATUS,
        // SENSOR SENSOR
        BLE_MESH_MODEL_OP_SENSOR_GET,
        -1,
        -1,
        BLE_MESH_MODEL_OP_SENSOR_STATUS,
        // SENSOR SENSOR_COLUMN
        BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET,
        -1,
        -1,
        BLE_MESH_MODEL_OP_SENSOR_COLUMN_STATUS,
        // SENSOR SENSOR_SERIES
        BLE_MESH_MODEL_OP_SENSOR_SERIES_GET,
        -1,
        -1,
        BLE_MESH_MODEL_OP_SENSOR_SERIES_STATUS,
        // SENSOR_SETUP SENSOR_CADENCE
        BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET,
        BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET,
        BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET_UNACK,
        BLE_MESH_MODEL_OP_SENSOR_CADENCE_STATUS,
        // SENSOR SENSOR_COLUMN
        BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET,
        -1,
        -1,
        BLE_MESH_MODEL_OP_SENSOR_SETTINGS_STATUS,
        // SENSOR SENSOR_SERIES
        BLE_MESH_MODEL_OP_SENSOR_SETTING_GET,
        BLE_MESH_MODEL_OP_SENSOR_SETTING_SET,
        BLE_MESH_MODEL_OP_SENSOR_SETTING_SET_UNACK,
        BLE_MESH_MODEL_OP_SENSOR_SETTING_STATUS,

};

// There is only one UUID, and it cannot be changed during runtime
// TODO: why is this initialized with these 2 values ?
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

// This is the same as esp_ble_mesh_elem_t just without const members
typedef struct mod_ble_mesh_elem_s {
    uint16_t element_addr;
    uint16_t location;
    uint8_t sig_model_count;
    uint8_t vnd_model_count;
    esp_ble_mesh_model_t *sig_models;
    esp_ble_mesh_model_t *vnd_models;
}mod_ble_mesh_elem_t;

typedef struct mod_ble_mesh_element_class_s {
    mp_obj_base_t base;
    mod_ble_mesh_elem_t *element;
}mod_ble_mesh_element_class_t;

typedef struct mod_ble_mesh_model_class_s {
    mp_obj_base_t base;
    struct mod_ble_mesh_model_class_s* next;
    // Pointer to the element owns this model
    mod_ble_mesh_element_class_t *element;
    // Index in the esp ble context
    uint8_t index;
    // User defined MicroPython callback function
    mp_obj_t callback;
    // Value of the model to return to MicroPython
    mp_obj_t value_mp_obj;
    // Default Get Opcode
    int32_t op_def_get;
    // Default Set Opcode
    int32_t op_def_set;
    // Default Set_Unack Opcode
    int32_t op_def_set_unack;
    // Default Status Opcode
    int32_t op_def_status;
    // Store whether this is a Server or a Client Model
    bool server_client;
    // Store Model Type
    uint8_t type;
    // Store Model Group
    uint8_t group;
}mod_ble_mesh_model_class_t;

typedef struct mod_ble_mesh_generic_server_callback_param_s {
    esp_ble_mesh_generic_server_cb_event_t event;
    esp_ble_mesh_generic_server_cb_param_t* param;
}mod_ble_mesh_generic_server_callback_param_t;

typedef struct mod_ble_mesh_generic_client_callback_param_s {
    esp_ble_mesh_generic_client_cb_event_t event;
    esp_ble_mesh_generic_client_cb_param_t* param;
}mod_ble_mesh_generic_client_callback_param_t;

typedef struct mod_ble_mesh_sensor_server_callback_param_s {
    esp_ble_mesh_sensor_server_cb_event_t event;
    esp_ble_mesh_sensor_server_cb_param_t* param;
}mod_ble_mesh_sensor_server_callback_param_t;

typedef struct mod_ble_mesh_sensor_client_callback_param_s {
    esp_ble_mesh_sensor_client_cb_event_t event;
    esp_ble_mesh_sensor_client_cb_param_t* param;
}mod_ble_mesh_sensor_client_callback_param_t;

typedef struct mod_ble_mesh_provision_callback_param_s {
    mp_obj_t callback;
    int8_t prov_event;
    int8_t oob_key;
}mod_ble_mesh_provision_callback_param_t;

typedef struct mod_ble_mesh_sensor_representation_s {
    float sen_min;
    float sen_max;
    float sen_res;
}mod_ble_mesh_sensor_representation_t;


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static mp_obj_t mod_ble_state_to_mp_obj(mod_ble_mesh_model_class_t* model, void* state_change);
static esp_ble_mesh_generic_client_set_state_t mod_ble_mp_obj_to_state(mp_obj_t obj, mod_ble_mesh_model_class_t* model);
static uint16_t example_ble_mesh_get_sensor_data(esp_ble_mesh_sensor_state_t *state, uint8_t *data);

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/

static const mp_obj_type_t mod_ble_mesh_model_type;
// TODO: add support for more Elements
static mod_ble_mesh_element_class_t mod_ble_mesh_element;
// This is a list containing all the Models regardless their Elements
static mod_ble_mesh_model_class_t *mod_ble_models_list = NULL;

static bool initialized = false;
static esp_ble_mesh_prov_t *provision_ptr;
static esp_ble_mesh_comp_t *composition_ptr;
// TODO: double check whether this is needed
static uint8_t msg_tid = 0x0;
static mod_ble_mesh_sensor_representation_t mod_ble_mesh_sen_rep;

// TODO: This should be part of the primary element as provisioning is performed on it only
mp_obj_t provision_callback;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static mod_ble_mesh_model_class_t* mod_ble_add_model_to_list(mod_ble_mesh_element_class_t* ble_mesh_element,
                                                             uint8_t index,
                                                             mp_obj_t callback,
                                                             int32_t op_def_get,
                                                             int32_t op_def_set,
                                                             int32_t op_def_set_unack,
                                                             int32_t op_def_status,
                                                             mp_obj_t value_mp_obj,
                                                             bool server_client,
                                                             uint8_t type,
                                                             uint8_t group) {

    mod_ble_mesh_model_class_t *model = mod_ble_models_list;
    mod_ble_mesh_model_class_t *model_last = NULL; // Only needed as helper pointer for the case when the list is fully empty


    // Find the last element of the list
    while(model != NULL) {
        // Save the last Model we checked
        model_last = model;
        // Jump to the next Model
        model = model->next;
    }

    // Allocate memory for the new element
    //TODO: check this allocation, it should be from GC controlled memory
    model = (mod_ble_mesh_model_class_t*)heap_caps_malloc(sizeof(mod_ble_mesh_model_class_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // Initialize the new Model
    model->base.type = &mod_ble_mesh_model_type;
    model->element = ble_mesh_element;
    model->index = index;
    model->callback = callback;
    model->next = NULL;
    model->op_def_get = op_def_get;
    model->op_def_set = op_def_set;
    model->op_def_set_unack = op_def_set_unack;
    model->op_def_status = op_def_status;
    model->value_mp_obj = value_mp_obj;
    model->server_client = server_client;
    model->type = type;
    model->group = group;

    // Add the new element to the list
    if(model_last != NULL) {
        model_last->next = model;
    }
    else {
        // This means this new element is the very first one in the list
        mod_ble_models_list = model;
    }

    return model;
}

static mod_ble_mesh_model_class_t* mod_ble_find_model(esp_ble_mesh_model_t *esp_ble_model) {

    mod_ble_mesh_model_class_t *model = mod_ble_models_list;

    // TODO: handle when we have more elements, first find the BLE Mesh Element using esp_ble_model->element reference

    // The pointer to the Model directly cannot be used to match the incoming ESP BLE Model with MOD BLE Mesh model,
    // because of some reasons the BLE Mesh library does not always use the same pointer what was saved into Element's model list, that's why the index must be used
    // Iterate through on all the Models
    while(model != NULL) {
        if(model->index == esp_ble_model->model_idx) {
            return model;
        }
        model = model->next;
    }

    return NULL;
}

static void mod_ble_mesh_generic_server_callback_handler(void* param_in) {

    mod_ble_mesh_generic_server_callback_param_t* callback_param = (mod_ble_mesh_generic_server_callback_param_t*)param_in;
    mod_ble_mesh_model_class_t* mod_ble_model = mod_ble_find_model(callback_param->param->model);

    if(mod_ble_model != NULL) {

        esp_ble_mesh_generic_server_cb_event_t event = callback_param->event;
        esp_ble_mesh_generic_server_cb_param_t* param = callback_param->param;
        esp_ble_mesh_model_t* model = callback_param->param->model;

        //printf("Addr: %d\n", param->ctx.addr);
        mp_obj_t args[3];

        switch (event) {
            case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
                //printf("ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT\n");
                // TODO: implement this case, for now let if falling through to STATE because
                // ESP_BLE_MESH_SERVER_AUTO_RSP is set, so SET event will never come, only STATE
            case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
                //printf("ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT\n");
                // Publish that the State of this Server Model changed

                args[0] = mod_ble_state_to_mp_obj(mod_ble_model, &(param->value.state_change));
                args[1] = mp_obj_new_int(event);
                //TODO: check what other object should be passed from the context
                args[2] = mp_obj_new_int(param->ctx.recv_op);
                mp_call_function_n_kw(mod_ble_model->callback, 3, 0, args);
                //
                break;
            case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
                //printf("ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT\n");
                // TODO: double check this, should be automatically answered by the Mesh Library because ESP_BLE_MESH_SERVER_AUTO_RSP is set
                break;
            default:
                // Handle it silently
                break;
        }
    }
}

static void mod_ble_mesh_generic_client_callback_handler(void* param_in) {

    mod_ble_mesh_generic_client_callback_param_t* callback_param = (mod_ble_mesh_generic_client_callback_param_t*)param_in;
    mod_ble_mesh_model_class_t* mod_ble_model = mod_ble_find_model(callback_param->param->params->model);

    if(mod_ble_model != NULL) {

        esp_ble_mesh_generic_client_cb_event_t event = callback_param->event;
        esp_ble_mesh_generic_client_cb_param_t* param = callback_param->param;

        //printf("Addr: %d\n", param->params->ctx.addr);

        switch (event) {
            case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
                //printf("ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT\n");
                //TODO: figure out a generic way to handle this... it is not easy
                break;
            case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
                //printf("ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT\n");
                break;
            case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
                //printf("ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT\n");
                //TODO: figure out a generic way to handle this... it is not easy
                break;
            case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
                //printf("ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT\n");
                break;
            default:
                break;
            }

        mp_obj_t args[3];
        args[0] = mod_ble_state_to_mp_obj(mod_ble_model, (void*)&(param->status_cb));
        args[1] = mp_obj_new_int(event);
        //TODO: check what other object should be passed from the context
        args[2] = mp_obj_new_int(param->params->ctx.recv_op);
        mp_call_function_n_kw(mod_ble_model->callback, 3, 0, args);
    }
}

static void mod_ble_mesh_sensor_server_callback_handler(void* param_in) {

    mod_ble_mesh_sensor_server_callback_param_t* callback_param = (mod_ble_mesh_sensor_server_callback_param_t*)param_in;
    mod_ble_mesh_model_class_t* mod_ble_model = mod_ble_find_model(callback_param->param->model);

    if(mod_ble_model != NULL) {

        esp_ble_mesh_sensor_server_cb_event_t event = callback_param->event;
        esp_ble_mesh_sensor_server_cb_param_t* param = callback_param->param;
        esp_ble_mesh_model_t* model = callback_param->param->model;

        if (event == ESP_BLE_MESH_SENSOR_SERVER_RECV_GET_MSG_EVT) {

            uint8_t *status = NULL;
            uint16_t buf_size = 0;
            uint16_t length = 0;

            esp_ble_mesh_sensor_state_t *state = &sensor_states[0];

            if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
            } else {
                /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
                if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
                    buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN + state->sensor_data.length + 1;
                } else {
                    buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN + state->sensor_data.length + 1;
                }
            }

            status = calloc(1, buf_size);
            length = example_ble_mesh_get_sensor_data(&sensor_states[0], status);

            esp_ble_mesh_server_model_send_msg(&param->ctx.model, &param->ctx, ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS, length, status);
            free(status);

        }
    }
}

static void mod_ble_mesh_sensor_client_callback_handler(void* param_in) {

    mod_ble_mesh_sensor_client_callback_param_t* callback_param = (mod_ble_mesh_sensor_client_callback_param_t*)param_in;
    mod_ble_mesh_model_class_t* mod_ble_model = mod_ble_find_model(callback_param->param->params->model);

    if(mod_ble_model != NULL) {

        esp_ble_mesh_sensor_client_cb_event_t event = callback_param->event;
        esp_ble_mesh_sensor_client_cb_param_t* param = callback_param->param;
        esp_ble_mesh_model_t* model = callback_param->param->params->model;

        if (event == ESP_BLE_MESH_SENSOR_CLIENT_PUBLISH_EVT && param->status_cb.sensor_status.marshalled_sensor_data->len) {
            mp_obj_t args[3];
            args[0] = mod_ble_state_to_mp_obj(mod_ble_model, (void*)&(param->status_cb));
            args[1] = mp_obj_new_int(event);
            //TODO: check what other object should be passed from the context
            args[2] = mp_obj_new_int(param->params->ctx.recv_op);
            mp_call_function_n_kw(mod_ble_model->callback, 3, 0, args);
        }
    }
}

STATIC void mod_ble_mesh_provision_callback_call_mp_callback(void* param_in) {

    mod_ble_mesh_provision_callback_param_t* callback_param = (mod_ble_mesh_provision_callback_param_t*)param_in;
    mp_obj_t args[2];
    // GET OOB Type
    args[0] = mp_obj_new_int(callback_param->prov_event);

    if(callback_param->prov_event == ESP_BLE_MESH_NODE_PROV_OUTPUT_NUMBER_EVT) {
        //OOB Key
        args[1] = mp_obj_new_int(callback_param->oob_key);
    }
    else {
        //Invalid OOB Key
        args[1] = mp_const_none;
    }

    // Call the user registered MicroPython function
    mp_call_function_n_kw(callback_param->callback, 2, 0, args);

    // Free callback_param
    heap_caps_free(callback_param);
}

static void mod_ble_mesh_provision_callback(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param) {
    mod_ble_mesh_provision_callback_param_t* callback_param_ptr = NULL;

    if(provision_callback != NULL) {
        callback_param_ptr = (mod_ble_mesh_provision_callback_param_t *)heap_caps_malloc(sizeof(mod_ble_mesh_provision_callback_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        callback_param_ptr->callback = provision_callback;
        callback_param_ptr->prov_event = event;

        if (event == ESP_BLE_MESH_NODE_PROV_OUTPUT_NUMBER_EVT) {
            callback_param_ptr->oob_key = param->node_prov_output_num.number;
        }

        if(callback_param_ptr != NULL) {
            // The user registered MicroPython callback will be called decoupled from the BLE Mesh context in the IRQ Task
            mp_irq_queue_interrupt(mod_ble_mesh_provision_callback_call_mp_callback, (void *)callback_param_ptr);
        }
    }
}

static void mod_ble_mesh_generic_client_callback(esp_ble_mesh_generic_client_cb_event_t event, esp_ble_mesh_generic_client_cb_param_t *param) {

    mod_ble_mesh_generic_client_callback_param_t* callback_param_ptr = (mod_ble_mesh_generic_client_callback_param_t *)heap_caps_malloc(sizeof(mod_ble_mesh_generic_server_callback_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    callback_param_ptr->param = (esp_ble_mesh_generic_client_cb_param_t *)heap_caps_malloc(sizeof(esp_ble_mesh_generic_client_cb_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    callback_param_ptr->param->params = (esp_ble_mesh_client_common_param_t *)heap_caps_malloc(sizeof(esp_ble_mesh_client_common_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    callback_param_ptr->event = event;
    memcpy(callback_param_ptr->param, param, sizeof(esp_ble_mesh_generic_client_cb_param_t));
    memcpy(callback_param_ptr->param->params, param->params, sizeof(esp_ble_mesh_client_common_param_t));

    // The registered callback will be handled in context of TASK_Interrupts
    mp_irq_queue_interrupt_non_ISR(mod_ble_mesh_generic_client_callback_handler, (void *)callback_param_ptr);
}

static void mod_ble_mesh_generic_server_callback(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param) {

    mod_ble_mesh_generic_server_callback_param_t* callback_param_ptr = (mod_ble_mesh_generic_server_callback_param_t *)heap_caps_malloc(sizeof(mod_ble_mesh_generic_server_callback_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    callback_param_ptr->param = (esp_ble_mesh_generic_server_cb_param_t *)heap_caps_malloc(sizeof(esp_ble_mesh_generic_server_cb_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    callback_param_ptr->event = event;
    memcpy(callback_param_ptr->param, param, sizeof(esp_ble_mesh_generic_server_cb_param_t));

    // The registered callback will be handled in context of TASK_Interrupts
    mp_irq_queue_interrupt_non_ISR(mod_ble_mesh_generic_server_callback_handler, (void *)callback_param_ptr);

}

static void mod_ble_mesh_sensor_server_callback(esp_ble_mesh_sensor_server_cb_event_t event,
        esp_ble_mesh_sensor_server_cb_param_t *param) {

    mod_ble_mesh_sensor_server_callback_param_t* callback_param_ptr = (mod_ble_mesh_sensor_server_callback_param_t *)heap_caps_malloc(sizeof(mod_ble_mesh_sensor_server_callback_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    callback_param_ptr->param = (esp_ble_mesh_sensor_server_cb_param_t *)heap_caps_malloc(sizeof(esp_ble_mesh_sensor_server_cb_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    callback_param_ptr->event = event;
    memcpy(callback_param_ptr->param, param, sizeof(esp_ble_mesh_sensor_server_cb_param_t));

    // The registered callback will be handled in context of TASK_Interrupts
    mp_irq_queue_interrupt_non_ISR(mod_ble_mesh_sensor_server_callback_handler, (void *)callback_param_ptr);

}

static void mod_ble_mesh_sensor_client_callback(esp_ble_mesh_sensor_client_cb_event_t event,
        esp_ble_mesh_sensor_client_cb_param_t *param) {

    mod_ble_mesh_sensor_client_callback_param_t* callback_param_ptr = (mod_ble_mesh_sensor_client_callback_param_t *)heap_caps_malloc(sizeof(mod_ble_mesh_sensor_client_callback_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    callback_param_ptr->param = (esp_ble_mesh_sensor_client_cb_param_t *)heap_caps_malloc(sizeof(esp_ble_mesh_sensor_client_cb_param_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    callback_param_ptr->event = event;
    memcpy(callback_param_ptr->param, param, sizeof(esp_ble_mesh_sensor_client_cb_param_t));

    // The registered callback will be handled in context of TASK_Interrupts
    mp_irq_queue_interrupt_non_ISR(mod_ble_mesh_sensor_client_callback_handler, (void *)callback_param_ptr);

}

static void mod_ble_mesh_config_server_callback(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param) {

    // TODO: here the user registered MicroPython API should be called with correct parameters via the Interrupt Task

    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            printf("ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD\n");
            printf("net_idx 0x%04x, app_idx 0x%04x\n",
                param->value.state_change.appkey_add.net_idx,
                param->value.state_change.appkey_add.app_idx);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            printf("ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND\n");
            printf("elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x\n",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            printf("ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD\n");
            printf("elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x\n",
                param->value.state_change.mod_sub_add.element_addr,
                param->value.state_change.mod_sub_add.sub_addr,
                param->value.state_change.mod_sub_add.company_id,
                param->value.state_change.mod_sub_add.model_id);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
            printf("ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET\n");
            printf("elem_addr 0x%04x, pub_addr 0x%04x, pub_period %d, mod_id 0x%04x\n",
                    param->value.state_change.mod_pub_set.element_addr,
                    param->value.state_change.mod_pub_set.pub_addr,
                    param->value.state_change.mod_pub_set.pub_period,
                    param->value.state_change.mod_pub_set.model_id);
            break;
        default:
            //TODO: we need to handle all types of event coming from Configuration Client (Provisioner) !
            break;
        }
    }
}

// Returns MicroPython object from C structure
static mp_obj_t mod_ble_state_to_mp_obj(mod_ble_mesh_model_class_t* model, void* state_change) {

    mp_obj_t ret = mp_const_none;

    if(model->server_client == MOD_BLE_MESH_SERVER) {
        if(model->group == MOD_BLE_MESH_GROUP_GENERIC) {
            esp_ble_mesh_generic_server_state_change_t* state_change_server;
            state_change_server = (esp_ble_mesh_generic_server_state_change_t*) state_change;
            if(model->type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
                esp_ble_mesh_gen_onoff_srv_t *srv = model->element->element->sig_models[model->index].user_data;
                if(state_change == NULL) {
                    ret = mp_obj_new_bool(srv->state.onoff);
                } else {
                    ret = mp_obj_new_bool(state_change_server->onoff_set.onoff);
                }
            }
            else if(model->type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
                esp_ble_mesh_gen_level_srv_t *srv = model->element->element->sig_models[model->index].user_data;
                if(state_change == NULL) {
                    ret = mp_obj_new_int(srv->state.level);
                } else {
                    ret = mp_obj_new_int(state_change_server->level_set.level);
                }
            }
        }
    } else {
        if(model->group == MOD_BLE_MESH_GROUP_GENERIC) {
            esp_ble_mesh_gen_client_status_cb_t* state_change_client;
            state_change_client = (esp_ble_mesh_gen_client_status_cb_t*) state_change;
            if(model->type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
                ret = mp_obj_new_bool(state_change_client->onoff_status.present_onoff);
            }
            else if(model->type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
                ret = mp_obj_new_int(state_change_client->level_status.present_level);
            }
        }
        else if(model->group == MOD_BLE_MESH_GROUP_SENSOR) {
            esp_ble_mesh_sensor_client_status_cb_t* state_change_client;
            state_change_client = (esp_ble_mesh_sensor_client_status_cb_t*) state_change;
            if(model->type == MOD_BLE_MESH_MODEL_SENSOR) {
                // GET the state of Sensor Server
                uint8_t *data = state_change_client->sensor_status.marshalled_sensor_data->__buf;
                uint8_t fmt = ESP_BLE_MESH_GET_SENSOR_DATA_FORMAT(data);
                //Can be used later
                //uint16_t prop_id = ESP_BLE_MESH_GET_SENSOR_DATA_PROPERTY_ID(data, fmt);
                uint8_t mpid_len = (fmt == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A ? ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN : ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN);
                uint8_t raw_meas1 = *(data + mpid_len);
                uint8_t raw_meas2 = *(data + mpid_len + 1);
                uint16_t raw_meas = (raw_meas2 << 8) | (raw_meas1 & 0xff);
                float float_meas = raw_meas*(mod_ble_mesh_sen_rep.sen_max - mod_ble_mesh_sen_rep.sen_min)/65535 + mod_ble_mesh_sen_rep.sen_min;
                ret =  mp_obj_new_float(round(float_meas/mod_ble_mesh_sen_rep.sen_res)*mod_ble_mesh_sen_rep.sen_res);
            }
        }
    }
    return ret;
}

// Sets Server Model state or returns Client set structure for Get request
static esp_ble_mesh_generic_client_set_state_t mod_ble_mp_obj_to_state(mp_obj_t obj, mod_ble_mesh_model_class_t* model) {

    esp_ble_mesh_generic_client_set_state_t set;

    if(model->server_client == MOD_BLE_MESH_SERVER) {
        // Server
        if(model->type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
            esp_ble_mesh_gen_onoff_srv_t *srv = model->element->element->sig_models[model->index].user_data;
            srv->state.onoff = mp_obj_get_int(obj);
        }
        if(model->type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
            esp_ble_mesh_gen_level_srv_t *srv = model->element->element->sig_models[model->index].user_data;
            srv->state.level = mp_obj_get_int(obj);
        }
    } else {
        // Client

        if(model->type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
            set.onoff_set.onoff = mp_obj_get_int(obj);
            set.onoff_set.op_en = false;
            set.onoff_set.tid = msg_tid++;
        }
        if(model->type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
            set.level_set.level = mp_obj_get_int(obj);
            set.level_set.op_en = false;
            set.level_set.tid = msg_tid++;
        }
    }
    return set;
}

/******************************************************************************
 DEFINE BLE MESH MODEL FUNCTIONS
 ******************************************************************************/

// Get State of Server/Client Model
STATIC mp_obj_t mod_ble_mesh_model_get_state(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_model_get_state_args[] = {
            { MP_QSTR_self_in,               MP_ARG_OBJ,                                                     },
            { MP_QSTR_addr,                  MP_ARG_INT,                   {.u_int = MOD_BLE_ADDR_ALL_NODES} },
            { MP_QSTR_app_idx,               MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0} },
            { MP_QSTR_state_type,            MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = mp_const_none} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_model_get_state_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_model_get_state_args, args);

    // Get Mod BLE Mesh Model
    mod_ble_mesh_model_class_t* self = (mod_ble_mesh_model_class_t*)args[0].u_obj;

    if(self->server_client == MOD_BLE_MESH_SERVER) {

        if(self->group == MOD_BLE_MESH_GROUP_GENERIC) {
            // GET the state of Generic Server
            return mod_ble_state_to_mp_obj(self, NULL);
        }
        else if(self->group == MOD_BLE_MESH_GROUP_SENSOR) {
            // GET the state of Sensor Server
            uint16_t raw_meas = (sensor_data.data[1] << 8) | (sensor_data.data[0] & 0xff);
            float float_meas = raw_meas*(mod_ble_mesh_sen_rep.sen_max - mod_ble_mesh_sen_rep.sen_min)/65535 + mod_ble_mesh_sen_rep.sen_min;
            return mp_obj_new_float(round(float_meas/mod_ble_mesh_sen_rep.sen_res)*mod_ble_mesh_sen_rep.sen_res);
        }
        else {
            return mp_const_none;
        }

    } else {
        // Fetch parameters
        uint16_t addr = args[1].u_int;
        uint16_t app_idx = args[2].u_int;
        mp_obj_t state_type = args[3].u_obj;

        // Fetch default opcode
        int32_t opcode = self->op_def_get;

        if(state_type != mp_const_none) {
            // Get opcode if state is defined
            opcode = opcode_table[self->type + mp_obj_get_int(state_type) + MOD_BLE_MESH_REQ_GET];
        }

        if(opcode == -1) {
            // Error if Get request is not possible on state
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Cannot Get this State!"));
        }

        // Setup CTX
        esp_ble_mesh_msg_ctx_t ctx;
        ctx.net_idx = 0;
        ctx.app_idx = app_idx;
        ctx.addr = addr;
        ctx.send_ttl = 3;
        ctx.send_rel = false;

        if(self->group == MOD_BLE_MESH_GROUP_GENERIC) {
            // GET the state of Generic Server
            uint8_t* data = (uint8_t*)calloc(11, sizeof(uint8_t));
            esp_ble_mesh_client_model_send_msg(&self->element->element->sig_models[self->index], &ctx, opcode, 11, data, 100, 0, ROLE_NODE);
        }
        else if(self->group == MOD_BLE_MESH_GROUP_SENSOR) {
            // Get the state of Sensor Server
            esp_ble_mesh_sensor_client_get_state_t get = {0};
            esp_ble_mesh_client_common_param_t common = {0};

            common.opcode = BLE_MESH_MODEL_OP_SENSOR_GET;
            common.model = &self->element->element->sig_models[self->index];
            common.ctx = ctx;
            common.msg_timeout = 100;
            common.msg_role = ROLE_NODE;

            //get.sensor_get.property_id = 0x0056;

            esp_ble_mesh_sensor_client_get_state(&common, &get);
        }

        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_model_get_state_obj, 1, mod_ble_mesh_model_get_state);

// Sending out Set State message and updating the value of the Model
STATIC mp_obj_t mod_ble_mesh_model_set_state(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_model_set_state_args[] = {
            { MP_QSTR_self_in,               MP_ARG_OBJ,                                                     },
            { MP_QSTR_value,                 MP_ARG_OBJ | MP_ARG_REQUIRED,                                   },
            { MP_QSTR_addr,                  MP_ARG_INT,                   {.u_int = MOD_BLE_ADDR_ALL_NODES} },
            { MP_QSTR_app_idx,               MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0} },
            { MP_QSTR_state_type,            MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = mp_const_none} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_model_set_state_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_model_set_state_args, args);

    // Get Mod BLE Mesh Model
    mod_ble_mesh_model_class_t* self = (mod_ble_mesh_model_class_t*)args[0].u_obj;

    // Fetch parameters
    mp_obj_t state_mp = args[1].u_obj;
    uint16_t addr = args[2].u_int;
    uint16_t app_idx = args[3].u_int;
    mp_obj_t state_type = args[4].u_obj;

    if(self->server_client == MOD_BLE_MESH_SERVER) {

        if(self->group == MOD_BLE_MESH_GROUP_GENERIC) {
            // SET the state of Generic Server
            mod_ble_mp_obj_to_state(state_mp, self);

            // Inform user through callback
            mp_obj_t args[3];
            args[0] = mod_ble_state_to_mp_obj(self, NULL);
            args[1] = mp_obj_new_int(0);
            args[2] = mp_obj_new_int(0);
            mp_call_function_n_kw(self->callback, 3, 0, args);
        }
        else if(self->group == MOD_BLE_MESH_GROUP_SENSOR) {
            // SET the state of Sensor Server
            float state_float = mp_obj_get_float(state_mp);
            uint16_t state_raw = (uint16_t)((65535/(mod_ble_mesh_sen_rep.sen_max - mod_ble_mesh_sen_rep.sen_min))*(state_float - mod_ble_mesh_sen_rep.sen_min));

            sensor_data.data[0] = state_raw & 0xff;
            sensor_data.data[1] = state_raw >> 8;
        }
    } else {

        // Fetch default opcode
        int32_t opcode = self->op_def_set_unack;

        if(state_type != mp_const_none) {
            // Get opcode if state is defined
            opcode = opcode_table[self->type + mp_obj_get_int(state_type) + MOD_BLE_MESH_REQ_SET_UNACK];
        }

        if(opcode == -1) {
            // Error if Set request is not possible on state
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Cannot Set this State!"));
        }

        if(self->group == MOD_BLE_MESH_GROUP_GENERIC) {

            // Send a Set request from Client to Server
            esp_ble_mesh_generic_client_set_state_t set = mod_ble_mp_obj_to_state(state_mp, self);
            esp_ble_mesh_client_common_param_t common = {0};

            // Set required options
            common.opcode = opcode;
            common.model = &self->element->element->sig_models[self->index];
            common.ctx.net_idx = 0;
            common.ctx.app_idx = app_idx;
            common.ctx.addr = addr;
            common.ctx.send_ttl = 3;
            common.ctx.send_rel = false;
            common.msg_timeout = 100;
            common.msg_role = ROLE_NODE;

            // Send set state
            esp_ble_mesh_generic_client_set_state(&common, &set);
        }
        else if(self->group == MOD_BLE_MESH_GROUP_SENSOR) {
            // TODO: Sensor SetState Features
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_model_set_state_obj, 1, mod_ble_mesh_model_set_state);

// Sending out Set State message and updating the value of the Model
STATIC mp_obj_t mod_ble_mesh_model_status_state(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_model_status_state_args[] = {
            { MP_QSTR_self_in,               MP_ARG_OBJ,                                                     },
            { MP_QSTR_addr,                  MP_ARG_INT,                   {.u_int = MOD_BLE_ADDR_ALL_NODES} },
            { MP_QSTR_app_idx,               MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 0} },
            { MP_QSTR_state_type,            MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = mp_const_none} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_model_status_state_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_model_status_state_args, args);

    // Get Mod BLE Mesh Model
    mod_ble_mesh_model_class_t* self = (mod_ble_mesh_model_class_t*)args[0].u_obj;

    // Fetch parameters
    uint16_t addr = args[1].u_int;
    uint16_t app_idx = args[2].u_int;
    mp_obj_t state_type = args[3].u_obj;

    if(self->server_client == MOD_BLE_MESH_SERVER) {

        // Fetch default opcode
        int32_t opcode = self->op_def_status;

        if(state_type != mp_const_none) {
            // Get opcode if state is defined
            opcode = opcode_table[self->type + mp_obj_get_int(state_type) + MOD_BLE_MESH_REQ_STATE];
        }

        if(opcode == -1) {
            // Error if Status request is not possible on state
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Cannot send Status of this State!"));
        }

        // Setup CTX
        esp_ble_mesh_msg_ctx_t ctx;
        ctx.net_idx = 0;
        ctx.app_idx = app_idx;
        ctx.addr = addr;
        ctx.send_ttl = 3;
        ctx.send_rel = false;

        if(self->group == MOD_BLE_MESH_GROUP_GENERIC) {

            if(self->type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
                // Send Status of Generic OnOff Server State
                esp_ble_mesh_gen_onoff_srv_t *srv = self->element->element->sig_models[self->index].user_data;
                esp_ble_mesh_server_model_send_msg(&self->element->element->sig_models[self->index], &ctx, opcode, sizeof(srv->state.onoff), &srv->state.onoff);
            }
            else if(self->type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
                // Send Status of Generic Level Server State
                esp_ble_mesh_gen_level_srv_t *srv = self->element->element->sig_models[self->index].user_data;
                esp_ble_mesh_server_model_send_msg(&self->element->element->sig_models[self->index], &ctx, opcode, sizeof(srv->state.level), &srv->state.level);
            }
        }
        else if(self->group == MOD_BLE_MESH_GROUP_SENSOR) {

            uint8_t *status = NULL;
            uint16_t buf_size = 0;
            uint16_t length = 0;

            esp_ble_mesh_sensor_state_t *state = &sensor_states[0];

            if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
                buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
            } else {
                /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
                if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
                    buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN + state->sensor_data.length + 1;
                } else {
                    buf_size += ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN + state->sensor_data.length + 1;
                }
            }

            status = calloc(1, buf_size);
            length = example_ble_mesh_get_sensor_data(&sensor_states[0], status);

            esp_ble_mesh_server_model_send_msg(&self->element->element->sig_models[self->index], &ctx, ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS, length, status);
            free(status);
        }

        return mp_const_none;

    } else {
        // Error if State request is not possible on state
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Cannot Send Status from Client!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_model_status_state_obj, 1, mod_ble_mesh_model_status_state);

STATIC const mp_map_elem_t mod_ble_mesh_model_locals_dict_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh_Model) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_get_state),                       (mp_obj_t)&mod_ble_mesh_model_get_state_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_set_state),                       (mp_obj_t)&mod_ble_mesh_model_set_state_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_status_state),                    (mp_obj_t)&mod_ble_mesh_model_status_state_obj },

};
STATIC MP_DEFINE_CONST_DICT(mod_ble_mesh_model_locals_dict, mod_ble_mesh_model_locals_dict_table);

static const mp_obj_type_t mod_ble_mesh_model_type = {
    { &mp_type_type },
    .name = MP_QSTR_BLE_Mesh_Model,
    .locals_dict = (mp_obj_t)&mod_ble_mesh_model_locals_dict,
};
/******************************************************************************
 DEFINE BLE MESH ELEMENT FUNCTIONS
 ******************************************************************************/

static uint16_t example_ble_mesh_get_sensor_data(esp_ble_mesh_sensor_state_t *state, uint8_t *data)
{
    uint8_t mpid_len = 0, data_len = 0;
    uint32_t mpid = 0;

    if (state == NULL || data == NULL) {
        printf("SInvalid parameter\n");
        return 0;
    }

    if (state->sensor_data.length == ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN) {
        /* For zero-length sensor data, the length is 0x7F, and the format is Format B. */
        mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
        mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        data_len = 0;
    } else {
        if (state->sensor_data.format == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A) {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN;
        } else {
            mpid = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID(state->sensor_data.length, state->sensor_property_id);
            mpid_len = ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN;
        }
        /* Use "state->sensor_data.length + 1" because the length of sensor data is zero-based. */
        data_len = state->sensor_data.length + 1;
    }

    memcpy(data, &mpid, mpid_len);
    memcpy(data + mpid_len, state->sensor_data.raw_value->data, data_len);

    return (mpid_len + data_len);
}

STATIC mp_obj_t mod_ble_mesh_element_add_model(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_add_model_args[] = {
            { MP_QSTR_self_in,               MP_ARG_OBJ,                                                       },
            { MP_QSTR_type,                  MP_ARG_INT,                  {.u_int = MOD_BLE_MESH_MODEL_GEN_ONOFF}},
            { MP_QSTR_server_client,         MP_ARG_INT,                  {.u_int = MOD_BLE_MESH_SERVER}},
            { MP_QSTR_callback,              MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
            { MP_QSTR_value,                 MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
            { MP_QSTR_sen_min,               MP_ARG_OBJ,                  {.u_int = MP_OBJ_NULL}},
            { MP_QSTR_sen_max,               MP_ARG_OBJ,                  {.u_int = MP_OBJ_NULL}},
            { MP_QSTR_sen_res,               MP_ARG_OBJ,                  {.u_int = MP_OBJ_NULL}},

    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_add_model_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_add_model_args, args);

    mod_ble_mesh_element_class_t* ble_mesh_element = (mod_ble_mesh_element_class_t*)args[0].u_obj;
    mp_int_t type = args[1].u_int;
    // TODO: check the range, only SERVER or CLIENT is allowed
    mp_int_t server_client = args[2].u_int;
    mp_obj_t callback = args[3].u_obj;
    mp_obj_t value_mp_obj = args[4].u_obj;

    mod_ble_mesh_sen_rep.sen_min = args[5].u_obj == MP_OBJ_NULL ? -100 : mp_obj_get_float(args[5].u_obj);
    mod_ble_mesh_sen_rep.sen_max = args[6].u_int == MP_OBJ_NULL ?  100 : mp_obj_get_float(args[6].u_obj);
    mod_ble_mesh_sen_rep.sen_res = args[7].u_int == MP_OBJ_NULL ?  0.01: mp_obj_get_float(args[7].u_obj);

    int16_t group = 0;

    // Start preparing the Model
    esp_ble_mesh_model_t tmp_model;
    int32_t op_def_get = -1;
    int32_t op_def_set = -1;
    int32_t op_def_set_unack = -1;
    int32_t op_def_status = -1;

    esp_ble_mesh_model_pub_t* pub = (esp_ble_mesh_model_pub_t *)heap_caps_malloc(sizeof(esp_ble_mesh_model_pub_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // Allocate 20 byte for the buffer, this size was taken from the examples
    uint8_t* net_buf_data = (uint8_t *)heap_caps_malloc(20, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    struct net_buf_simple* net_buf_simple_ptr = (struct net_buf_simple *)heap_caps_malloc(sizeof(struct net_buf_simple), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    net_buf_simple_ptr->__buf = net_buf_data;
    net_buf_simple_ptr->data = net_buf_data;
    net_buf_simple_ptr->len = 0;
    net_buf_simple_ptr->size = 20;
    pub->update = 0;
    pub->msg = net_buf_simple_ptr;
    pub->dev_role = ROLE_NODE;

    // Server Type
    if(server_client == MOD_BLE_MESH_SERVER) {
        if(type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
            //TODO: check the validity of the value
            // This will be saved into the Model's user_data field
            esp_ble_mesh_gen_onoff_srv_t* onoff_server = (esp_ble_mesh_gen_onoff_srv_t *)heap_caps_malloc(sizeof(esp_ble_mesh_gen_onoff_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            onoff_server->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
            onoff_server->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

            esp_ble_mesh_model_t gen_onoff_srv_mod = ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(pub, onoff_server);
            memcpy(&tmp_model, &gen_onoff_srv_mod, sizeof(gen_onoff_srv_mod));

            op_def_status = BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS;

            group = MOD_BLE_MESH_GROUP_GENERIC;
        }
        else if(type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
            //TODO: check the validity of the value
            // This will be saved into the Model's user_data field
            esp_ble_mesh_gen_level_srv_t* level_server = (esp_ble_mesh_gen_level_srv_t *)heap_caps_malloc(sizeof(esp_ble_mesh_gen_level_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            level_server->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
            level_server->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

            esp_ble_mesh_model_t gen_level_srv_mod = ESP_BLE_MESH_MODEL_GEN_LEVEL_SRV(pub, level_server);
            memcpy(&tmp_model, &gen_level_srv_mod, sizeof(gen_level_srv_mod));

            op_def_status = BLE_MESH_MODEL_OP_GEN_LEVEL_STATUS;

            group = MOD_BLE_MESH_GROUP_GENERIC;
        }
        else if(type == MOD_BLE_MESH_MODEL_SENSOR) {
            //esp_ble_mesh_sensor_srv_t* sensors_server = (esp_ble_mesh_sensor_srv_t *)heap_caps_malloc(sizeof(esp_ble_mesh_sensor_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            //sensors_server->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP;
            //sensors_server->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP;

            //esp_ble_mesh_model_t sen_srv_mod = ESP_BLE_MESH_MODEL_SENSOR_SRV(pub, &sensors_server);
            esp_ble_mesh_model_t sen_srv_mod = ESP_BLE_MESH_MODEL_SENSOR_SRV(&sensor_pub, &sensor_server);
            memcpy(&tmp_model, &sen_srv_mod, sizeof(sen_srv_mod));

            op_def_status = BLE_MESH_MODEL_OP_SENSOR_STATUS;

            group = MOD_BLE_MESH_GROUP_SENSOR;
        }
        else if(type == MOD_BLE_MESH_MODEL_SENSOR_SETUP) {
            //esp_ble_mesh_sensor_setup_srv_t* sensors_setup_server = (esp_ble_mesh_sensor_setup_srv_t *)heap_caps_malloc(sizeof(esp_ble_mesh_sensor_setup_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            //sensors_setup_server->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
            //sensors_setup_server->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

            //esp_ble_mesh_model_t sen_setup_srv_mod = ESP_BLE_MESH_MODEL_SENSOR_SETUP_SRV(pub, sensors_setup_server);
            //memcpy(&tmp_model, &sen_setup_srv_mod, sizeof(sen_setup_srv_mod));
            esp_ble_mesh_model_t sen_stp_srv_mod = ESP_BLE_MESH_MODEL_SENSOR_SETUP_SRV(&sensor_setup_pub, &sensor_setup_server);
            memcpy(&tmp_model, &sen_stp_srv_mod, sizeof(sen_stp_srv_mod));

            op_def_status = BLE_MESH_MODEL_OP_SENSOR_CADENCE_STATUS;

            group = MOD_BLE_MESH_GROUP_SENSOR;
        }
        else {
            //TODO: Add support for more functionality
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Not supported Model type!!"));
        }
    }
    else {
        if(type == MOD_BLE_MESH_MODEL_GEN_ONOFF) {
            // This will be saved into the Model's user_data field
            esp_ble_mesh_client_t* onoff_client = (esp_ble_mesh_client_t *)heap_caps_calloc(1, sizeof(esp_ble_mesh_client_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            esp_ble_mesh_model_t gen_onoff_cli_mod = ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(pub, onoff_client);
            memcpy(&tmp_model, &gen_onoff_cli_mod, sizeof(gen_onoff_cli_mod));

            op_def_get = BLE_MESH_MODEL_OP_GEN_ONOFF_GET;
            op_def_set = BLE_MESH_MODEL_OP_GEN_ONOFF_SET;
            op_def_set_unack = BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;

            group = MOD_BLE_MESH_GROUP_GENERIC;
        }
        else if(type == MOD_BLE_MESH_MODEL_GEN_LEVEL) {
            // This will be saved into the Model's user_data field
            esp_ble_mesh_client_t* level_client = (esp_ble_mesh_client_t *)heap_caps_calloc(1, sizeof(esp_ble_mesh_client_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            esp_ble_mesh_model_t gen_level_cli_mod = ESP_BLE_MESH_MODEL_GEN_LEVEL_CLI(pub, level_client);
            memcpy(&tmp_model, &gen_level_cli_mod, sizeof(gen_level_cli_mod));

            op_def_get = BLE_MESH_MODEL_OP_GEN_LEVEL_GET;
            op_def_set = BLE_MESH_MODEL_OP_GEN_LEVEL_SET;
            op_def_set_unack = BLE_MESH_MODEL_OP_GEN_LEVEL_SET_UNACK;

            group = MOD_BLE_MESH_GROUP_GENERIC;
        }
        else if(type == MOD_BLE_MESH_MODEL_SENSOR) {
            //esp_ble_mesh_client_t* sensors_client = (esp_ble_mesh_client_t *)heap_caps_calloc(1, sizeof(esp_ble_mesh_client_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            esp_ble_mesh_model_t sen_cli_mod = ESP_BLE_MESH_MODEL_SENSOR_CLI(NULL, &sensor_client);
            memcpy(&tmp_model, &sen_cli_mod, sizeof(sen_cli_mod));

            op_def_get = BLE_MESH_MODEL_OP_SENSOR_GET;

            group = MOD_BLE_MESH_GROUP_SENSOR;
        }
        else {
            //TODO: Add support for more functionality
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Not supported Model type!!"));
        }
    }

    // Allocate memory for the new model, use realloc because the underlying BLE Mesh library expects the Models as an array, and not as a list
    ble_mesh_element->element->sig_models = (esp_ble_mesh_model_t*)heap_caps_realloc(ble_mesh_element->element->sig_models, (ble_mesh_element->element->sig_model_count+1) * sizeof(esp_ble_mesh_model_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    memcpy(&ble_mesh_element->element->sig_models[ble_mesh_element->element->sig_model_count], &tmp_model, sizeof(tmp_model));

    // Create the MicroPython Model
    mod_ble_mesh_model_class_t* model = mod_ble_add_model_to_list(ble_mesh_element,
                                                                  ble_mesh_element->element->sig_model_count,
                                                                  callback,
                                                                  op_def_get,
                                                                  op_def_set,
                                                                  op_def_set_unack,
                                                                  op_def_status,
                                                                  value_mp_obj,
                                                                  server_client,
                                                                  type,
                                                                  group);

    // Indicate we have a new element in the array
    ble_mesh_element->element->sig_model_count++;

    // Return with the MicroPtyhon Model object
    return model;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_element_add_model_obj, 1, mod_ble_mesh_element_add_model);

STATIC const mp_map_elem_t mod_ble_mesh_element_locals_dict_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                     MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh_Element) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_add_model),                    (mp_obj_t)&mod_ble_mesh_element_add_model_obj },

};
STATIC MP_DEFINE_CONST_DICT(mod_ble_mesh_element_locals_dict, mod_ble_mesh_element_locals_dict_table);

static const mp_obj_type_t mod_ble_mesh_element_type = {
    { &mp_type_type },
    .name = MP_QSTR_BLE_Mesh_Element,
    .locals_dict = (mp_obj_t)&mod_ble_mesh_element_locals_dict,
};


/******************************************************************************
 DEFINE BLE MESH CLASS FUNCTIONS
 ******************************************************************************/

// TODO: add parameters
// Initialize the module
STATIC mp_obj_t mod_ble_mesh_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t mod_ble_mesh_init_args[] = {
            { MP_QSTR_name,                  MP_ARG_OBJ,                  {.u_obj = MP_OBJ_NULL}},
            { MP_QSTR_auth,                  MP_ARG_INT | MP_ARG_KW_ONLY, {.u_int = 0}},
            { MP_QSTR_callback,              MP_ARG_OBJ  | MP_ARG_KW_ONLY,{.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_init_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_init_args, args);

    // The BLE Mesh module should be initialized only once
    if(initialized == false) {
        esp_err_t err;

        provision_ptr = (esp_ble_mesh_prov_t *)heap_caps_malloc(sizeof(esp_ble_mesh_prov_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        composition_ptr = (esp_ble_mesh_comp_t *)heap_caps_malloc(sizeof(esp_ble_mesh_comp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        if(provision_ptr != NULL && composition_ptr != NULL) {

            // Set UUID
            memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
            provision_ptr->uuid = dev_uuid;

            // Initiate parameters with NO OOB case
            provision_ptr->input_size = 0;
            provision_ptr->input_actions = ESP_BLE_MESH_NO_INPUT;

            provision_ptr->output_size = 0;
            provision_ptr->output_actions = ESP_BLE_MESH_NO_OUTPUT;

            // GET auth information
            int oob_type = args[1].u_int;

            if(oob_type & MOD_BLE_MESH_INPUT_OOB) {
                provision_ptr->input_size = 1;
                provision_ptr->input_actions = ESP_BLE_MESH_PUSH;
            }
            if(oob_type & MOD_BLE_MESH_OUTPUT_OOB) {
                provision_ptr->output_size = 1;
                provision_ptr->output_actions = ESP_BLE_MESH_BLINK;
            }

            //TODO: initialize composition based on input parameters
            composition_ptr->cid = 0x02C4;  // CID_ESP=0x02C4
            // TODO: add support for more Elements
            composition_ptr->elements = (esp_ble_mesh_elem_t *)mod_ble_mesh_element.element;
            composition_ptr->element_count = 1;

            // Get MP callback
            provision_callback = args[2].u_obj;

            // Register Generic Callbacks
            esp_ble_mesh_register_generic_client_callback(mod_ble_mesh_generic_client_callback);
            esp_ble_mesh_register_generic_server_callback(mod_ble_mesh_generic_server_callback);

            // Register Sensor Callbacks
            esp_ble_mesh_register_sensor_server_callback(mod_ble_mesh_sensor_server_callback);
            esp_ble_mesh_register_sensor_client_callback(mod_ble_mesh_sensor_client_callback);

            // Register Config Server Callback
            esp_ble_mesh_register_config_server_callback(mod_ble_mesh_config_server_callback);

            // Register Provision Callback
            esp_ble_mesh_register_prov_callback(mod_ble_mesh_provision_callback);

            if(args[0].u_obj != MP_OBJ_NULL) {
                err = esp_ble_mesh_set_unprovisioned_device_name(mp_obj_str_get_str(args[0].u_obj));
            }
            else {
                err = esp_ble_mesh_set_unprovisioned_device_name(MOD_BLE_MESH_DEFAULT_NAME);
            }

            if(err != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "BLE Mesh node name cannot be set, error code: %d!", err));
            }

            err = esp_ble_mesh_init(provision_ptr, composition_ptr);

            if(err != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "BLE Mesh module could not be initialized, error code: %d!", err));
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

    net_buf_simple_push_u8(&sensor_data, 0);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_init_obj, 0, mod_ble_mesh_init);

// Set node provisioning
STATIC mp_obj_t mod_ble_mesh_set_node_prov(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_set_node_prov_args[] = {
            { MP_QSTR_bearer,                MP_ARG_INT,                   {.u_int = 4}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_set_node_prov_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_set_node_prov_args, args);

    // Get bearer type
    int type = args[0].u_int;

    // Disable node provision in prior
    esp_ble_mesh_node_prov_disable(MOD_BLE_MESH_PROV_ADV|MOD_BLE_MESH_PROV_GATT);

    // Check if provision mode is within valid range
    if((type >= MOD_BLE_MESH_PROV_ADV) && (type <= (MOD_BLE_MESH_PROV_ADV|MOD_BLE_MESH_PROV_GATT))) {
        esp_ble_mesh_node_prov_enable(type);
    }
    else if(type != MOD_BLE_MESH_PROV_NONE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Node provision mode is not valid!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_set_node_prov_obj, 0, mod_ble_mesh_set_node_prov);

// Reset node provisioning information
STATIC mp_obj_t mod_ble_mesh_reset_node_prov(void) {

    // Delete and reset node provision information
    esp_ble_mesh_node_local_reset();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_ble_mesh_reset_node_prov_obj, mod_ble_mesh_reset_node_prov);

// Set node provisioning
STATIC mp_obj_t mod_ble_mesh_input_oob(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_input_oob_args[] = {
            { MP_QSTR_oob,                   MP_ARG_OBJ,                   {.u_obj = MP_OBJ_NULL}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_input_oob_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_input_oob_args, args);

    if(args[0].u_obj != MP_OBJ_NULL) {
        if(mp_obj_is_int(args[0].u_obj)) {
            esp_ble_mesh_node_input_number(mp_obj_get_int(args[0].u_obj));
        }
        else if(mp_obj_is_str(args[0].u_obj)) {
            esp_ble_mesh_node_input_string(mp_obj_get_type_str(args[0].u_obj));
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_input_oob_obj, 0, mod_ble_mesh_input_oob);

// TODO: add parameters for configuring the Configuration Server Model
STATIC mp_obj_t mod_ble_mesh_create_element(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_ble_mesh_create_element_args[] = {
            { MP_QSTR_primary,               MP_ARG_BOOL | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
            { MP_QSTR_feature,               MP_ARG_INT  | MP_ARG_KW_ONLY, {.u_int = 0}},
            { MP_QSTR_beacon,                MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true}},
            { MP_QSTR_ttl,                   MP_ARG_INT  | MP_ARG_KW_ONLY, {.u_int = 7}},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(mod_ble_mesh_create_element_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_ble_mesh_create_element_args, args);

    // Get primary bool
    bool primary = args[0].u_bool;

    if(primary) {
        // TODO: check here if not other primary element exists
        if(1) {
            // Get Configuration Server Model parameters
            int feature = args[1].u_int;
            bool beacon = args[2].u_bool;
            int ttl = args[3].u_int;

            //TODO: add support for several more elements
            mod_ble_mesh_element_class_t *ble_mesh_element = &mod_ble_mesh_element;

            // Initiate an empty element
            ble_mesh_element->base.type = &mod_ble_mesh_element_type;
            ble_mesh_element->element = (mod_ble_mesh_elem_t *)heap_caps_calloc(1, sizeof(mod_ble_mesh_elem_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

            // Add the mandatory Configuration Server Model
            esp_ble_mesh_cfg_srv_t* configuration_server_model_ptr = (esp_ble_mesh_cfg_srv_t *)heap_caps_calloc(1, sizeof(esp_ble_mesh_cfg_srv_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

            // Configure the Configuration Server Model
            configuration_server_model_ptr->relay = ((feature & MOD_BLE_MESH_RELAY) > 0);
            configuration_server_model_ptr->friend_state = ((feature & MOD_BLE_MESH_FRIEND) > 0);
            configuration_server_model_ptr->gatt_proxy = ((feature & MOD_BLE_MESH_GATT_PROXY) > 0);

            configuration_server_model_ptr->beacon = beacon;
            configuration_server_model_ptr->default_ttl = ttl;

            configuration_server_model_ptr->net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20); /* 3 transmissions with 20ms interval */
            configuration_server_model_ptr->relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20);

            // Prepare temp model
            esp_ble_mesh_model_t tmp_model = ESP_BLE_MESH_MODEL_CFG_SRV(configuration_server_model_ptr);

            // Allocate memory for the model
            ble_mesh_element->element->sig_models = (esp_ble_mesh_model_t*)heap_caps_calloc(1, sizeof(esp_ble_mesh_model_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            // Copy the already prepared element to the new location
            memcpy(ble_mesh_element->element->sig_models, &tmp_model, sizeof(esp_ble_mesh_model_t));

            // Create the MicroPython Model
            // TODO: add callback if the user configures it
            (void)mod_ble_add_model_to_list(ble_mesh_element, 0, NULL, 0, 0, 0, 0, NULL, MOD_BLE_MESH_SERVER, MOD_BLE_MESH_MODEL_NONE, MOD_BLE_MESH_MODEL_CONFIGURATION);
            // This is the first model
            ble_mesh_element->element->sig_model_count = 1;

            return ble_mesh_element;
        }
        else {
            // TODO: add support for more elements
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Only one Element can be created at this version!"));

            return mp_const_none;
        }
    }
    else {
        // TODO: add secondary element
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Only primary element is supported!"));
        //Return none until not implemented scenario
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ble_mesh_create_element_obj, 0, mod_ble_mesh_create_element);


STATIC const mp_map_elem_t mod_ble_mesh_globals_table[] = {
        { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                       MP_OBJ_NEW_QSTR(MP_QSTR_BLE_Mesh) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_init),                           (mp_obj_t)&mod_ble_mesh_init_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_set_node_prov),                  (mp_obj_t)&mod_ble_mesh_set_node_prov_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_reset_node_prov),                (mp_obj_t)&mod_ble_mesh_reset_node_prov_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_input_oob),                      (mp_obj_t)&mod_ble_mesh_input_oob_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_create_element),                 (mp_obj_t)&mod_ble_mesh_create_element_obj },

        // Constants of Advertisement
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_ADV),                       MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_PROV_ADV) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_GATT),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_PROV_GATT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_NONE),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_PROV_NONE) },
        // Constants of Node Features
        { MP_OBJ_NEW_QSTR(MP_QSTR_RELAY),                          MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_RELAY) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LOW_POWER),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_LOW_POWER) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_GATT_PROXY),                     MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_GATT_PROXY) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_FRIEND),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_FRIEND) },
        // Constants of Authentication
        { MP_OBJ_NEW_QSTR(MP_QSTR_OOB_INPUT),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_INPUT_OOB) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_OOB_OUTPUT),                     MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_OUTPUT_OOB) },
        // Constants of Node Addresses
        { MP_OBJ_NEW_QSTR(MP_QSTR_ADDR_ALL_NODES),                 MP_OBJ_NEW_SMALL_INT(MOD_BLE_ADDR_ALL_NODES) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_ADDR_PUBLISH),                   MP_OBJ_NEW_SMALL_INT(MOD_BLE_ADDR_PUBLISH) },

        // Constants of Server-Client
        { MP_OBJ_NEW_QSTR(MP_QSTR_SERVER),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_SERVER) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_CLIENT) },

        // Constants of States
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_ONOFF),                    MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_ONOFF) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_LEVEL),                    MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_LEVEL) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_LEVEL_DELTA),              MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_LEVEL_DELTA) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_LEVEL_MOVE),               MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_LEVEL_MOVE) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_DESCRIPTOR),           MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_DESCRIPTOR) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_COLUMN),               MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_COLUMN) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_SERIES),               MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_SERIES) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_SET_CADENCE),          MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_SET_CADENCE) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_SET_SETTINGS),         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_SET_SETTINGS) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_STATE_SEN_SET_SETTING),          MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_STATE_SEN_SET_SETTING) },

        // Models
        { MP_OBJ_NEW_QSTR(MP_QSTR_GEN_ONOFF),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_GEN_ONOFF) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_GEN_LEVEL),                      MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_GEN_LEVEL) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_SENSOR),                         MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_SENSOR) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_SENSOR_SETUP),                   MP_OBJ_NEW_SMALL_INT(MOD_BLE_MESH_MODEL_SENSOR_SETUP) },

        // Provisioning Events
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_REGISTER_EVT),              MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_PROV_REGISTER_COMP_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_ENABLE_EVT),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_DISABLE_EVT),               MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_DISABLE_COMP_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LINK_OPEN_EVT),                  MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_LINK_CLOSE_EVT),                 MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_COMPLETE_EVT),              MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_RESET_EVT),                 MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_RESET_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_OUTPUT_OOB_REQ_EVT),        MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_OUTPUT_NUMBER_EVT) },
        { MP_OBJ_NEW_QSTR(MP_QSTR_PROV_INPUT_OOB_REQ_EVT),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_MESH_NODE_PROV_INPUT_EVT) },

};

STATIC MP_DEFINE_CONST_DICT(mod_ble_mesh_globals, mod_ble_mesh_globals_table);

// Sub-module of Bluetooth module (modbt.c)
const mp_obj_module_t mod_ble_mesh = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_ble_mesh_globals,
};
