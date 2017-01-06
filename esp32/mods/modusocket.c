/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modwlan.h"
#include "modusocket.h"
#include "mpexception.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define MODUSOCKET_MAX_SOCKETS                      15

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    int32_t sd;
    bool    user;
} modusocket_sock_t;

/******************************************************************************
 DEFINE PRIVATE DATA
 ******************************************************************************/
STATIC const mp_obj_type_t socket_type;
STATIC const mp_obj_type_t raw_socket_type;
//STATIC OsiLockObj_t modusocket_LockObj;
STATIC modusocket_sock_t modusocket_sockets[MODUSOCKET_MAX_SOCKETS] = {{.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1},
                                                                       {.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1},
                                                                       {.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1}, {.sd = -1}};

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modusocket_pre_init (void) {
    // create the wlan lock
//    ASSERT(OSI_OK == sl_LockObjCreate(&modusocket_LockObj, "SockLock"));
//    sl_LockObjUnlock (&modusocket_LockObj);
}

void modusocket_socket_add (int32_t sd, bool user) {
//    sl_LockObjLock (&modusocket_LockObj, SL_OS_WAIT_FOREVER);
    for (int i = 0; i < MODUSOCKET_MAX_SOCKETS; i++) {
        if (modusocket_sockets[i].sd < 0) {
            modusocket_sockets[i].sd = sd;
            modusocket_sockets[i].user = user;
            break;
        }
    }
//    sl_LockObjUnlock (&modusocket_LockObj);
}

void modusocket_socket_delete (int32_t sd) {
//    sl_LockObjLock (&modusocket_LockObj, SL_OS_WAIT_FOREVER);
    for (int i = 0; i < MODUSOCKET_MAX_SOCKETS; i++) {
        if (modusocket_sockets[i].sd == sd) {
            modusocket_sockets[i].sd = -1;
            break;
        }
    }
//    sl_LockObjUnlock (&modusocket_LockObj);
}

//void modusocket_enter_sleep (void) {
//    fd_set socketset;
//    int32_t maxfd = 0;
//
//    for (int i = 0; i < MOD_NETWORK_MAX_SOCKETS; i++) {
//        int32_t sd;
//        if ((sd = modusocket_sockets[i].sd) >= 0) {
//            FD_SET(sd, &socketset);
//            maxfd = (maxfd > sd) ? maxfd : sd;
//        }
//    }
//
//    if (maxfd > 0) {
//        // wait for any of the sockets to become ready...
//        sl_Select(maxfd + 1, &socketset, NULL, NULL, NULL);
//    }
//}

void modusocket_close_all_user_sockets (void) {
//    sl_LockObjLock (&modusocket_LockObj, SL_OS_WAIT_FOREVER);
    for (int i = 0; i < MODUSOCKET_MAX_SOCKETS; i++) {
        if (modusocket_sockets[i].sd >= 0 && modusocket_sockets[i].user) {
//            sl_Close(modusocket_sockets[i].sd); // FIXME
            modusocket_sockets[i].sd = -1;
        }
    }
//    sl_LockObjUnlock (&modusocket_LockObj);
}

///******************************************************************************/
// socket class

STATIC void socket_select_nic(mod_network_socket_obj_t *self, const byte *ip) {
    if (self->sock_base.nic == MP_OBJ_NULL) {
        // select a nic (based on some criteria TBD)
        self->sock_base.nic = mod_network_find_nic(ip);
        self->sock_base.nic_type = (mod_network_nic_type_t*)mp_obj_get_type(self->sock_base.nic);
    }
}

