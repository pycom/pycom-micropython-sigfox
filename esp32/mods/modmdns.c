/*
 * Copyright (c) 2021, Pycom Limited.
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

#include "mdns.h"
#include "netutils.h"

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
typedef struct mod_mdns_query_obj_s {
    mp_obj_base_t base;
    mp_obj_t instance_name;       /* instance name */
    mp_obj_t hostname;            /* hostname */
    mp_obj_t port;                /* service port */
    mp_obj_t txt;                 /* txt record */
    mp_obj_t addr;                /* linked list of IP addresses found */
}mod_mdns_query_obj_t;
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
STATIC bool initialized = false;
STATIC const mp_obj_type_t mod_mdns_query_type;

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
            { MP_QSTR_hostname,                 MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
            { MP_QSTR_instance_name,            MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}}
    };

    if(initialized == true) {

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_mdns_set_name_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_mdns_set_name_args, args);

        //set host name if specified
        if(args[0].u_obj != MP_OBJ_NULL) {
            const char* hostname = mp_obj_str_get_str(args[0].u_obj);
            mdns_hostname_set(hostname);
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
            { MP_QSTR_txt,                     MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
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

        // Get txt
        mdns_txt_item_t* service_txt = NULL;
        size_t length_total = 0;

        if(args[3].u_obj != MP_OBJ_NULL) {
            mp_obj_t* items;
            mp_obj_tuple_get(args[3].u_obj, &length_total, &items);

            service_txt = malloc(length_total * sizeof(mdns_txt_item_t));

            for(int i = 0; i < length_total; i++) {
                size_t length_elem = 0;
                mp_obj_t* item;
                mp_obj_tuple_get(items[i], &length_elem, &item);

                // There should be only key-value pair here
                if(length_elem != 2) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Txt must contain key-value pair strings"));
                }

                service_txt[i].key = (char*)mp_obj_str_get_str(item[0]);
                service_txt[i].value = (char*)mp_obj_str_get_str(item[1]);
            }
        }

        esp_err_t ret = mdns_service_add(NULL, service_type, proto, port, service_txt, length_total);
        free(service_txt);
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


// Initiate a new query
STATIC mp_obj_t mod_mdns_query(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_mdns_query_args[] = {
            { MP_QSTR_timeout,                  MP_ARG_INT  | MP_ARG_REQUIRED, },
            { MP_QSTR_service_type,             MP_ARG_OBJ  | MP_ARG_REQUIRED, },
            { MP_QSTR_proto,                    MP_ARG_INT  | MP_ARG_REQUIRED, },
            { MP_QSTR_instance_name,            MP_ARG_OBJ  | MP_ARG_KW_ONLY,  {.u_obj = MP_OBJ_NULL}},
    };

    if(initialized == true) {

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_mdns_query_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_mdns_query_args, args);

        // Get timeout value
        uint32_t timeout = args[0].u_int;

        // Get service type
        const char * service_type = mp_obj_str_get_str(args[1].u_obj);

        // Get proto
        const char * proto = NULL;
        mp_int_t proto_num = args[2].u_int;
        if(proto_num != MOD_MDNS_PROTO_TCP && proto_num != MOD_MDNS_PROTO_UDP) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "proto must be 0 (TCP) or 1 (UDP)"));
        }
        proto = proto_num == MOD_MDNS_PROTO_TCP ? "_tcp" : "_udp";

        // Get instance name
        const char * instance_name = NULL;
        if(args[3].u_obj != NULL) {
            instance_name = mp_obj_str_get_str(args[3].u_obj);
        }


        mdns_result_t *results;
        esp_err_t ret;

        if(instance_name == NULL) {
            ret = mdns_query_ptr(service_type, proto, timeout, 10, &results);
        }
        else {
            ret = mdns_query_srv(instance_name, service_type, proto, timeout, &results);
        }


        if(ret != ESP_OK) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Query failed, error code: %d", ret));
        }
        else {
            mdns_result_t *result = results;
            mp_obj_t queries_list = mp_obj_new_list(0, NULL);
            while(result != NULL) {
                mod_mdns_query_obj_t *query_obj = m_new(mod_mdns_query_obj_t, 1);
                query_obj->base.type = (mp_obj_t)&mod_mdns_query_type;
                query_obj->instance_name = mp_obj_new_str(result->instance_name, strlen(result->instance_name));
                query_obj->hostname = mp_obj_new_str(result->hostname, strlen(result->hostname));
                query_obj->port = mp_obj_new_int(result->port);

                query_obj->txt = mp_obj_new_list(0, NULL);
                for(int i = 0; i < result->txt_count; i++) {
                    mp_obj_t tuple[2];
                    tuple[0] = mp_obj_new_str(result->txt[i].key, strlen(result->txt[i].key));
                    tuple[1] = mp_obj_new_str(result->txt[i].value, strlen(result->txt[i].value));
                    mp_obj_list_append(query_obj->txt, mp_obj_new_tuple(2, tuple));
                }

                if (result->addr) {
                    query_obj->addr = netutils_format_ipv4_addr((uint8_t *)&result->addr->addr.u_addr.ip4.addr, NETUTILS_BIG);
                } else {
                    u32_t zero_ip = 0;
                    query_obj->addr = netutils_format_ipv4_addr((uint8_t *)&zero_ip, NETUTILS_BIG);
                }

                mp_obj_list_append(queries_list, query_obj);

                result = result->next;
            }

            mdns_query_results_free(results);

            return queries_list;

        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "MDNS module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_mdns_query_obj, 3, mod_mdns_query);

STATIC mp_obj_t mod_mdns_query_instance_name(mp_obj_t self) {

    return ((mod_mdns_query_obj_t *)self)->instance_name;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_mdns_query_instance_name_obj, mod_mdns_query_instance_name);

STATIC mp_obj_t mod_mdns_query_hostname(mp_obj_t self) {

    return ((mod_mdns_query_obj_t *)self)->hostname;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_mdns_query_hostname_obj, mod_mdns_query_hostname);

STATIC mp_obj_t mod_mdns_query_port(mp_obj_t self) {

    return ((mod_mdns_query_obj_t *)self)->port;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_mdns_query_port_obj, mod_mdns_query_port);

STATIC mp_obj_t mod_mdns_query_txt(mp_obj_t self) {

    return ((mod_mdns_query_obj_t *)self)->txt;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_mdns_query_txt_obj, mod_mdns_query_txt);

STATIC mp_obj_t mod_mdns_query_addr(mp_obj_t self) {

    return ((mod_mdns_query_obj_t *)self)->addr;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_mdns_query_addr_obj, mod_mdns_query_addr);

STATIC const mp_map_elem_t mod_mdns_query_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance_name),           (mp_obj_t)&mod_mdns_query_instance_name_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hostname),               (mp_obj_t)&mod_mdns_query_hostname_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_port),                    (mp_obj_t)&mod_mdns_query_port_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_txt),                     (mp_obj_t)&mod_mdns_query_txt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_addr),                    (mp_obj_t)&mod_mdns_query_addr_obj },
};
STATIC MP_DEFINE_CONST_DICT(mod_mdns_query_locals_dict, mod_mdns_query_locals_dict_table);

static const mp_obj_type_t mod_mdns_query_type = {
    { &mp_type_type },
    .name = MP_QSTR_MDNS_Query,
    .locals_dict = (mp_obj_t)&mod_mdns_query_locals_dict,
};


STATIC const mp_map_elem_t mod_mdns_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_mdns) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_mdns_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                          (mp_obj_t)&mod_mdns_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_name),                        (mp_obj_t)&mod_mdns_set_name_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_service),                     (mp_obj_t)&mod_mdns_add_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remove_service),                  (mp_obj_t)&mod_mdns_remove_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_query),                           (mp_obj_t)&mod_mdns_query_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROTO_TCP),                     MP_OBJ_NEW_SMALL_INT(MOD_MDNS_PROTO_TCP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROTO_UDP),                     MP_OBJ_NEW_SMALL_INT(MOD_MDNS_PROTO_UDP) },
};

STATIC MP_DEFINE_CONST_DICT(mod_mdns_globals, mod_mdns_globals_table);

const mp_obj_module_t mod_mdns = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_mdns_globals,
};
