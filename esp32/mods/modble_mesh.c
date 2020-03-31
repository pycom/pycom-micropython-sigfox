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
static esp_ble_mesh_comp_t *component_ptr;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE BLE MESH CLASS FUNCTIONS
 ******************************************************************************/

// Initialize the module
STATIC mp_obj_t mod_ble_mesh_init() {

    // The BLE Mesh module should be initialized only once
    if(initialized == false) {
        provision_ptr = (esp_ble_mesh_prov_t *)heap_caps_malloc(sizeof(esp_ble_mesh_prov_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        component_ptr = (esp_ble_mesh_comp_t *)heap_caps_malloc(sizeof(esp_ble_mesh_comp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        //TODO: initialize provision and component based on input parameters

        if(provision_ptr != NULL && component_ptr != NULL) {
            esp_err_t err = esp_ble_mesh_init(provision_ptr, component_ptr);
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