// constructor socket(family=AF_INET, type=SOCK_STREAM, proto=IPPROTO_TCP, fileno=None)
STATIC mp_obj_t socket_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 4, false);

    // create socket object
    mod_network_socket_obj_t *s = m_new_obj_with_finaliser(mod_network_socket_obj_t);
    if (n_args > 0 && mp_obj_get_int(args[0]) == AF_LORA) {
        s->base.type = (mp_obj_t)&raw_socket_type;
        s->sock_base.u_param.type = SOCK_RAW;
    } else {
        s->base.type = (mp_obj_t)&socket_type;
        s->sock_base.u_param.domain = AF_INET;
        s->sock_base.u_param.type = SOCK_STREAM;
        s->sock_base.u_param.proto = IPPROTO_TCP;
    }
    s->sock_base.nic = MP_OBJ_NULL;
    s->sock_base.nic_type = NULL;
    s->sock_base.u_param.fileno = -1;
    s->sock_base.timeout = -1;      // sockets are blocking by default
    s->sock_base.is_ssl = false;
    s->sock_base.connected = false;

    if (n_args > 0) {
        s->sock_base.u_param.domain = mp_obj_get_int(args[0]);
        if (n_args > 1) {
            s->sock_base.u_param.type = mp_obj_get_int(args[1]);
            if (n_args > 2) {
                s->sock_base.u_param.proto = mp_obj_get_int(args[2]);
                if (n_args > 3) {
                    s->sock_base.u_param.fileno = mp_obj_get_int(args[3]);
                }
            }
        }
    }

    // don't forget to select a network card
    if (s->sock_base.u_param.domain == AF_INET) {
        socket_select_nic(s, (const byte *)"");
    } else {
        if (s->sock_base.u_param.type != SOCK_RAW) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "incorrect socket type"));
        }
        socket_select_nic(s, NULL);
    }

    // now create the socket
    int _errno;
    if (s->sock_base.nic_type->n_socket(s, &_errno) != 0) {
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    // add the socket to the list
    modusocket_socket_add(s->sock_base.sd, true);

    return s;
}

