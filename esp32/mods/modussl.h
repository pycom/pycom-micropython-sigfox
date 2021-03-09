/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODUSSL_H_
#define MODUSSL_H_

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct _mp_obj_ssl_socket_t {
    mp_obj_base_t base;
    mod_network_socket_base_t sock_base;
    mp_obj_t o_sock;
    vstr_t vstr_ca;
    vstr_t vstr_cert;
    vstr_t vstr_key;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context context_fd;
    mbedtls_x509_crt own_cert;
    mbedtls_pk_context pk_key;
    uint8_t read_timeout;
} mp_obj_ssl_socket_t;

typedef struct _mp_obj_ssl_session_t {
    mp_obj_base_t base;
    mbedtls_ssl_session saved_session;
} mp_obj_ssl_session_t;

#endif /* MODUSSL_H_ */
