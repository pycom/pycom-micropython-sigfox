/* 
* Copyright (c) 2017, Pycom Limited and its licensors.
*
* This software is licensed under the GNU GPL version 3 or any later version,
* with permitted additional terms. For more information see the Pycom Licence
* v1.0 document supplied with this file, or available at:
* https://www.pycom.io/opensource/licensing
*
* This file contains code under the following copyright and licensing notices.
* The code has been changed but otherwise retained.
*/


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/misc.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "ff.h"

#include "machpin.h"
#include "pins.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "pybioctl.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "modussl.h"

#include "modlte.h"
#include "3gpp/lib3GPP.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define MAKE_SOCKADDR(addr, ip, port)       struct sockaddr addr; \
                                            addr.sa_family = AF_INET; \
                                            addr.sa_data[0] = port >> 8; \
                                            addr.sa_data[1] = port; \
                                            addr.sa_data[2] = ip[3]; \
                                            addr.sa_data[3] = ip[2]; \
                                            addr.sa_data[4] = ip[1]; \
                                            addr.sa_data[5] = ip[0];

#define UNPACK_SOCKADDR(addr, ip, port)     port = (addr.sa_data[0] << 8) | addr.sa_data[1]; \
                                            ip[0] = addr.sa_data[5]; \
                                            ip[1] = addr.sa_data[4]; \
                                            ip[2] = addr.sa_data[3]; \
                                            ip[3] = addr.sa_data[2];

#define FILE_READ_SIZE 512

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
lte_obj_t lte_obj;


/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

#define UART_GPIO_TX CONFIG_GSM_TX
#define UART_GPIO_RX CONFIG_GSM_RX
#define UART_PIN_CTS CONFIG_GSM_CTS
#define UART_PIN_RTS CONFIG_GSM_RTS

void modlte_init0(void) {
    if (gpio_set_direction(UART_GPIO_TX, GPIO_MODE_OUTPUT)) return;
	if (gpio_set_direction(UART_GPIO_RX, GPIO_MODE_INPUT)) return;
	if (gpio_set_direction(UART_PIN_CTS, GPIO_MODE_INPUT)) return;
	if (gpio_set_direction(UART_PIN_RTS, GPIO_MODE_OUTPUT)) return;
	if (gpio_set_pull_mode(UART_GPIO_RX, GPIO_PULLUP_ONLY)) return;
	
	
	ppposInit();
}


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void lte_do_connect();

static int lte_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family);
static int lte_socket_socket(mod_network_socket_obj_t *s, int *_errno);
static void lte_socket_close(mod_network_socket_obj_t *s);
static int lte_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int lte_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno);
static int lte_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno);
static int lte_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int lte_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
static int lte_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
static int lte_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);
static int lte_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);
static int lte_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
static int lte_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
static int lte_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

static void lte_do_connect() {

    // first close any active connections
    lte_obj.disconnected = true;

	int res = ppposConnect();
	
    if (!res) lte_obj.disconnected = false;

    return;
}

/******************************************************************************/
// Micro Python bindings; LTE class

static mp_obj_t lte_init_helper(lte_obj_t *self, const mp_arg_val_t *args) {
    return mp_const_none;
}

static const mp_arg_t lte_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_cid,          MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1} }
};

static mp_obj_t lte_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), lte_init_args, args);

    // setup the object
    lte_obj_t *self = &lte_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_lte;

    if (n_kw > 0) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
        // start the peripheral
        lte_init_helper(self, &args[1]);
    }
    return (mp_obj_t)self;
}

STATIC mp_obj_t lte_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &lte_init_args[1], args);
    return lte_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_init_obj, 1, lte_init);

mp_obj_t lte_deinit(mp_obj_t self_in) {

    if (!lte_obj.disconnected) {
        ppposDisconnect(0,1);
        lte_obj.started = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_deinit_obj, lte_deinit);


STATIC mp_obj_t lte_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_cid,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    //TODO: Pick out cid and use it for the connection

    // connect to the network
    lte_do_connect ();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_connect_obj, 1, lte_connect);

STATIC mp_obj_t lte_disconnect(mp_obj_t self_in) {
    ppposDisconnect(1, 1);
    lte_obj.disconnected = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_disconnect_obj, lte_disconnect);


STATIC mp_obj_t lte_isconnected(mp_obj_t self_in) {
    if (ppposStatus() == GSM_STATE_CONNECTED)
        return mp_const_true;
    else 
        return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_isconnected_obj, lte_isconnected);


