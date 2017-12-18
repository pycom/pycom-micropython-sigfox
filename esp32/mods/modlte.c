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
lte_obj_t lte_obj = {
//        .mode = -1,
//        .status = 0,
//        .ip = 0,
//        .auth = MICROPY_PORT_WLAN_AP_SECURITY,
//        .channel = MICROPY_PORT_WLAN_AP_CHANNEL,
//        .ssid = MICROPY_PORT_WLAN_AP_SSID,
//        .key = MICROPY_PORT_WLAN_AP_KEY,
//        .mac = {0},
//        //.ssid_o = {0},
//        //.bssid = {0},
//    #if (MICROPY_PORT_HAS_TELNET || MICROPY_PORT_HAS_FTP)
//        .servers_enabled = false,
//    #endif
};

static EventGroupHandle_t gsm_event_group;

// Event bits
#define CONNECTED_BIT BIT0

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
//OsiLockObj_t wlan_LockObj; TODO




/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void lte_reset (void);
//static esp_err_t wlan_event_handler(void *ctx, system_event_t *event);
STATIC void lte_do_connect ();

//STATIC void wlan_event_handler_cb (System_Event_t *event);

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

void lte_setup () {
// TODO
}


//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

STATIC esp_err_t gsm_event_handler(void *ctx, system_event_t *event) {
/*
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
    {
        system_event_sta_connected_t *_event = (system_event_sta_connected_t *)&event->event_info;
        memcpy(wlan_obj.bssid, _event->bssid, 6);
        wlan_obj.channel = _event->channel;
        wlan_obj.auth = _event->authmode;
    }
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        system_event_sta_disconnected_t *disconn = &event->event_info.disconnected;
        switch (disconn->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_AUTH_FAIL:
            case WIFI_REASON_GROUP_CIPHER_INVALID:
            case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            case WIFI_REASON_802_1X_AUTH_FAILED:
            case WIFI_REASON_CIPHER_SUITE_REJECTED:
                wlan_obj.disconnected = true;
                break;
            default:
                // Let other errors through and try to reconnect.
                break;
        }
        if (!wlan_obj.disconnected) {
            wifi_mode_t mode;
            if (esp_wifi_get_mode(&mode) == ESP_OK) {
                if (mode & WIFI_MODE_STA) {
                    esp_wifi_connect();
                }
            }
        }
        break;
    default:
        break;
    }
    */
    return ESP_OK;
    
}

STATIC void lte_do_connect () {

    // first close any active connections
    lte_obj.disconnected = true;

	int res = ppposInit();
	
    //TODO

    lte_obj.disconnected = false;

    return;

    // TODO Add timeout handling!!
}



/* 
static void uart_assign_pins_af (mach_uart_obj_t *self, mp_obj_t *pins, uint32_t n_pins) {
    if (n_pins > 1) {
        for (int i = 0; i < n_pins; i++) {
            if (pins[i] != mp_const_none) {
                pin_obj_t *pin = pin_find(pins[i]);
                int32_t af_in, af_out, mode, pull;
                if (i % 2) {
                    af_in = mach_uart_pin_af[self->uart_id][i];
                    af_out = -1;
                    mode = GPIO_MODE_INPUT;
                    pull = MACHPIN_PULL_UP;
                } else {
                    af_in = -1;
                    af_out = mach_uart_pin_af[self->uart_id][i];
                    mode = GPIO_MODE_OUTPUT;
                    pull = MACHPIN_PULL_UP;
                }
                pin_config(pin, af_in, af_out, mode, pull, 1);
                self->pins[i] = pin;
            }
        }
    } else {
        pin_obj_t *pin = pin_find(pins[0]);
        // make the pin Rx by default
        pin_config(pin, mach_uart_pin_af[self->uart_id][1], -1, GPIO_MODE_INPUT, MACHPIN_PULL_UP, 1);
        self->pins[0] = pin;
    }
    self->n_pins = n_pins;
}
 */


/******************************************************************************/
// Micro Python bindings; LTE class

