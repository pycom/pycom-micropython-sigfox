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


#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "mpexception.h"
#include "py/stream.h"
#include "esp32_mphal.h"

#include "modnetwork.h"
#include "modusocket.h"

#include "sigfox/modsigfox.h"


STATIC const mp_arg_t sigfox_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,                           MP_ARG_INT,   {.u_int  = E_SIGFOX_MODE_SIGFOX} },
    { MP_QSTR_rcz,                            MP_ARG_INT,   {.u_int  = E_SIGFOX_RCZ1} },
#if !defined(FIPY) && !defined(LOPY4)
    { MP_QSTR_frequency,     MP_ARG_KW_ONLY | MP_ARG_OBJ,   {.u_obj  = mp_const_none} },
#endif
};

STATIC mp_obj_t sigfox_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(sigfox_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), sigfox_init_args, args);

    // setup the object
    sigfox_obj_t *self = &sigfox_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_sigfox;

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // run the constructor if the peripehral is not initialized or extra parameters are given
    if (n_kw > 0 || self->state == E_SIGFOX_STATE_NOINIT) {
        // start the peripheral
        sigfox_init_helper(self, &args[1]);
        // register it as a network card
        mod_network_register_nic(self);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t sigfox_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(sigfox_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &sigfox_init_args[1], args);
    return sigfox_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(sigfox_init_obj, 1, sigfox_init);

STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_mac_obj, sigfox_mac);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_id_obj, sigfox_id);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_pac_obj, sigfox_pac);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sigfox_test_mode_obj, sigfox_test_mode);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sigfox_cw_obj, sigfox_cw);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_frequencies_obj, sigfox_frequencies);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_config_obj, 1, 2, sigfox_config);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_public_key_obj, 1, 2, sigfox_public_key);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_rssi_obj, sigfox_rssi);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_rssi_offset_obj, 1, 2, sigfox_rssi_offset);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_freq_offset_obj, 1, 2, sigfox_freq_offset);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_version_obj, sigfox_version);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_info_obj, sigfox_info);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_reset_obj, sigfox_reset);


STATIC const mp_map_elem_t sigfox_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&sigfox_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&sigfox_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_id),                  (mp_obj_t)&sigfox_id_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pac),                 (mp_obj_t)&sigfox_pac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_test_mode),           (mp_obj_t)&sigfox_test_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cw),                  (mp_obj_t)&sigfox_cw_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_frequencies),         (mp_obj_t)&sigfox_frequencies_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_config),              (mp_obj_t)&sigfox_config_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_public_key),          (mp_obj_t)&sigfox_public_key_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_version),             (mp_obj_t)&sigfox_version_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rssi),                (mp_obj_t)&sigfox_rssi_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rssi_offset),         (mp_obj_t)&sigfox_rssi_offset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_freq_offset),         (mp_obj_t)&sigfox_freq_offset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_info),                (mp_obj_t)&sigfox_info_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),               (mp_obj_t)&sigfox_reset_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_SIGFOX),              MP_OBJ_NEW_SMALL_INT(E_SIGFOX_MODE_SIGFOX) },
#if !defined(FIPY) && !defined(LOPY4)
    { MP_OBJ_NEW_QSTR(MP_QSTR_FSK),                 MP_OBJ_NEW_SMALL_INT(E_SIGFOX_MODE_FSK) },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ1),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ1) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ2),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ2) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ3),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ3) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ4),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ4) },
};

STATIC MP_DEFINE_CONST_DICT(sigfox_locals_dict, sigfox_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_sigfox = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_Sigfox,
        .make_new = sigfox_make_new,
        .locals_dict = (mp_obj_t)&sigfox_locals_dict,
     },

    .n_socket = sigfox_socket_socket,
    .n_close = sigfox_socket_close,
    .n_send = sigfox_socket_send,
    .n_recv = sigfox_socket_recv,
    .n_settimeout = sigfox_socket_settimeout,
    .n_setsockopt = sigfox_socket_setsockopt,
    .n_ioctl = sigfox_socket_ioctl,
};