STATIC const mp_map_elem_t lte_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&lte_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&lte_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&lte_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&lte_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&lte_isconnected_obj },

    // class constants
};
STATIC MP_DEFINE_CONST_DICT(lte_locals_dict, lte_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_lte = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_LTE,
        .make_new = lte_make_new,
        .locals_dict = (mp_obj_t)&lte_locals_dict,
     },

    .n_gethostbyname = lte_gethostbyname,
    .n_listen = lte_socket_listen,
    .n_accept = lte_socket_accept,
    .n_socket = lte_socket_socket,
    .n_close = lte_socket_close,
    .n_connect = lte_socket_connect,
    .n_sendto =  lte_socket_sendto,
    .n_send = lte_socket_send,
    .n_recv = lte_socket_recv,
    .n_recvfrom = lte_socket_recvfrom,
    .n_settimeout = lte_socket_settimeout,
    .n_setsockopt = lte_socket_setsockopt,
    .n_bind = lte_socket_bind,
    .n_ioctl = lte_socket_ioctl,
};

///******************************************************************************/
//// Micro Python bindings; GSM socket

static int lte_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family) {
    uint32_t ip;
    struct hostent *h = gethostbyname(name);
    if (h == NULL) {
        // CPython: socket.herror
        return -errno;
    }
    ip = *(uint32_t*)*h->h_addr_list;
    out_ip[0] = ip;
    out_ip[1] = ip >> 8;
    out_ip[2] = ip >> 16;
    out_ip[3] = ip >> 24;
    return 0;
}

static int lte_socket_socket(mod_network_socket_obj_t *s, int *_errno) {
    int32_t sd = socket(s->sock_base.u.u_param.domain, s->sock_base.u.u_param.type, s->sock_base.u.u_param.proto);
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }

    // enable address reusing
    uint32_t option = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    s->sock_base.u.sd = sd;
    return 0;
}

static void lte_socket_close(mod_network_socket_obj_t *s) {
    // this is to prevent the finalizer to close a socket that failed when being created
    if (s->sock_base.u.sd >= 0) {
        if (s->sock_base.is_ssl) {
            mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
            if (ss->sock_base.connected) {
                while(mbedtls_ssl_close_notify(&ss->ssl) == MBEDTLS_ERR_SSL_WANT_WRITE);
            }
            mbedtls_net_free(&ss->context_fd);
            mbedtls_x509_crt_free(&ss->cacert);
            mbedtls_x509_crt_free(&ss->own_cert);
            mbedtls_pk_free(&ss->pk_key);
            mbedtls_ssl_free(&ss->ssl);
            mbedtls_ssl_config_free(&ss->conf);
            mbedtls_ctr_drbg_free(&ss->ctr_drbg);
            mbedtls_entropy_free(&ss->entropy);
        } else {
            close(s->sock_base.u.sd);
        }
        modusocket_socket_delete(s->sock_base.u.sd);
        s->sock_base.connected = false;
        s->sock_base.u.sd = -1;
    }
}

static int lte_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = bind(s->sock_base.u.sd, &addr, sizeof(addr));
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int lte_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno) {
    int ret = listen(s->sock_base.u.sd, backlog);
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int lte_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno) {
    // accept incoming connection
    int32_t sd;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    sd = accept(s->sock_base.u.sd, &addr, &addr_len);
    // save the socket descriptor
    s2->sock_base.u.sd = sd;
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }

    s2->sock_base.connected = true;

    // return ip and port
    UNPACK_SOCKADDR(addr, ip, *port);
    return 0;
}

static int lte_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = connect(s->sock_base.u.sd, &addr, sizeof(addr));

    if (ret != 0) {
        // printf("Connect returned -0x%x\n", -ret);
        *_errno = ret;
        return -1;
    }

    // printf("Connected.\n");

    if (s->sock_base.is_ssl && (ret == 0)) {
        mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;

        if ((ret = mbedtls_net_set_block(&ss->context_fd)) != 0) {
            // printf("failed! net_set_(non)block() returned -0x%x\n", -ret);
            *_errno = ret;
            return -1;
        }

        mbedtls_ssl_set_bio(&ss->ssl, &ss->context_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

        while ((ret = mbedtls_ssl_handshake(&ss->ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_TIMEOUT)
            {
                // printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
                *_errno = ret;
                return -1;
            }
        }

        // printf("Verifying peer X.509 certificate...\n");

        if ((ret = mbedtls_ssl_get_verify_result(&ss->ssl)) != 0) {
            /* In real life, we probably want to close connection if ret != 0 */
            // printf("Failed to verify peer certificate!\n");
            *_errno = ret;
            return -1;
        } else {
            // printf("Certificate verified.\n");
        }
    }

    s->sock_base.connected = true;
    return 0;
}

static int lte_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    mp_int_t bytes = 0;
    if (len > 0) {
        if (s->sock_base.is_ssl) {
            mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
            while ((bytes = mbedtls_ssl_write(&ss->ssl, (const unsigned char *)buf, len)) <= 0) {
                if (bytes != MBEDTLS_ERR_SSL_WANT_READ && bytes != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    // printf("mbedtls_ssl_write returned -0x%x\n", -bytes);
                    break;
                } else {
                    *_errno = MP_EAGAIN;
                    return -1;
                }
            }
        } else {
            bytes = send(s->sock_base.u.sd, (const void *)buf, len, 0);
        }
    }
    if (bytes <= 0) {
        *_errno = errno;
        return -1;
    }
    return bytes;
}


