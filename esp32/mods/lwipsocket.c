/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
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

#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modwlan.h"
#include "serverstask.h"
#include "mpexception.h"
#include "antenna.h"
#include "modussl.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwipsocket.h"


#define WLAN_MAX_RX_SIZE                    2048
#define WLAN_MAX_TX_SIZE                    1476

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

//
///******************************************************************************/
//// Micro Python bindings; LWIP socket

int lwipsocket_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family) {
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

int lwipsocket_socket_socket(mod_network_socket_obj_t *s, int *_errno) {
    int32_t sd = socket(s->sock_base.u.u_param.domain, s->sock_base.u.u_param.type, s->sock_base.u.u_param.proto);
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }

    // enable address reusing
    uint32_t option = 1;
    lwip_setsockopt_r(sd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    s->sock_base.u.sd = sd;
    return 0;
}

void lwipsocket_socket_close(mod_network_socket_obj_t *s) {
    // this is to prevent the finalizer to close a socket that failed when being created
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
        lwip_close_r(s->sock_base.u.sd);
    }
    modusocket_socket_delete(s->sock_base.u.sd);
    s->sock_base.connected = false;
}

int lwipsocket_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = lwip_bind_r(s->sock_base.u.sd, &addr, sizeof(addr));
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

int lwipsocket_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno) {
    int ret = lwip_listen_r(s->sock_base.u.sd, backlog);
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

int lwipsocket_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno) {
    // accept incoming connection
    int32_t sd;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    sd = lwip_accept_r(s->sock_base.u.sd, &addr, &addr_len);
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

int lwipsocket_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = lwip_connect_r(s->sock_base.u.sd, &addr, sizeof(addr));

    if (ret != 0) {
        // printf("Connect returned -0x%x\n", -ret);
        *_errno = errno;
        return -1;
    }

    // printf("Connected.\n");

    if (s->sock_base.is_ssl && (ret == 0)) {

        ret = lwipsocket_socket_setup_ssl(s, _errno);
    }

    s->sock_base.connected = true;
    return ret;
}

int lwipsocket_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
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
            bytes = lwip_send_r(s->sock_base.u.sd, (const void *)buf, len, 0);
        }
    }
    if (bytes <= 0) {
        *_errno = errno;
        return -1;
    }
    return bytes;
}

int lwipsocket_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
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
                // blocking and timed out, return with error
                // mbedtls_net_recv_timeout() returned with timeout
                else {
                    break;
                }
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
        ret = lwip_recv_r(s->sock_base.u.sd, buf, MIN(len, WLAN_MAX_RX_SIZE), 0);
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
    }
    return ret;
}

int lwipsocket_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    if (len > 0) {
        MAKE_SOCKADDR(addr, ip, port)
        int ret = lwip_sendto_r(s->sock_base.u.sd, (byte*)buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
        return ret;
    }
    return 0;
}

int lwipsocket_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    mp_int_t ret = lwip_recvfrom_r(s->sock_base.u.sd, buf, MIN(len, WLAN_MAX_RX_SIZE), 0, &addr, &addr_len);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    UNPACK_SOCKADDR(addr, ip, *port);
    return ret;
}

int lwipsocket_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    int ret = lwip_setsockopt_r(s->sock_base.u.sd, level, opt, optval, optlen);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

int lwipsocket_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    int ret;

    if (s->sock_base.is_ssl) {
       mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
       // mbedtls_net_recv_timeout() API is registered with mbedtls_ssl_set_bio() so setting timeout on receive works
       mbedtls_ssl_conf_read_timeout(&ss->conf, timeout_ms);
    }
    else {
        uint32_t option = lwip_fcntl_r(s->sock_base.u.sd, F_GETFL, 0);

        if (timeout_ms <= 0) {
            if (timeout_ms == 0) {
                // set non-blocking mode
                option |= O_NONBLOCK;
            } else {
                // set blocking mode
                option &= ~O_NONBLOCK;
                timeout_ms = UINT32_MAX;
            }
        } else {
            // set blocking mode
            option &= ~O_NONBLOCK;
        }

        // set the timeout
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;              // seconds
        tv.tv_usec = (timeout_ms % 1000) * 1000;    // microseconds
        ret = lwip_setsockopt_r(s->sock_base.u.sd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        ret |= lwip_setsockopt_r(s->sock_base.u.sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ret |= lwip_fcntl_r(s->sock_base.u.sd, F_SETFL, option);

        if (ret != 0) {
            *_errno = errno;
            return -1;
        }
    }

    s->sock_base.timeout = timeout_ms;
    return 0;
}

int lwipsocket_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret;
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        int32_t sd = s->sock_base.u.sd;

        // init fds
        fd_set rfds, wfds, xfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&xfds);

        // set fds if needed
        if (flags & MP_STREAM_POLL_RD) {
            FD_SET(sd, &rfds);
        }
        if (flags & MP_STREAM_POLL_WR) {
            FD_SET(sd, &wfds);
        }
        if (flags & MP_STREAM_POLL_HUP) {
            FD_SET(sd, &xfds);
        }

        // call select with the smallest possible timeout
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10;
        int32_t nfds = lwip_select(sd + 1, &rfds, &wfds, &xfds, &tv);

        // check for errors
        if (nfds < 0) {
            *_errno = errno;
            return MP_STREAM_ERROR;
        }

        // check return of select
        if (FD_ISSET(sd, &rfds)) {
            ret |= MP_STREAM_POLL_RD;
        }
        if (FD_ISSET(sd, &wfds)) {
            ret |= MP_STREAM_POLL_WR;
        }
        if (FD_ISSET(sd, &xfds)) {
            ret |= MP_STREAM_POLL_HUP;
        }
    } else {
        *_errno = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

int lwipsocket_socket_setup_ssl(mod_network_socket_obj_t *s, int *_errno)
{
    int ret;
    uint32_t count = 0;
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
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_TIMEOUT ) || count >= ss->read_timeout)
        {
            // printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
            *_errno = ret;
            return -1;
        }
        if(ret == MBEDTLS_ERR_SSL_TIMEOUT)
        {
            count++;
        }
    }

    // printf("Verifying peer X.509 certificate...\n");

    if ((ret = mbedtls_ssl_get_verify_result(&ss->ssl)) != 0) {
        /* In real life, we probably want to close connection if ret != 0 */
        // printf("Failed to verify peer certificate!\n");
        *_errno = ret;
        return -1;
    }
    // printf("Certificate verified.\n");
    return 0;
}
