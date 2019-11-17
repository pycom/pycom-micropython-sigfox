/*
 * Copyright (c) 2019, Pycom Limited.
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

#include "mdns.h"

#include "modmdns.h"
#include "modnetwork.h"


/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MOD_MDNS_PROTO_TCP      (0)
#define MOD_MDNS_PROTO_UDP      (1)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
STATIC bool initialized = false;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE MDNS CLASS FUNCTIONS
 ******************************************************************************/

// Initialize the module and start advertisement
STATIC mp_obj_t mod_mdns_init() {

    // The MDNS module should be initialized only once
    if(initialized == false) {

        //initialize mDNS service
        esp_err_t ret = mdns_init();
        if (ret != ESP_OK) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "MDNS module could not be initialized, error code: %d", ret));
            // Just to fulfill the compiler's needs
            return mp_const_none;
        }

        initialized = true;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "MDNS module already initialized!"));
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_mdns_init_obj, mod_mdns_init);

// Deinitialize the module and stop advertisement
STATIC mp_obj_t mod_mdns_deinit() {

    if(initialized == true) {

        mdns_service_remove_all();
        mdns_free();
        initialized = false;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_mdns_deinit_obj, mod_mdns_deinit);


// Set the host name and instance name of the device to advertise
STATIC mp_obj_t mod_mdns_set_name(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_mdns_set_name_args[] = {
            { MP_QSTR_host_name,                 MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
            { MP_QSTR_instance_name,             MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}}
    };

    if(initialized == true) {

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_mdns_set_name_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_mdns_set_name_args, args);

        //set host name if specified
        if(args[0].u_obj != MP_OBJ_NULL) {
            const char* host_name = mp_obj_str_get_str(args[0].u_obj);
            mdns_hostname_set(host_name);
        }

        //set instance name if specified
        if(args[1].u_obj != MP_OBJ_NULL) {
            const char* instance_name = mp_obj_str_get_str(args[1].u_obj);
            mdns_instance_name_set(instance_name);
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "MDNS module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_mdns_set_name_obj, 0, mod_mdns_set_name);

// Add a new service to advertise
STATIC mp_obj_t mod_mdns_add_service(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_mdns_add_service_args[] = {
            { MP_QSTR_service_type,             MP_ARG_OBJ  | MP_ARG_REQUIRED, },
            { MP_QSTR_proto,                    MP_ARG_INT  | MP_ARG_REQUIRED, },
            { MP_QSTR_port,                     MP_ARG_INT  | MP_ARG_REQUIRED, },
            { MP_QSTR_text,                     MP_ARG_OBJ  | MP_ARG_KW_ONLY,  {.u_obj = MP_OBJ_NULL}},
    };

    if(initialized == true) {

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_mdns_add_service_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_mdns_add_service_args, args);

        // Get service type
        const char* service_type = mp_obj_str_get_str(args[0].u_obj);
        if(service_type == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "service_type must be a valid string"));
        }

        // Get protocol
        mp_int_t proto_num = args[1].u_int;
        if(proto_num != MOD_MDNS_PROTO_TCP && proto_num != MOD_MDNS_PROTO_UDP) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "proto must be 0 (TCP) or 1 (UDP)"));
        }
        const char* proto = proto_num == MOD_MDNS_PROTO_TCP ? "_tcp" : "_udp";

        // Get port number
        mp_int_t port = args[2].u_int;


//        mdns_txt_item_t* service_text_data_array;
//        mp_int_t item_count = 0;
//
//        mp_obj_iter_buf_t iter_buf;
//        mp_obj_t iterable = mp_getiter(args[3].u_obj, &iter_buf);
//        mp_obj_t item;
//        while ((item = mp_iternext(iterable)) != MP_OBJ_STOP_ITERATION) {
//           if (mp_obj_is_true(item)) {
//               item_count++;
//           }
//        }

//        service_text_data_array = m_malloc(item_count * sizeof(mdns_txt_item_t));
//
//        item_count = 0;
//        iterable = mp_getiter(args[3].u_obj, &iter_buf);
//        while ((item = mp_iternext(iterable)) != MP_OBJ_STOP_ITERATION) {
//           if (mp_obj_is_true(item)) {
//
//               mp_obj_tuple_get(item, &data_length, &data_ptr);
//
//               service_text_data_array[item_count].key
//
//               item_count++;
//           }
//        }

//        mdns_txt_item_t serviceTxtData[3] = {
//            {"board","esp32"},
//            {"u","user"},
//            {"p","password"}
//        };

        esp_err_t ret = mdns_service_add(NULL, service_type, proto, port, NULL, 0);
        if(ret != ESP_OK) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Service could not be added, error code: %d", ret));
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "MDNS module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_mdns_add_service_obj, 3, mod_mdns_add_service);

// Remove a service
STATIC mp_obj_t mod_mdns_remove_service(mp_obj_t service_type_in, mp_obj_t proto_in) {

    if(initialized == true) {

        const char* service_type = mp_obj_str_get_str(service_type_in);
        if(service_type == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "service_type must be a valid string"));
        }

        mp_int_t proto_num = mp_obj_get_int(proto_in);
        if(proto_num != MOD_MDNS_PROTO_TCP && proto_num != MOD_MDNS_PROTO_UDP) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "proto must be 0 (TCP) or 1 (UDP)"));
        }
        const char* proto = proto_num == MOD_MDNS_PROTO_TCP ? "_tcp" : "_udp";

        esp_err_t ret = mdns_service_remove(service_type, proto);
        if(ret != ESP_OK) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Service could not be removed, error code: %d", ret));
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "MDNS module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_mdns_remove_service_obj, mod_mdns_remove_service);


STATIC const mp_map_elem_t mod_mdns_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_mdns) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_mdns_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                          (mp_obj_t)&mod_mdns_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_name),                        (mp_obj_t)&mod_mdns_set_name_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_service),                     (mp_obj_t)&mod_mdns_add_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remove_service),                  (mp_obj_t)&mod_mdns_remove_service_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROTO_TCP),                     MP_OBJ_NEW_SMALL_INT(MOD_MDNS_PROTO_TCP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROTO_UDP),                     MP_OBJ_NEW_SMALL_INT(MOD_MDNS_PROTO_UDP) },
};

STATIC MP_DEFINE_CONST_DICT(mod_mdns_globals, mod_mdns_globals_table);

const mp_obj_module_t mod_mdns = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_mdns_globals,
};
