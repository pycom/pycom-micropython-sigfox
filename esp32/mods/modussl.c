/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "mpexception.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modussl.h"
#include "mptask.h"
#include "pycom_general_util.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define DEFAULT_SSL_READ_TIMEOUT                    10 //sec

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
// tiny object for storing ssl sessions
STATIC mp_obj_t ssl_session_free(mp_obj_t self_in) {
    mp_obj_ssl_session_t *self = self_in;
    mbedtls_ssl_session_free(&self->saved_session);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ssl_session_free_obj, ssl_session_free);

STATIC const mp_map_elem_t ssl_session_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),             (mp_obj_t)&ssl_session_free_obj },
};
STATIC MP_DEFINE_CONST_DICT(ssl_session_locals_dict, ssl_session_locals_dict_table);

static const mp_obj_type_t mod_ssl_session_type = {
    { &mp_type_type },
    .name = MP_QSTR_SSLSession,
    .locals_dict = (mp_obj_t)&ssl_session_locals_dict,
};

// ssl sockets inherit from normal socket, so we take its
// locals and stream methods
STATIC const mp_obj_type_t ssl_socket_type = {
    { &mp_type_type },
    .name = MP_QSTR_ussl,
    .getiter = NULL,
    .iternext = NULL,
    .protocol = &socket_stream_p,
    .locals_dict = (mp_obj_t)&socket_locals_dict,
};

