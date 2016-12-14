/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
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
} mod_network_nic_type_t;

typedef struct _mod_network_socket_base_t {
    mp_obj_t nic;
    mod_network_nic_type_t *nic_type;
    union {
        struct {
            // this order is important so that fileno gets > 0 once
            // the socket descriptor is assigned after being created.
            uint8_t domain;
            int8_t fileno;
            uint8_t type;
            uint8_t proto;
        } u_param;
        int32_t sd;
    };
    int32_t timeout;
    bool is_ssl;
    bool connected;
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

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
void mod_network_init0(void);
void mod_network_register_nic(mp_obj_t nic);
mp_obj_t mod_network_find_nic(const uint8_t *ip);

#endif  // MODNETWORK_H_