STATIC mp_obj_t lte_init_helper(lte_obj_t *self, const mp_arg_val_t *args) {



    //mp_obj_t pins_o = args[1].u_obj;
    //uart_assign_pins(self, pins, n_pins);
    
    
    /*
    
    // get the mode
    int8_t mode = args[0].u_int;
    wlan_validate_mode(mode);

    // get the ssid
    const char *ssid = NULL;
    if (args[1].u_obj != NULL) {
        ssid = mp_obj_str_get_str(args[1].u_obj);
        wlan_validate_ssid_len(strlen(ssid));
    }

    // get the auth config
    uint8_t auth = WIFI_AUTH_OPEN;
    const char *key = NULL;
    if (args[2].u_obj != mp_const_none) {
        mp_obj_t *sec;
        mp_obj_get_array_fixed_n(args[2].u_obj, 2, &sec);
        auth = mp_obj_get_int(sec[0]);
        key = mp_obj_str_get_str(sec[1]);
        if (strlen(key) < 8 || strlen(key) > 32) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid key length"));
        }
        wlan_validate_security(auth, key);
    }

    // get the channel
    uint8_t channel = args[3].u_int;
    wlan_validate_channel(channel);

    // get the antenna type
    uint8_t antenna = args[4].u_int;
    wlan_validate_antenna(antenna);

    wlan_obj.pwrsave = args[5].u_bool;

    if (mode != WIFI_MODE_STA) {
        if (ssid == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "AP SSID not given"));
        }
        if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "WPA2_ENT not supported in AP mode"));
        }
    }

    // initialize the wlan subsystem
    wlan_setup(mode, (const char *)ssid, auth, (const char *)key, channel, antenna, false);
    mod_network_register_nic(&wlan_obj);

	*/
    return mp_const_none;
}

STATIC const mp_arg_t lte_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_baudrate,                        MP_ARG_INT,  {.u_int = 921600} },
    { MP_QSTR_pins,           MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    // { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_MODE_STA} },
    // { MP_QSTR_ssid,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    // { MP_QSTR_auth,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    // { MP_QSTR_channel,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    // { MP_QSTR_antenna,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = ANTENNA_TYPE_INTERNAL} },
    // { MP_QSTR_power_save,   MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
};

STATIC mp_obj_t lte_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), lte_init_args, args);

    // setup the object
    lte_obj_t *self = &lte_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_lte;


    // FIXME work out the uart id
    uint uart_id = mp_obj_get_int(args[0].u_obj);


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

    if (lte_obj.started) {
        //esp_wifi_stop();
        lte_obj.started = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_deinit_obj, lte_deinit);


STATIC mp_obj_t lte_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
    	/*
        { MP_QSTR_ssid,                 MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_auth,                                   MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bssid,                MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_ca_certs,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_keyfile,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_certfile,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_identity,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        */
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // connect to the requested access point
    lte_do_connect ();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_connect_obj, 1, lte_connect);

STATIC mp_obj_t lte_disconnect(mp_obj_t self_in) {
//TODO
    lte_obj.disconnected = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_disconnect_obj, lte_disconnect);

STATIC mp_obj_t lte_isconnected(mp_obj_t self_in) {
    //if (xEventGroupGetBits(gsm_event_group) & CONNECTED_BIT) {
    //    return mp_const_true;
    //}
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

    .n_socket = lte_socket_socket,
    .n_close = lte_socket_close,
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

        // printf("Performing the SSL/TLS handshake...\n");

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

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modlte_init0(void) {
    //xCmdQueue = xQueueCreate(LORA_CMD_QUEUE_SIZE_MAX, sizeof(lora_cmd_data_t));
    //xRxQueue = xQueueCreate(LORA_DATA_QUEUE_SIZE_MAX, sizeof(lora_rx_data_t));
    //GSMEvents = xEventGroupCreate();

    //if (!lorawan_nvs_open()) {
    //    printf("Error opening LoRa NVS namespace!\n");
    //}

    //xTaskCreatePinnedToCore(TASK_LTE, "LTE", LORA_STACK_SIZE / sizeof(StackType_t), NULL, LORA_TASK_PRIORITY, NULL, 0);
}