static int32_t mod_ssl_setup_socket (mp_obj_ssl_socket_t *ssl_sock, const mbedtls_ssl_session *saved_session, const char *host_name,
                                     const char *ca_cert, const char *client_cert, const char *client_key,
                                     uint32_t ssl_verify, uint32_t client_or_server) {

    int32_t ret;
    mbedtls_ssl_init(&ssl_sock->ssl);
    mbedtls_ssl_config_init(&ssl_sock->conf);
    mbedtls_ctr_drbg_init(&ssl_sock->ctr_drbg);
    mbedtls_x509_crt_init(&ssl_sock->cacert);
    mbedtls_x509_crt_init(&ssl_sock->own_cert);
    mbedtls_pk_init(&ssl_sock->pk_key);
    // printf("Seeding the random number generator\n");

    mbedtls_entropy_init(&ssl_sock->entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ssl_sock->ctr_drbg, mbedtls_entropy_func, &ssl_sock->entropy, (const unsigned char *)"Pycom", strlen("Pycom"))) != 0) {
        return ret;
    }

    if (ca_cert) {
        // printf("Loading the CA root certificate...\n");
        ret = mbedtls_x509_crt_parse(&ssl_sock->cacert, (uint8_t *)ca_cert, strlen(ca_cert) + 1);
        if (ret < 0) {
            // printf("mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
            return ret;
        }
    }

    if (client_cert) {
        // printf("Loading the own certificate...\n");
        ret = mbedtls_x509_crt_parse(&ssl_sock->own_cert, (uint8_t *)client_cert, strlen(client_cert) + 1);
        if (ret < 0) {
            // printf("mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
            return ret;
        }
    }

    if (client_key) {
        ret = mbedtls_pk_parse_key(&ssl_sock->pk_key, (uint8_t *)client_key, strlen(client_key) + 1, (const unsigned char *)"", 0);
        if (ret < 0) {
            // printf("mbedtls_pk_parse_key returned -0x%x\n\n", -ret);
            return ret;
        }
    }

    if ((ret = mbedtls_ssl_config_defaults(&ssl_sock->conf,
                                          client_or_server,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        // printf("mbedtls_ssl_config_defaults returned %d\n", ret);
        return ret;
    }

    mbedtls_ssl_conf_authmode(&ssl_sock->conf, ssl_verify);
    mbedtls_ssl_conf_rng(&ssl_sock->conf, mbedtls_ctr_drbg_random, &ssl_sock->ctr_drbg);
    mbedtls_ssl_conf_ca_chain(&ssl_sock->conf, &ssl_sock->cacert, NULL);
    if (client_cert && client_key) {
        if ((ret = mbedtls_ssl_conf_own_cert(&ssl_sock->conf,
                                             &ssl_sock->own_cert,
                                             &ssl_sock->pk_key)) != 0) {
            // printf("mbedtls_ssl_conf_own_cert returned %d\n", ret);
            return ret;
        }
    }

    if ((ret = mbedtls_ssl_setup(&ssl_sock->ssl, &ssl_sock->conf)) != 0) {
        // printf("mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        return ret;
    }
    if (saved_session != NULL) {
        if ((ret = mbedtls_ssl_set_session(&ssl_sock->ssl, saved_session)) != 0) {
            // printf("mbedtls_ssl_set_session returned -0x%x\n\n", -ret);
            return ret;
        }
    }

    if (host_name) {
        // printf("Setting hostname for TLS session...\n");
        /* Hostname set here should match CN in server certificate */
        if ((ret = mbedtls_ssl_set_hostname(&ssl_sock->ssl, host_name)) != 0) {
            // printf("mbedtls_ssl_set_hostname returned -0x%x\n", -ret);
            return ret;
        }
    }

    ssl_sock->context_fd.fd = ssl_sock->sock_base.u.sd;
    ssl_sock->sock_base.is_ssl = true;

    // perform the handshake if already connected
    if (ssl_sock->sock_base.connected) {

        if ((ret = mbedtls_net_set_block(&ssl_sock->context_fd)) != 0) {
            // printf("failed! net_set_(non)block() returned -0x%x\n", -ret);
            return ret;
        }

        mbedtls_ssl_set_bio(&ssl_sock->ssl, &ssl_sock->context_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

        //printf("Performing the SSL/TLS handshake...\n");
        int count = 0;
        while ((ret = mbedtls_ssl_handshake(&ssl_sock->ssl)) != 0)
        {
            if ((ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_TIMEOUT) || count >= ssl_sock->read_timeout) {
                 //printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
                return ret;
            }
            if(ret == MBEDTLS_ERR_SSL_TIMEOUT)
            {
                count++;
            }
        }

        //printf("Verifying peer X.509 certificate...\n");
        ret = mbedtls_ssl_get_verify_result(&ssl_sock->ssl);
        if (ret == 0) {
            //printf("Certificate verified.\n");
            return 0;
        }
        // If no verification is needed the mbedtls_ssl_get_verify_result() returns with MBEDTLS_X509_BADCERT_SKIP_VERIFY
        else if((ssl_verify == MBEDTLS_SSL_VERIFY_NONE) && (ret == MBEDTLS_X509_BADCERT_SKIP_VERIFY)) {
            //printf("Certification validation skipped.\n");
            return 0;
        }
        else {
            /* In real life, we probably want to close connection in this case */
            //printf("Failed to verify peer certificate!\n");
            return -1;
        }
    }

    return 0;
}


/******************************************************************************/
// Micro Python bindings; SSL class

STATIC mp_obj_t mod_ssl_wrap_socket(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_sock,                         MP_ARG_REQUIRED | MP_ARG_OBJ,  },
        { MP_QSTR_keyfile,                      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_certfile,                     MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_server_side,                  MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_cert_reqs,                    MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = MBEDTLS_SSL_VERIFY_NONE} },
        { MP_QSTR_ssl_version,                  MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
        { MP_QSTR_ca_certs,                     MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_server_hostname,              MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_saved_session,                MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,                      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    };

    int32_t _error;

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    bool server_side = args[3].u_bool;
    uint32_t verify_type = args[4].u_int;
    // chech if ca validation is required
    if (verify_type != MBEDTLS_SSL_VERIFY_NONE && args[6].u_obj == mp_const_none) {
        goto arg_error;
    }

    // retrieve the file paths (with an 6 byte offset in order to strip it from the '/flash' prefix)
    const char *keyfile_path  = (args[1].u_obj == mp_const_none) ? NULL : mp_obj_str_get_str(args[1].u_obj);
    const char *certfile_path = (args[2].u_obj == mp_const_none) ? NULL : mp_obj_str_get_str(args[2].u_obj);
    const char *cafile_path   = (args[6].u_obj == mp_const_none || verify_type == MBEDTLS_SSL_VERIFY_NONE) ?
                                NULL : mp_obj_str_get_str(args[6].u_obj);
    const char *host_name = (args[7].u_obj == mp_const_none) ? NULL : mp_obj_str_get_str(args[7].u_obj);

    // server side requires both certfile and keyfile
    if (args[3].u_bool && (!keyfile_path || !certfile_path)) {
        goto arg_error;
    }

    // socket type check
    if (!mp_obj_is_type(args[0].u_obj, &socket_type)) {
    	goto arg_error;
    }

    // saved_session type check
    if (args[8].u_obj != mp_const_none) {
    	if (!mp_obj_is_type(args[8].u_obj, &mod_ssl_session_type)) {
    		goto arg_error;
        }
    }

    // Retrieve previously saved session
    const mbedtls_ssl_session *saved_session  = (args[8].u_obj == mp_const_none) ? NULL : &((mp_obj_ssl_session_t *)args[8].u_obj)->saved_session;

    // create the ssl socket
    mp_obj_ssl_socket_t *ssl_sock = m_new_obj_with_finaliser(mp_obj_ssl_socket_t);
    // ssl sockets inherit all properties from the original socket
    memcpy (&ssl_sock->sock_base, &((mod_network_socket_obj_t *)args[0].u_obj)->sock_base, sizeof(mod_network_socket_base_t));
    ssl_sock->base.type = &ssl_socket_type;
    ssl_sock->o_sock = args[0].u_obj;       // this is needed so that the GC doesnt collect the socket

    //Read timeout
    if(args[9].u_obj == mp_const_none)
    {
        ssl_sock->read_timeout = DEFAULT_SSL_READ_TIMEOUT;
    }
    else
    {
        ssl_sock->read_timeout = mp_obj_get_int(args[9].u_obj);
    }

    const char *ca_cert = NULL;
    if (cafile_path) {
        ca_cert = pycom_util_read_file(cafile_path, &ssl_sock->vstr_ca);
        if(ca_cert == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "CA file not found"));
        }
    }

    const char *client_cert = NULL;
    const char *client_key = NULL;
    if (certfile_path && keyfile_path) {
        client_cert = pycom_util_read_file(certfile_path, &ssl_sock->vstr_ca);
        if(client_cert == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "certificate file not found"));
        }
        client_key = pycom_util_read_file(keyfile_path, &ssl_sock->vstr_ca);
        if(client_key == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "key file not found"));
        }
    }


    MP_THREAD_GIL_EXIT();

    _error = mod_ssl_setup_socket(ssl_sock, saved_session, host_name, ca_cert, client_cert, client_key,
                                  verify_type, server_side ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT);

    MP_THREAD_GIL_ENTER();

    if (_error) {
        mp_raise_OSError(_error);
    }

    return ssl_sock;

