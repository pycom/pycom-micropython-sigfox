/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Daniel Campora
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MODNETWORK_H_
#define MODNETWORK_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MOD_NETWORK_IPV4ADDR_BUF_SIZE             (4)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
// forward declarations
struct _mod_network_socket_obj_t;

typedef enum {
    SOCKET_CONN_START = 0,
    SOCKET_CONNECTED,
    SOCKET_NOT_CONNECTED,
    SOCKET_CONN_PENDING,
    SOCKET_CONN_ERROR,
    SOCKET_CONN_TIMEDOUT
}mod_network_sock_conn_status_t;

typedef struct _mod_network_nic_type_t {
    mp_obj_type_t base;

    // API for non-socket operations
    int (*n_gethostbyname)(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family);

    // API for socket operations; return -1 on error
    int (*n_socket)(struct _mod_network_socket_obj_t *socket, int *_errno);
    void (*n_close)(struct _mod_network_socket_obj_t *socket);
    int (*n_bind)(struct _mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno);
    int (*n_listen)(struct _mod_network_socket_obj_t *socket, mp_int_t backlog, int *_errno);
    int (*n_accept)(struct _mod_network_socket_obj_t *socket, struct _mod_network_socket_obj_t *socket2, byte *ip, mp_uint_t *port, int *_errno);
    int (*n_connect)(struct _mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno);
    int (*n_send)(struct _mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno);
    int (*n_recv)(struct _mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno);
    int (*n_sendto)(struct _mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);
    int (*n_recvfrom)(struct _mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);
    int (*n_setsockopt)(struct _mod_network_socket_obj_t *socket, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
    int (*n_settimeout)(struct _mod_network_socket_obj_t *socket, mp_int_t timeout_ms, int *_errno);
    int (*n_ioctl)(struct _mod_network_socket_obj_t *socket, mp_uint_t request, mp_uint_t arg, int *_errno);
    int (*n_setupssl)(struct _mod_network_socket_obj_t *socket, int *_errno);

    // Interface status
    bool (*inf_up)(void);
    // Bring Inf_up
    void (*set_default_inf)(void);
} mod_network_nic_type_t;

typedef struct _mod_network_socket_base_t {
    mp_obj_t nic;
    mod_network_nic_type_t *nic_type;
    union {
        struct {
            uint8_t domain;
            int8_t fileno;
            uint8_t type;
            uint8_t proto;
        } u_param;
        int32_t sd;
    }u;
    int32_t timeout;
    bool is_ssl;
    bool connected;
    uint8_t ip_addr[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
    mp_uint_t port;
    mod_network_sock_conn_status_t conn_status;
    int err;
    uint8_t domain;
} mod_network_socket_base_t;

typedef struct _mod_network_socket_obj_t {
    mp_obj_base_t base;
    mod_network_socket_base_t sock_base;
} mod_network_socket_obj_t;

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
extern const mod_network_nic_type_t mod_network_nic_type_wlan;
extern const mod_network_nic_type_t mod_network_nic_type_lora;
extern const mod_network_nic_type_t mod_network_nic_type_bt;
extern const mod_network_nic_type_t mod_network_nic_type_sigfox;
extern const mod_network_nic_type_t mod_network_nic_type_lte;

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
void mod_network_init0(void);
void mod_network_register_nic(mp_obj_t nic);
void mod_network_deregister_nic(mp_obj_t nic);
mp_obj_t mod_network_find_nic(const mod_network_socket_obj_t *s, const uint8_t *ip);

#endif  // MODNETWORK_H_
