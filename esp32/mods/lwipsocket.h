/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LWIPSOCKET_H_
#define LWIPSOCKET_H_

#include "modnetwork.h"

extern int lwipsocket_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family);

extern int lwipsocket_socket_socket(mod_network_socket_obj_t *s, int *_errno);

extern void lwipsocket_socket_close(mod_network_socket_obj_t *s);

extern int lwipsocket_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);

extern int lwipsocket_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno);

extern int lwipsocket_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno);

extern int lwipsocket_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);

extern int lwipsocket_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);

extern int lwipsocket_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);

extern int lwipsocket_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);

extern int lwipsocket_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);

extern int lwipsocket_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);

extern int lwipsocket_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);

extern int lwipsocket_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);

extern int lwipsocket_socket_setup_ssl(mod_network_socket_obj_t *s, int *_errno);

#endif      // LWIPSOCKET_H_