arg_error:
    mp_raise_ValueError(mpexception_value_invalid_arguments);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ssl_wrap_socket_obj, 0, mod_ssl_wrap_socket);

STATIC mp_obj_t mod_ssl_save_session(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssl_sock,     MP_ARG_REQUIRED | MP_ARG_OBJ, },
    };

    int32_t _error;

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_obj_ssl_socket_t *ssl_sock = args[0].u_obj;

    // Create the SSL session obj
    mp_obj_ssl_session_t *ssl_session = m_new_obj_with_finaliser(mp_obj_ssl_session_t);
    ssl_session->base.type = &mod_ssl_session_type;
    memset(&ssl_session->saved_session, 0, sizeof(mbedtls_ssl_session));

    MP_THREAD_GIL_EXIT();

    _error = mbedtls_ssl_get_session(&ssl_sock->ssl, &ssl_session->saved_session);

    MP_THREAD_GIL_ENTER();

    if (_error) {
        mp_raise_OSError(_error);
    }

    return ssl_session;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_ssl_save_session_obj, 0, mod_ssl_save_session);

STATIC const mp_map_elem_t mp_module_ussl_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_ussl) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wrap_socket),         (mp_obj_t)&mod_ssl_wrap_socket_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_save_session),        (mp_obj_t)&mod_ssl_save_session_obj },

    // class exceptions
    { MP_OBJ_NEW_QSTR(MP_QSTR_SSLError),            (mp_obj_t)&mp_type_OSError },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_CERT_NONE),           MP_OBJ_NEW_SMALL_INT(MBEDTLS_SSL_VERIFY_NONE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CERT_OPTIONAL),       MP_OBJ_NEW_SMALL_INT(MBEDTLS_SSL_VERIFY_OPTIONAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CERT_REQUIRED),       MP_OBJ_NEW_SMALL_INT(MBEDTLS_SSL_VERIFY_REQUIRED) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_SSL_TIMEOUT),         MP_OBJ_NEW_SMALL_INT(MBEDTLS_ERR_SSL_TIMEOUT) },

    // { MP_OBJ_NEW_QSTR(MP_QSTR_PROTOCOL_SSLv3),      MP_OBJ_NEW_SMALL_INT(SL_SO_SEC_METHOD_SSLV3) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_PROTOCOL_TLSv1),      MP_OBJ_NEW_SMALL_INT(SL_SO_SEC_METHOD_TLSV1) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_PROTOCOL_TLSv1_1),    MP_OBJ_NEW_SMALL_INT(SL_SO_SEC_METHOD_TLSV1_1) },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_PROTOCOL_TLSv1_2),    MP_OBJ_NEW_SMALL_INT(SL_SO_SEC_METHOD_TLSV1_2) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_ussl_globals, mp_module_ussl_globals_table);

const mp_obj_module_t mp_module_ussl = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_ussl_globals,
};