// method socket.close()
STATIC mp_obj_t socket_close(mp_obj_t self_in) {
    mod_network_socket_obj_t *self = self_in;
    self->sock_base.nic_type->n_close(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_close_obj, socket_close);

// method socket.bind(address)
STATIC mp_obj_t socket_bind(mp_obj_t self_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = self_in;
    int _errno;

#ifdef LOPY
    if (self->sock_base.nic_type == &mod_network_nic_type_lora) {
        mp_uint_t port = mp_obj_get_int(addr_in);

        if (self->sock_base.nic_type->n_bind(self, NULL, port, &_errno) != 0) {
            nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
        }
    } else {
#endif
        // get the address
        uint8_t ip[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
        mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_LITTLE);

        if (self->sock_base.nic_type->n_bind(self, ip, port, &_errno) != 0) {
            nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
        }
#ifdef LOPY
    }
#endif
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

// method socket.listen([backlog])
STATIC mp_obj_t socket_listen(mp_uint_t n_args, const mp_obj_t *args) {
    mod_network_socket_obj_t *self = args[0];

    int32_t backlog = 0;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
        backlog = (backlog < 0) ? 0 : backlog;
    }

    int _errno;
    if (self->sock_base.nic_type->n_listen(self, backlog, &_errno) != 0) {
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

// method socket.accept()
STATIC mp_obj_t socket_accept(mp_obj_t self_in) {
    mod_network_socket_obj_t *self = self_in;

    // create new socket object
    mod_network_socket_obj_t *socket2 = m_new_obj_with_finaliser(mod_network_socket_obj_t);
    // the new socket inherits all properties from its parent
    memcpy (socket2, self, sizeof(mod_network_socket_obj_t));

    // accept the incoming connection
    uint8_t ip[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
    mp_uint_t port;
    int _errno;
    MP_THREAD_GIL_EXIT();
    if (self->sock_base.nic_type->n_accept(self, socket2, ip, &port, &_errno) != 0) {
        MP_THREAD_GIL_ENTER();
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }

    // add the socket to the list
    MP_THREAD_GIL_ENTER();
    modusocket_socket_add(socket2->sock_base.sd, true);

    // make the return value
    mp_obj_tuple_t *client = mp_obj_new_tuple(2, NULL);
    client->items[0] = socket2;
    client->items[1] = netutils_format_inet_addr(ip, port, NETUTILS_LITTLE);
    return client;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

// method socket.connect(address)
STATIC mp_obj_t socket_connect(mp_obj_t self_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = self_in;

    // get address
    uint8_t ip[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_LITTLE);

    // connect the socket
    int _errno;
    MP_THREAD_GIL_EXIT();
    if (self->sock_base.nic_type->n_connect(self, ip, port, &_errno) != 0) {
        MP_THREAD_GIL_ENTER();
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

// method socket.send(bytes)
STATIC mp_obj_t socket_send(mp_obj_t self_in, mp_obj_t buf_in) {
    mod_network_socket_obj_t *self = self_in;
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    int _errno;
    MP_THREAD_GIL_EXIT();
    mp_int_t ret = self->sock_base.nic_type->n_send(self, bufinfo.buf, bufinfo.len, &_errno);
    MP_THREAD_GIL_ENTER();
    if (ret < 0) {
        if (_errno == EAGAIN && self->sock_base.timeout > 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
        }
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    return mp_obj_new_int_from_uint(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_send_obj, socket_send);

// method socket.recv(bufsize)
STATIC mp_obj_t socket_recv(mp_obj_t self_in, mp_obj_t len_in) {
    mod_network_socket_obj_t *self = self_in;
    mp_int_t len = mp_obj_get_int(len_in);
    vstr_t vstr;
    vstr_init_len(&vstr, len);
    int _errno;
    MP_THREAD_GIL_EXIT();
    mp_int_t ret = self->sock_base.nic_type->n_recv(self, (byte*)vstr.buf, len, &_errno);
    MP_THREAD_GIL_ENTER();
    if (ret < 0) {
        if (_errno == EAGAIN) {
            if (self->sock_base.timeout > 0) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
            } else {
                ret = 0;        // non-blocking socket
            }
        } else {
            nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
        }
    }
    if (ret == 0) {
        return mp_const_empty_bytes;
    }
    vstr.len = ret;
    vstr.buf[vstr.len] = '\0';
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_recv_obj, socket_recv);

// method socket.sendto(bytes, address)
STATIC mp_obj_t socket_sendto(mp_obj_t self_in, mp_obj_t data_in, mp_obj_t addr_in) {
    mod_network_socket_obj_t *self = self_in;

    // get the data
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    // get address
    uint8_t ip[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
    mp_uint_t port = netutils_parse_inet_addr(addr_in, ip, NETUTILS_LITTLE);

    // call the nic to sendto
    int _errno;
    MP_THREAD_GIL_EXIT();
    mp_int_t ret = self->sock_base.nic_type->n_sendto(self, bufinfo.buf, bufinfo.len, ip, port, &_errno);
    MP_THREAD_GIL_ENTER();
    if (ret < 0) {
        if (_errno == EAGAIN && self->sock_base.timeout > 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
        }
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    return mp_obj_new_int(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(socket_sendto_obj, socket_sendto);

// method socket.recvfrom(bufsize)
STATIC mp_obj_t socket_recvfrom(mp_obj_t self_in, mp_obj_t len_in) {
    mod_network_socket_obj_t *self = self_in;
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(len_in));
    byte ip[4];
    mp_uint_t port;
    int _errno;
    MP_THREAD_GIL_EXIT();
    mp_int_t ret = self->sock_base.nic_type->n_recvfrom(self, (byte*)vstr.buf, vstr.len, ip, &port, &_errno);
    MP_THREAD_GIL_ENTER();
    if (ret < 0) {
        if (_errno == EAGAIN && self->sock_base.timeout > 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
        }
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    mp_obj_t tuple[2];
    if (ret == 0) {
        tuple[0] = mp_const_empty_bytes;
    } else {
        vstr.len = ret;
        vstr.buf[vstr.len] = '\0';
        tuple[0] = mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
    }
    tuple[1] = netutils_format_inet_addr(ip, port, NETUTILS_LITTLE);
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_recvfrom_obj, socket_recvfrom);

// method socket.setsockopt(level, optname, value)
STATIC mp_obj_t socket_setsockopt(mp_uint_t n_args, const mp_obj_t *args) {
    mod_network_socket_obj_t *self = args[0];

    mp_int_t level = mp_obj_get_int(args[1]);
    mp_int_t opt = mp_obj_get_int(args[2]);

    const void *optval;
    mp_uint_t optlen;
    mp_int_t val;
    if (mp_obj_is_integer(args[3])) {
        val = mp_obj_get_int_truncated(args[3]);
        optval = &val;
        optlen = sizeof(val);
    } else if (MP_OBJ_IS_TYPE(args[3], &mp_type_bool)) {
        val = mp_obj_is_true(args[3]);
        optval = &val;
        optlen = sizeof(val);
    } else {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        optval = bufinfo.buf;
        optlen = bufinfo.len;
    }

    int _errno;
    if (self->sock_base.nic_type->n_setsockopt(self, level, opt, optval, optlen, &_errno) != 0) {
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 4, socket_setsockopt);

// method socket.settimeout(value)
// timeout=0 means non-blocking
// timeout=None means blocking
// otherwise, timeout is in seconds
STATIC mp_obj_t socket_settimeout(mp_obj_t self_in, mp_obj_t timeout_in) {
    mod_network_socket_obj_t *self = self_in;
    int32_t timeout;
    if (timeout_in == mp_const_none) {
        timeout = -1;
    } else {
        timeout = 1000 * mp_obj_get_float(timeout_in);
    }
    int _errno;
    if (self->sock_base.nic_type->n_settimeout(self, timeout, &_errno) != 0) {
        nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(_errno)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_settimeout_obj, socket_settimeout);

// method socket.setblocking(flag)
STATIC mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t blocking) {
    if (mp_obj_is_true(blocking)) {
        return socket_settimeout(self_in, mp_const_none);
    } else {
        return socket_settimeout(self_in, MP_OBJ_NEW_SMALL_INT(0));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

STATIC mp_obj_t socket_makefile(mp_uint_t n_args, const mp_obj_t *args) {
    // TODO: CPython explicitly says that closing the returned object doesn't
    // close the original socket (Python2 at all says that fd is dup()ed). But
    // we save on the bloat.
    mod_network_socket_obj_t *self = args[0];
    if (n_args > 1) {
        const char *mode = mp_obj_str_get_str(args[1]);
        if (strcmp(mode, "rb") && strcmp(mode, "wb")) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
        }
    }
    return self;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_makefile_obj, 1, 6, socket_makefile);

STATIC const mp_map_elem_t socket_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),         (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_close),           (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bind),            (mp_obj_t)&socket_bind_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_listen),          (mp_obj_t)&socket_listen_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_accept),          (mp_obj_t)&socket_accept_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),         (mp_obj_t)&socket_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),            (mp_obj_t)&socket_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendall),         (mp_obj_t)&socket_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recv),            (mp_obj_t)&socket_recv_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendto),          (mp_obj_t)&socket_sendto_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recvfrom),        (mp_obj_t)&socket_recvfrom_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setsockopt),      (mp_obj_t)&socket_setsockopt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_settimeout),      (mp_obj_t)&socket_settimeout_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setblocking),     (mp_obj_t)&socket_setblocking_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_makefile),        (mp_obj_t)&socket_makefile_obj },

    // stream methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),            (mp_obj_t)&mp_stream_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readall),         (mp_obj_t)&mp_stream_readall_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readinto),        (mp_obj_t)&mp_stream_readinto_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline),        (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),           (mp_obj_t)&mp_stream_write_obj },
};

MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

STATIC const mp_map_elem_t raw_socket_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),         (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_close),           (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),            (mp_obj_t)&socket_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recv),            (mp_obj_t)&socket_recv_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_settimeout),      (mp_obj_t)&socket_settimeout_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bind),            (mp_obj_t)&socket_bind_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setblocking),     (mp_obj_t)&socket_setblocking_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setsockopt),      (mp_obj_t)&socket_setsockopt_obj },
};

MP_DEFINE_CONST_DICT(raw_socket_locals_dict, raw_socket_locals_dict_table);

STATIC mp_uint_t socket_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mod_network_socket_obj_t *self = self_in;
    mp_int_t ret = self->sock_base.nic_type->n_recv(self, buf, size, errcode);
    if (ret < 0) {
//        // we need to ignore the socket closed error here because a readall() or read() without params
//        // only returns when the socket is closed by the other end
//        if (*errcode != ESECCLOSED) {
            ret = MP_STREAM_ERROR;
//        } else {
//            ret = 0;  // TODO: Do we need a similar check here for lwip?
//        }
    }
    return ret;
}

STATIC mp_uint_t socket_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mod_network_socket_obj_t *self = self_in;
    mp_int_t ret = self->sock_base.nic_type->n_send(self, buf, size, errcode);
    if (ret < 0) {
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

STATIC mp_uint_t socket_ioctl(mp_obj_t self_in, mp_uint_t request, mp_uint_t arg, int *errcode) {
    mod_network_socket_obj_t *self = self_in;
    return self->sock_base.nic_type->n_ioctl(self, request, arg, errcode);
}

const mp_stream_p_t socket_stream_p = {
    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
    .is_text = false,
};

const mp_stream_p_t raw_socket_stream_p = {
    .ioctl = socket_ioctl,
    .is_text = false,
};

STATIC const mp_obj_type_t socket_type = {
    { &mp_type_type },
    .name = MP_QSTR_socket,
    .make_new = socket_make_new,
    .protocol = &socket_stream_p,
    .locals_dict = (mp_obj_t)&socket_locals_dict,
};

STATIC const mp_obj_type_t raw_socket_type = {
    { &mp_type_type },
    .name = MP_QSTR_socket,
    .make_new = socket_make_new,
    .protocol = &raw_socket_stream_p,
    .locals_dict = (mp_obj_t)&raw_socket_locals_dict,
};

///******************************************************************************/
//// usocket module

// function usocket.getaddrinfo(host, port)
/// \function getaddrinfo(host, port)
STATIC mp_obj_t mod_usocket_getaddrinfo(mp_obj_t host_in, mp_obj_t port_in) {
    mp_uint_t hlen;
    const char *host = mp_obj_str_get_data(host_in, &hlen);
    mp_int_t port = mp_obj_get_int(port_in);

    // find a nic that can do a name lookup
    for (mp_uint_t i = 0; i < MP_STATE_PORT(mod_network_nic_list).len; i++) {
        mp_obj_t nic = MP_STATE_PORT(mod_network_nic_list).items[i];
        mod_network_nic_type_t *nic_type = (mod_network_nic_type_t*)mp_obj_get_type(nic);
        if (nic_type->n_gethostbyname != NULL) {
            // ipv4 only
            uint8_t out_ip[MOD_NETWORK_IPV4ADDR_BUF_SIZE];
            int32_t result = nic_type->n_gethostbyname(host, hlen, out_ip, AF_INET);
            if (result < 0) {
                // negate result as it contains the error code which must be positive
                nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(-result)));
            }
            mp_obj_tuple_t *tuple = mp_obj_new_tuple(5, NULL);
            tuple->items[0] = MP_OBJ_NEW_SMALL_INT(AF_INET);
            tuple->items[1] = MP_OBJ_NEW_SMALL_INT(SOCK_STREAM);
            tuple->items[2] = MP_OBJ_NEW_SMALL_INT(0);
            tuple->items[3] = MP_OBJ_NEW_QSTR(MP_QSTR_);
            tuple->items[4] = netutils_format_inet_addr(out_ip, port, NETUTILS_BIG);
            return mp_obj_new_list(1, (mp_obj_t*)&tuple);
        }
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "no available NIC"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_usocket_getaddrinfo_obj, mod_usocket_getaddrinfo);