static int lte_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    int ret;
    if (s->sock_base.is_ssl) {
        mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
        do {
            ret = mbedtls_ssl_read(&ss->ssl, (unsigned char *)buf, len);
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ) {
                // do nothing
            } else if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
                // printf("SSL timeout recieved\n");
                // non-blocking return
                if (s->sock_base.timeout == 0) {
                    ret = 0;
                    break;
                }
                // blocking do nothing
            }
            else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                // printf("Close notify received\n");
                ret = 0;
                break;
            } else if (ret < 0) {
                // printf("mbedtls_ssl_read returned -0x%x\n", -ret);
                break;
            } else if (ret == 0) {
                // printf("Connection closed\n");
                break;
            } else {
                // printf("Data read OK = %d\n", ret);
                break;
            }
        } while (true);
        if (ret < 0) {
            *_errno = ret;
            return -1;
        }
    } else {
        ret = recv(s->sock_base.u.sd, buf, MIN(len, LTE_MAX_RX_SIZE), 0);
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
    }
    return ret;
}

static int lte_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    if (len > 0) {
        MAKE_SOCKADDR(addr, ip, port)
        int ret = sendto(s->sock_base.u.sd, (byte*)buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
        return ret;
    }
    return 0;
}

static int lte_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    mp_int_t ret = recvfrom(s->sock_base.u.sd, buf, MIN(len, LTE_MAX_RX_SIZE), 0, &addr, &addr_len);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    UNPACK_SOCKADDR(addr, ip, *port);
    return ret;
}

static int lte_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    int ret = setsockopt(s->sock_base.u.sd, level, opt, optval, optlen);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int lte_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    int ret;
    if (timeout_ms <= 0) {
        uint32_t option = fcntl(s->sock_base.u.sd, F_GETFL, 0);
        if (timeout_ms == 0) {
            // set non-blocking mode
            option |= O_NONBLOCK;
        } else {
            // set blocking mode
            option &= ~O_NONBLOCK;
        }
        ret = fcntl(s->sock_base.u.sd, F_SETFL, option);
    } else {
        // set timeout
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;              // seconds
        tv.tv_usec = (timeout_ms % 1000) * 1000;    // microseconds
        ret = setsockopt(s->sock_base.u.sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    if (ret != 0) {
        *_errno = errno;
        return -1;
    }

    s->sock_base.timeout = timeout_ms;
    return 0;
}

static int lte_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret;
    if (request == MP_IOCTL_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        int32_t sd = s->sock_base.u.sd;

        // init fds
        fd_set rfds, wfds, xfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&xfds);

        // set fds if needed
        if (flags & MP_IOCTL_POLL_RD) {
            FD_SET(sd, &rfds);
        }
        if (flags & MP_IOCTL_POLL_WR) {
            FD_SET(sd, &wfds);
        }
        if (flags & MP_IOCTL_POLL_HUP) {
            FD_SET(sd, &xfds);
        }

        // call select with the smallest possible timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10;
        int32_t nfds = select(sd + 1, &rfds, &wfds, &xfds, &tv);

        // check for errors
        if (nfds < 0) {
            *_errno = errno;
            return MP_STREAM_ERROR;
        }

        // check return of select
        if (FD_ISSET(sd, &rfds)) {
            ret |= MP_IOCTL_POLL_RD;
        }
        if (FD_ISSET(sd, &wfds)) {
            ret |= MP_IOCTL_POLL_WR;
        }
        if (FD_ISSET(sd, &xfds)) {
            ret |= MP_IOCTL_POLL_HUP;
        }
    } else {
        *_errno = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}