STATIC const mp_map_elem_t mp_module_usocket_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),        MP_OBJ_NEW_QSTR(MP_QSTR_usocket) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_socket),          (mp_obj_t)&socket_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getaddrinfo),     (mp_obj_t)&mod_usocket_getaddrinfo_obj },

    // class exceptions
    { MP_OBJ_NEW_QSTR(MP_QSTR_error),           (mp_obj_t)&mp_type_OSError },
    { MP_OBJ_NEW_QSTR(MP_QSTR_timeout),         (mp_obj_t)&mp_type_TimeoutError },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_AF_INET),         MP_OBJ_NEW_SMALL_INT(AF_INET) },
#ifdef LOPY
    { MP_OBJ_NEW_QSTR(MP_QSTR_AF_LORA),         MP_OBJ_NEW_SMALL_INT(AF_LORA) },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_SOCK_STREAM),     MP_OBJ_NEW_SMALL_INT(SOCK_STREAM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SOCK_DGRAM),      MP_OBJ_NEW_SMALL_INT(SOCK_DGRAM) },
#ifdef LOPY
    { MP_OBJ_NEW_QSTR(MP_QSTR_SOCK_RAW),        MP_OBJ_NEW_SMALL_INT(SOCK_RAW) },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_IPPROTO_TCP),     MP_OBJ_NEW_SMALL_INT(IPPROTO_TCP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IPPROTO_UDP),     MP_OBJ_NEW_SMALL_INT(IPPROTO_UDP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROTO_LORA),      MP_OBJ_NEW_SMALL_INT(PROTO_LORA) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_SOL_SOCKET),      MP_OBJ_NEW_SMALL_INT(SOL_SOCKET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SOL_LORA),        MP_OBJ_NEW_SMALL_INT(SOL_LORA) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_SO_REUSEADDR),    MP_OBJ_NEW_SMALL_INT(SO_REUSEADDR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SO_CONFIRMED),    MP_OBJ_NEW_SMALL_INT(SO_LORAWAN_CONFIRMED) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SO_DR),           MP_OBJ_NEW_SMALL_INT(SO_LORAWAN_DR) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_usocket_globals, mp_module_usocket_globals_table);

const mp_obj_module_t mp_module_usocket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_usocket_globals,
};
