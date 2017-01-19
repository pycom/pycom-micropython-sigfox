/*
 * Copyright (c) 2016, Pycom Limited.
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

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event_loop.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modwlan.h"
#include "pybioctl.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "antenna.h"
#include "modussl.h"

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
//#define CLR_STATUS_BIT_ALL(status)      (status = 0)
//#define SET_STATUS_BIT(status, bit)     (status |= ( 1 << (bit)))
//#define CLR_STATUS_BIT(status, bit)     (status &= ~(1 << (bit)))
//#define GET_STATUS_BIT(status, bit)     (0 != (status & (1 << (bit))))
//
//#define IS_NW_PROCSR_ON(status)         GET_STATUS_BIT(status, STATUS_BIT_NWP_INIT)
//#define IS_CONNECTED(status)            GET_STATUS_BIT(status, STATUS_BIT_CONNECTION)
//#define IS_IP_LEASED(status)            GET_STATUS_BIT(status, STATUS_BIT_IP_LEASED)
//#define IS_IP_ACQUIRED(status)          GET_STATUS_BIT(status, STATUS_BIT_IP_ACQUIRED)
//#define IS_SMART_CFG_START(status)      GET_STATUS_BIT(status, STATUS_BIT_SMARTCONFIG_START)
//#define IS_P2P_DEV_FOUND(status)        GET_STATUS_BIT(status, STATUS_BIT_P2P_DEV_FOUND)
//#define IS_P2P_REQ_RCVD(status)         GET_STATUS_BIT(status, STATUS_BIT_P2P_REQ_RECEIVED)
//#define IS_CONNECT_FAILED(status)       GET_STATUS_BIT(status, STATUS_BIT_CONNECTION_FAILED)
//#define IS_PING_DONE(status)            GET_STATUS_BIT(status, STATUS_BIT_PING_DONE)
//
//#define MODWLAN_SL_SCAN_ENABLE          1
//#define MODWLAN_SL_SCAN_DISABLE         0
//#define MODWLAN_SL_MAX_NETWORKS         20
//
//#define MODWLAN_MAX_NETWORKS            20
//#define MODWLAN_SCAN_PERIOD_S           3600     // 1 hour
//#define MODWLAN_WAIT_FOR_SCAN_MS        1050
//#define MODWLAN_CONNECTION_WAIT_MS      2
//
//#define ASSERT_ON_ERROR(x)              ASSERT((x) >= 0)
//
//#define IPV4_ADDR_STR_LEN_MAX           (16)

#define WLAN_MAX_RX_SIZE                1024
#define WLAN_MAX_TX_SIZE                1476

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

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
/*STATIC*/ wlan_obj_t wlan_obj = {  // FIXME (need to find a better way to register the nic automatically)
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
//
//STATIC const mp_irq_methods_t wlan_irq_methods;

static EventGroupHandle_t wifi_event_group;


// Event bits
const int CONNECTED_BIT = BIT0;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
//OsiLockObj_t wlan_LockObj; TODO

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void wlan_clear_data (void);
//STATIC void wlan_reenable (SlWlanMode_t mode);
STATIC void wlan_servers_start (void);
STATIC void wlan_servers_stop (void);
//STATIC void wlan_reset (void);
STATIC void wlan_validate_mode (uint mode);
STATIC void wlan_set_mode (uint mode);
STATIC void wlan_setup_ap (const char *ssid, uint32_t ssid_len, uint32_t auth, const char *key, uint32_t key_len,
                           uint32_t channel, bool add_mac);
STATIC void wlan_validate_ssid_len (uint32_t len);
STATIC uint32_t wlan_set_ssid_internal (const char *ssid, uint8_t len, bool add_mac);
STATIC void wlan_validate_security (uint8_t auth, const char *key);
STATIC void wlan_set_security_internal (uint8_t auth, const char *key, uint8_t len);
STATIC void wlan_validate_channel (uint8_t channel);
#if MICROPY_HW_ANTENNA_DIVERSITY
STATIC void wlan_validate_antenna (uint8_t antenna);
STATIC void wlan_set_antenna (uint8_t antenna);
#endif
static esp_err_t wlan_event_handler(void *ctx, system_event_t *event);
STATIC modwlan_Status_t wlan_do_connect (const char* ssid, uint32_t ssid_len, const char* bssid,
                                         const char* key, uint32_t key_len, int32_t timeout);
//STATIC void wlan_get_sl_mac (void);
//STATIC void wlan_wep_key_unhexlify (const char *key, char *key_out);
//STATIC void wlan_lpds_irq_enable (mp_obj_t self_in);
//STATIC void wlan_lpds_irq_disable (mp_obj_t self_in);
//STATIC bool wlan_scan_result_is_unique (const mp_obj_list_t *nets, uint8_t *bssid);

//STATIC void wlan_event_handler_cb (System_Event_t *event);

static int wlan_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family);
static int wlan_socket_socket(mod_network_socket_obj_t *s, int *_errno);
static void wlan_socket_close(mod_network_socket_obj_t *s);
static int wlan_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int wlan_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno);
static int wlan_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno);
static int wlan_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int wlan_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
static int wlan_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
static int wlan_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);
static int wlan_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);
static int wlan_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
static int wlan_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
static int wlan_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void wlan_pre_init (void) {
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wlan_event_handler, NULL));
    wlan_obj.base.type = (mp_obj_t)&mod_network_nic_type_wlan;
}

void wlan_setup (int32_t mode, const char *ssid, uint32_t ssid_len, uint32_t auth, const char *key, uint32_t key_len,
                 uint32_t channel, uint32_t antenna, bool add_mac) {

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // stop the servers
    wlan_servers_stop();

    esp_wifi_get_mac(WIFI_IF_STA, wlan_obj.mac);

    esp_wifi_stop();

    wlan_set_antenna(antenna);
    wlan_set_mode(mode);

    wifi_ps_type_t wifi_ps_type;
    if (mode != WIFI_MODE_STA) {
        wlan_setup_ap (ssid, ssid_len, auth, key, key_len, channel, add_mac);
        wifi_ps_type = WIFI_PS_NONE;
    } else {
        wifi_ps_type = WIFI_PS_MODEM;
    }

    // set the power saving mode
    ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_ps_type));

    esp_wifi_start();

    // start the servers before returning
    wlan_servers_start();
}

void wlan_stop (uint32_t timeout) {
    wlan_servers_stop();
    // sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER); FIXME
//    sl_Stop(timeout); FIXME
    wlan_clear_data();
    wlan_obj.mode = -1;
}

void wlan_get_mac (uint8_t *macAddress) {
    if (macAddress) {
        memcpy (macAddress, wlan_obj.mac, sizeof(wlan_obj.mac));
    }
}

void wlan_get_ip (uint32_t *ip) {
//    if (ip) {
//        *ip = IS_IP_ACQUIRED(wlan_obj.status) ? wlan_obj.ip : 0;
//    }
}

//bool wlan_is_connected (void) {
////    return (GET_STATUS_BIT(wlan_obj.status, STATUS_BIT_CONNECTION) &&
////            (GET_STATUS_BIT(wlan_obj.status, STATUS_BIT_IP_ACQUIRED) || wlan_obj.mode != ROLE_STA));
//}

void wlan_set_current_time (uint32_t seconds_since_2000) {
//    timeutils_struct_time_t tm;
//    timeutils_seconds_since_2000_to_struct_time(seconds_since_2000, &tm);
//
//    SlDateTime_t sl_datetime = {0};
//    sl_datetime.sl_tm_day  = tm.tm_mday;
//    sl_datetime.sl_tm_mon  = tm.tm_mon;
//    sl_datetime.sl_tm_year = tm.tm_year;
//    sl_datetime.sl_tm_hour = tm.tm_hour;
//    sl_datetime.sl_tm_min  = tm.tm_min;
//    sl_datetime.sl_tm_sec  = tm.tm_sec;
//    sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION, SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME, sizeof(SlDateTime_t), (uint8_t *)(&sl_datetime));
}

void wlan_off_on (void) {
    // no need to lock the WLAN object on every API call since the servers and the MicroPtyhon
    // task have the same priority
//    wlan_reenable(wlan_obj.mode);
}

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

STATIC esp_err_t wlan_event_handler(void *ctx, system_event_t *event) {
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
            case WIFI_REASON_AUTH_FAIL:
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
    return ESP_OK;
}

STATIC void wlan_clear_data (void) {
//    CLR_STATUS_BIT_ALL(wlan_obj.status); FIXME
    wlan_obj.ip = 0;
    //memset(wlan_obj.ssid_o, 0, sizeof(wlan_obj.ssid));
    //memset(wlan_obj.bssid, 0, sizeof(wlan_obj.bssid));
}

//STATIC void wlan_reenable (SlWlanMode_t mode) {
////    // stop and start again
////    sl_LockObjLock (&wlan_LockObj, SL_OS_WAIT_FOREVER);
////    sl_Stop(SL_STOP_TIMEOUT);
////    wlan_clear_data();
////    wlan_obj.mode = sl_Start(0, 0, 0);
////    sl_LockObjUnlock (&wlan_LockObj);
////    ASSERT (wlan_obj.mode == mode);
//}

STATIC void wlan_servers_start (void) {
    // start the servers if they were enabled before
    if (wlan_obj.enable_servers) {
        servers_start();
    }
}

STATIC void wlan_servers_stop (void) {
    if (servers_are_enabled()) {
        wlan_obj.enable_servers = true;
    }

    // stop all other processes using the wlan engine
    if (wlan_obj.enable_servers) {
        servers_stop();
    }
}

STATIC void wlan_setup_ap (const char *ssid, uint32_t ssid_len, uint32_t auth, const char *key, uint32_t key_len,
                           uint32_t channel, bool add_mac) {
    ssid_len = wlan_set_ssid_internal (ssid, ssid_len, add_mac);
    wlan_set_security_internal(auth, key, key_len);
    // get the current config and then change it
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_AP, &config);
    memcpy((char *)config.ap.ssid, (char *)wlan_obj.ssid, ssid_len);
    config.ap.ssid_len = ssid_len;
    config.ap.authmode = wlan_obj.auth;
    strcpy((char *)config.ap.password, (char *)wlan_obj.key);
    config.ap.channel = channel;
    wlan_obj.channel = channel;
    config.ap.max_connection = 4;
    esp_wifi_set_config(WIFI_IF_AP, &config);
}

//STATIC void wlan_reset (void) {
//    wlan_servers_stop();
////    wlan_reenable (wlan_obj.mode); FIXME
//    wlan_servers_start();
//}

STATIC void wlan_validate_mode (uint mode) {
    if (mode < WIFI_MODE_STA || mode > WIFI_MODE_APSTA) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_mode (uint mode) {
    wlan_obj.mode = mode;
    esp_wifi_set_mode(mode);
}

STATIC void wlan_validate_ssid_len (uint32_t len) {
    if (len > MODWLAN_SSID_LEN_MAX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC uint32_t wlan_set_ssid_internal (const char *ssid, uint8_t len, bool add_mac) {
    // save the ssid
    memcpy(&wlan_obj.ssid, ssid, len);
    // append the last 2 bytes of the MAC address, since the use of this functionality is under our control
    // we can assume that the lenght of the ssid is less than (32 - 5)
    if (add_mac) {
        snprintf((char *)&wlan_obj.ssid[len], sizeof(wlan_obj.ssid) - len, "-%02x%02x", wlan_obj.mac[4], wlan_obj.mac[5]);
        len += 5;
    }
    wlan_obj.ssid[len] = '\0';
    return len;
}

STATIC void wlan_validate_security (uint8_t auth, const char *key) {
    if (auth < WIFI_AUTH_WEP && auth > WIFI_AUTH_WPA_WPA2_PSK) {
        goto invalid_args;
    }
//    if (auth == AUTH_WEP) {
//        for (mp_uint_t i = strlen(key); i > 0; i--) {
//            if (!unichar_isxdigit(*key++)) {
//                goto invalid_args;
//            }
//        }
//    }
    return;

invalid_args:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC void wlan_set_security_internal (uint8_t auth, const char *key, uint8_t len) {
    wlan_obj.auth = auth;
//    uint8_t wep_key[32];
    if (key != NULL) {
        memcpy(wlan_obj.key, key, len);
        wlan_obj.key[len] = '\0';
//        if (auth == SL_SEC_TYPE_WEP) {
//            wlan_wep_key_unhexlify(key, (char *)&wep_key);
//            key = (const char *)&wep_key;
//            len /= 2;
//        }
    } else {
        wlan_obj.key[0] = '\0';
    }
}

STATIC void wlan_validate_channel (uint8_t channel) {
    if (channel < 1 || channel > 11) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

#if MICROPY_HW_ANTENNA_DIVERSITY
STATIC void wlan_validate_antenna (uint8_t antenna) {
    if (antenna != ANTENNA_TYPE_INTERNAL && antenna != ANTENNA_TYPE_EXTERNAL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_antenna (uint8_t antenna) {
    wlan_obj.antenna = antenna;
    antenna_select(antenna);
}
#endif

STATIC modwlan_Status_t wlan_do_connect (const char* ssid, uint32_t ssid_len, const char* bssid,
                                         const char* key, uint32_t key_len, int32_t timeout) {
    wifi_config_t config;
    memset(&config, 0, sizeof(config));

    // first close any active connections
    esp_wifi_disconnect();

    wlan_obj.disconnected = false;

    memcpy(config.sta.ssid, ssid, ssid_len);
    if (key) {
        memcpy(config.sta.password, key, key_len);
    }
    if (bssid) {
        memcpy(config.sta.bssid, bssid, sizeof(config.sta.bssid));
        config.sta.bssid_set = true;
    }

    esp_wifi_set_config(WIFI_IF_STA, &config);
    esp_wifi_connect();
    return MODWLAN_OK;

    // TODO Add timeout handling!!

//    if (!sl_WlanConnect((_i8*)ssid, ssid_len, (uint8_t*)bssid, &secParams, NULL)) {
//        // wait for the WLAN Event
//        uint32_t waitForConnectionMs = 0;
//        while (timeout && !IS_CONNECTED(wlan_obj.status)) {
//            mp_hal_delay_ms(MODWLAN_CONNECTION_WAIT_MS);
//            waitForConnectionMs += MODWLAN_CONNECTION_WAIT_MS;
//            if (timeout > 0 && waitForConnectionMs > timeout) {
//                return MODWLAN_ERROR_TIMEOUT;
//            }
//            wlan_update();
//        }
//        return MODWLAN_OK;
//    }
//    return MODWLAN_ERROR_INVALID_PARAMS;
}

//STATIC void wlan_get_sl_mac (void) {
//    // Get the MAC address
////    uint8_t macAddrLen = SL_MAC_ADDR_LEN;
////    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddrLen, wlan_obj.mac);
//}

//STATIC void wlan_wep_key_unhexlify (const char *key, char *key_out) {
//    byte hex_byte = 0;
//    for (mp_uint_t i = strlen(key); i > 0 ; i--) {
//        hex_byte += unichar_xdigit_value(*key++);
//        if (i & 1) {
//            hex_byte <<= 4;
//        } else {
//            *key_out++ = hex_byte;
//            hex_byte = 0;
//        }
//    }
//}

//STATIC void wlan_lpds_irq_enable (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    self->irq_enabled = true;
//}
//
//STATIC void wlan_lpds_irq_disable (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    self->irq_enabled = false;
//}
//
//STATIC int wlan_irq_flags (mp_obj_t self_in) {
//    wlan_obj_t *self = self_in;
//    return self->irq_flags;
//}
//
//STATIC bool wlan_scan_result_is_unique (const mp_obj_list_t *nets, uint8_t *bssid) {
//    for (int i = 0; i < nets->len; i++) {
//        // index 1 in the list is the bssid
//        mp_obj_str_t *_bssid = (mp_obj_str_t *)((mp_obj_tuple_t *)nets->items[i])->items[1];
//        if (!memcmp (_bssid->data, bssid, SL_BSSID_LENGTH)) {
//            return false;
//        }
//    }
//    return true;
//}

/******************************************************************************/
// Micro Python bindings; WLAN class

/// \class WLAN - WiFi driver

STATIC mp_obj_t wlan_init_helper(wlan_obj_t *self, const mp_arg_val_t *args) {
    // get the mode
    int8_t mode = args[0].u_int;
    wlan_validate_mode(mode);

    // get the ssid
    mp_uint_t ssid_len = 0;
    const char *ssid = NULL;
    if (args[1].u_obj != NULL) {
        ssid = mp_obj_str_get_data(args[1].u_obj, &ssid_len);
        wlan_validate_ssid_len(ssid_len);
    }

    // get the auth config
    uint8_t auth = WIFI_AUTH_OPEN;
    mp_uint_t key_len = 0;
    const char *key = NULL;
    if (args[2].u_obj != mp_const_none) {
        mp_obj_t *sec;
        mp_obj_get_array_fixed_n(args[2].u_obj, 2, &sec);
        auth = mp_obj_get_int(sec[0]);
        key = mp_obj_str_get_data(sec[1], &key_len);
        wlan_validate_security(auth, key);
    }

    // get the channel
    uint8_t channel = args[3].u_int;
    wlan_validate_channel(channel);

    // get the antenna type
    uint8_t antenna = args[4].u_int;
#if MICROPY_HW_ANTENNA_DIVERSITY
    wlan_validate_antenna(antenna);
#endif

    // initialize the wlan subsystem
    wlan_setup(mode, (const char *)ssid, ssid_len, auth, (const char *)key, key_len, channel, antenna, false);

    mod_network_register_nic(&wlan_obj);

    return mp_const_none;
}

STATIC const mp_arg_t wlan_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_MODE_STA} },
    { MP_QSTR_ssid,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_auth,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_channel,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_antenna,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = ANTENNA_TYPE_INTERNAL} },
};
STATIC mp_obj_t wlan_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), wlan_init_args, args);

    // setup the object
    wlan_obj_t *self = &wlan_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_wlan;

    // give it to the sleep module
    //pyb_sleep_set_wlan_obj(self); // FIXME

    if (n_kw > 0) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
        // start the peripheral
        wlan_init_helper(self, &args[1]);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t wlan_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &wlan_init_args[1], args);
    return wlan_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_init_obj, 1, wlan_init);

STATIC mp_obj_t wlan_deinit(mp_obj_t self_in) {

    if (servers_are_enabled()){
       wlan_servers_stop();
    }

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_deinit();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_deinit_obj, wlan_deinit);

STATIC mp_obj_t wlan_scan(mp_obj_t self_in) {
    STATIC const qstr wlan_scan_info_fields[] = {
        MP_QSTR_ssid, MP_QSTR_bssid, MP_QSTR_sec, MP_QSTR_channel, MP_QSTR_rssi
    };

    // check for correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    esp_wifi_scan_start(NULL, true);

    uint16_t ap_num;
    wifi_ap_record_t *ap_record_buffer;
    wifi_ap_record_t *ap_record;

    esp_wifi_scan_get_ap_num(&ap_num); // get the number of scanned APs
    ap_record_buffer = pvPortMalloc(ap_num * sizeof(wifi_ap_record_t));
    if (ap_record_buffer == NULL){
        return mp_const_none;
    }

    mp_obj_t nets = mp_const_none;
    // get the scanned AP list
    if (ESP_OK == esp_wifi_scan_get_ap_records(&ap_num, (wifi_ap_record_t *)ap_record_buffer)) {
        nets = mp_obj_new_list(0, NULL);
        for (int i = 0; i < ap_num; i++) {
            ap_record = &ap_record_buffer[i];
            mp_obj_t tuple[5];
            tuple[0] = mp_obj_new_str((const char *)ap_record->ssid, strlen((char *)ap_record->ssid), false);
            tuple[1] = mp_obj_new_bytes((const byte *)ap_record->bssid, sizeof(ap_record->bssid));
            tuple[2] = mp_obj_new_int(ap_record->authmode);
            tuple[3] = mp_obj_new_int(ap_record->primary);
            tuple[4] = mp_obj_new_int(ap_record->rssi);

            // add the network to the list
            mp_obj_list_append(nets, mp_obj_new_attrtuple(wlan_scan_info_fields, 5, tuple));
        }
    }
    vPortFree(ap_record_buffer);

    return nets;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_scan_obj, wlan_scan);

STATIC mp_obj_t wlan_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssid,     MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_auth,                       MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bssid,    MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,  MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the ssid
    mp_uint_t ssid_len;
    const char *ssid = mp_obj_str_get_data(args[0].u_obj, &ssid_len);
    wlan_validate_ssid_len(ssid_len);

    // get the auth
    mp_uint_t key_len = 0;
    const char *key = NULL;
    if (args[1].u_obj != mp_const_none) {
        mp_obj_t *sec;
        mp_obj_get_array_fixed_n(args[1].u_obj, 2, &sec);
        key = mp_obj_str_get_data(sec[1], &key_len);
//        wlan_validate_security(auth, key); FIXME

//        // convert the wep key if needed
//        if (auth == SL_SEC_TYPE_WEP) {
//            uint8_t wep_key[32];
//            wlan_wep_key_unhexlify(key, (char *)&wep_key);
//            key = (const char *)&wep_key;
//            key_len /= 2;
//        }
    }

    // get the bssid
    const char *bssid = NULL;
    if (args[2].u_obj != mp_const_none) {
        bssid = mp_obj_str_get_str(args[2].u_obj);
    }

    // get the timeout
    int32_t timeout = -1;
    if (args[3].u_obj != mp_const_none) {
        timeout = mp_obj_get_int(args[3].u_obj);
    }

    // copy the new ssid and connect to the requested access point
    strcpy((char *)wlan_obj.ssid, ssid);
    modwlan_Status_t status;
    status = wlan_do_connect (ssid, ssid_len, bssid, key, key_len, timeout);
    if (status == MODWLAN_ERROR_TIMEOUT) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    } else if (status == MODWLAN_ERROR_INVALID_PARAMS) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
    memcpy(wlan_obj.key, key, key_len);
    wlan_obj.key[key_len] = '\0';
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_connect_obj, 1, wlan_connect);

STATIC mp_obj_t wlan_disconnect(mp_obj_t self_in) {
    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    esp_wifi_disconnect();
    wlan_obj.disconnected = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_disconnect_obj, wlan_disconnect);

STATIC mp_obj_t wlan_isconnected(mp_obj_t self_in) {
    if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_isconnected_obj, wlan_isconnected);

STATIC mp_obj_t wlan_ifconfig (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
   STATIC const mp_arg_t wlan_ifconfig_args[] = {
       { MP_QSTR_id,               MP_ARG_INT,     {.u_int = 0} },
       { MP_QSTR_config,           MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
   };

   // parse args
   mp_arg_val_t args[MP_ARRAY_SIZE(wlan_ifconfig_args)];
   mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), wlan_ifconfig_args, args);

   // check the interface id
   if (args[0].u_int != 0) {
       nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
   }

   ip_addr_t dns_addr;
   // get the configuration
   if (args[1].u_obj == MP_OBJ_NULL) {
        // get
        tcpip_adapter_ip_info_t ip_info;
        dns_addr = dns_getserver(0);
        if (ESP_OK == tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info)) {
            mp_obj_t ifconfig[4] = {
                netutils_format_ipv4_addr((uint8_t *)&ip_info.ip.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.gw.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&dns_addr.u_addr.ip4.addr, NETUTILS_BIG)
            };
            return mp_obj_new_tuple(4, ifconfig);
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
   } else { // set the configuration
       if (MP_OBJ_IS_TYPE(args[1].u_obj, &mp_type_tuple)) {
           // set a static ip
           mp_obj_t *items;
           mp_obj_get_array_fixed_n(args[1].u_obj, 4, &items);

           // stop the DHCP client first
           tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

           tcpip_adapter_ip_info_t ip_info;
           netutils_parse_ipv4_addr(items[0], (uint8_t *)&ip_info.ip.addr, NETUTILS_BIG);
           netutils_parse_ipv4_addr(items[1], (uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG);
           netutils_parse_ipv4_addr(items[2], (uint8_t *)&ip_info.gw.addr, NETUTILS_BIG);
           netutils_parse_ipv4_addr(items[3], (uint8_t *)&dns_addr.u_addr.ip4.addr, NETUTILS_BIG);
           tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
           // dns_setserver(1, &dns_addr); FIXME, this doesn't seem to be supported by the IDF

           // if (wlan_obj.mode == ROLE_AP) {
           //     ASSERT_ON_ERROR(sl_NetCfgSet(SL_IPV4_AP_P2P_GO_STATIC_ENABLE, IPCONFIG_MODE_ENABLE_IPV4, sizeof(SlNetCfgIpV4Args_t), (uint8_t *)&ipV4));
           //     SlNetAppDhcpServerBasicOpt_t dhcpParams;
           //     dhcpParams.lease_time      =  4096;                             // lease time (in seconds) of the IP Address
           //     dhcpParams.ipv4_addr_start =  ipV4.ipV4 + 1;                    // first IP Address for allocation.
           //     dhcpParams.ipv4_addr_last  =  (ipV4.ipV4 & 0xFFFFFF00) + 254;   // last IP Address for allocation.
           //     ASSERT_ON_ERROR(sl_NetAppStop(SL_NET_APP_DHCP_SERVER_ID));      // stop DHCP server before settings
           //     ASSERT_ON_ERROR(sl_NetAppSet(SL_NET_APP_DHCP_SERVER_ID, NETAPP_SET_DHCP_SRV_BASIC_OPT,
           //                     sizeof(SlNetAppDhcpServerBasicOpt_t), (uint8_t* )&dhcpParams));  // set parameters
           //     ASSERT_ON_ERROR(sl_NetAppStart(SL_NET_APP_DHCP_SERVER_ID));     // start DHCP server with new settings
           // } else {
           //     ASSERT_ON_ERROR(sl_NetCfgSet(SL_IPV4_STA_P2P_CL_STATIC_ENABLE, IPCONFIG_MODE_ENABLE_IPV4, sizeof(SlNetCfgIpV4Args_t), (uint8_t *)&ipV4));
           // }
       } else {
           // check for the correct string
           const char *mode = mp_obj_str_get_str(args[1].u_obj);
           if (strcmp("dhcp", mode) && strcmp("auto", mode)) {
               nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
           }

           // // only if we are not in AP mode
           // if (wlan_obj.mode != ROLE_AP) {
           //     uint8_t val = 1;
           //     sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, IPCONFIG_MODE_ENABLE_IPV4, 1, &val);
           // }

           if (ESP_OK != tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA)) {
               nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
           }
       }
       // // config values have changed, so reset
       // wlan_reset();
       // // set current time and date (needed to validate certificates)
       // wlan_set_current_time (pyb_rtc_get_seconds());
       return mp_const_none;
   }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_ifconfig_obj, 1, wlan_ifconfig);

STATIC mp_obj_t wlan_mode (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->mode);
    } else {
        uint mode = mp_obj_get_int(args[1]);
        wlan_validate_mode(mode);
        wlan_set_mode(mode);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_mode_obj, 1, 2, wlan_mode);

STATIC mp_obj_t wlan_ssid (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_str((const char *)self->ssid, strlen((const char *)self->ssid), false);
    } else {
        mp_uint_t ssid_len;
        const char *ssid = mp_obj_str_get_data(args[1], &ssid_len);
        wlan_validate_ssid_len(ssid_len);
        ssid_len = wlan_set_ssid_internal (ssid, ssid_len, false);
        // get the current config and then change it
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        memcpy((char *)config.ap.ssid, (char *)wlan_obj.ssid, ssid_len);
        config.ap.ssid_len = ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_ssid_obj, 1, 2, wlan_ssid);

STATIC mp_obj_t wlan_bssid (mp_obj_t self_in) {
    wlan_obj_t *self = self_in;
    return mp_obj_new_bytes((const byte *)self->bssid, 6);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_bssid_obj, wlan_bssid);

STATIC mp_obj_t wlan_auth (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        if (self->auth == WIFI_AUTH_OPEN) {
            return mp_const_none;
        } else {
            mp_obj_t security[2];
            security[0] = mp_obj_new_int(self->auth);
            security[1] = mp_obj_new_str((const char *)self->key, strlen((const char *)self->key), false);
            return mp_obj_new_tuple(2, security);
        }
    } else {
        // get the auth config
        uint8_t auth = WIFI_AUTH_OPEN;
        mp_uint_t key_len = 0;
        const char *key = NULL;
        if (args[1] != mp_const_none) {
            mp_obj_t *sec;
            mp_obj_get_array_fixed_n(args[1], 2, &sec);
            auth = mp_obj_get_int(sec[0]);
            key = mp_obj_str_get_data(sec[1], &key_len);
            wlan_validate_security(auth, key);
        }
        wlan_set_security_internal(auth, key, key_len);
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        config.ap.authmode = wlan_obj.auth;
        strcpy((char *)config.ap.password, (char *)wlan_obj.key);
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_auth_obj, 1, 2, wlan_auth);

STATIC mp_obj_t wlan_channel (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->channel);
    } else {
        uint8_t channel  = mp_obj_get_int(args[1]);
        wlan_validate_channel(channel);
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        config.ap.channel = channel;
        wlan_obj.channel = channel;
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_channel_obj, 1, 2, wlan_channel);

STATIC mp_obj_t wlan_antenna (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->antenna);
    } else {
    #if MICROPY_HW_ANTENNA_DIVERSITY
        uint8_t antenna  = mp_obj_get_int(args[1]);
        wlan_validate_antenna(antenna);
        wlan_set_antenna(antenna);
    #endif
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_antenna_obj, 1, 2, wlan_antenna);

STATIC mp_obj_t wlan_mac (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_bytes((const byte *)self->mac, sizeof(self->mac));
    } else {
//        mp_buffer_info_t bufinfo;
//        mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
//        if (bufinfo.len != 6) {
//            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
//        }
//        memcpy(self->mac, bufinfo.buf, SL_MAC_ADDR_LEN);
//        sl_NetCfgSet(SL_MAC_ADDRESS_SET, 1, SL_MAC_ADDR_LEN, (uint8_t *)self->mac);
//        wlan_reset(); FIXME
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_mac_obj, 1, 2, wlan_mac);

//STATIC mp_obj_t wlan_irq (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
//    mp_arg_val_t args[mp_irq_INIT_NUM_ARGS];
//    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, mp_irq_INIT_NUM_ARGS, mp_irq_init_args, args);
//
//    wlan_obj_t *self = pos_args[0];
//
//    // check the trigger, only one type is supported
//    if (mp_obj_get_int(args[0].u_obj) != MODWLAN_WIFI_EVENT_ANY) {
//        goto invalid_args;
//    }
//
//    // check the power mode
//    if (mp_obj_get_int(args[3].u_obj) != PYB_PWR_MODE_LPDS) {
//        goto invalid_args;
//    }
//
//    // create the callback
//    mp_obj_t _irq = mp_irq_new (self, args[2].u_obj, &wlan_irq_methods);
//    self->irq_obj = _irq;
//
//    // enable the irq just before leaving
//    wlan_lpds_irq_enable(self);
//
//    return _irq;
//
//invalid_args:
//    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
//}
//STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_irq_obj, 1, wlan_irq);

STATIC const mp_map_elem_t wlan_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&wlan_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&wlan_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_scan),                (mp_obj_t)&wlan_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&wlan_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&wlan_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&wlan_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig),            (mp_obj_t)&wlan_ifconfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mode),                (mp_obj_t)&wlan_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ssid),                (mp_obj_t)&wlan_ssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bssid),               (mp_obj_t)&wlan_bssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_auth),                (mp_obj_t)&wlan_auth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),             (mp_obj_t)&wlan_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_antenna),             (mp_obj_t)&wlan_antenna_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&wlan_mac_obj },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_irq),                 (mp_obj_t)&wlan_irq_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_connections),         (mp_obj_t)&wlan_connections_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_urn),                 (mp_obj_t)&wlan_urn_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA),                 MP_OBJ_NEW_SMALL_INT(WIFI_MODE_STA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP),                  MP_OBJ_NEW_SMALL_INT(WIFI_MODE_AP) },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_STA_AP),              MP_OBJ_NEW_SMALL_INT(STATIONAP_MODE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WEP),                 MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WEP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA),                 MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA2),                MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_INT_ANT),             MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_INTERNAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EXT_ANT),             MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_EXTERNAL) },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_ANY_EVENT),           MP_OBJ_NEW_SMALL_INT(MODWLAN_WIFI_EVENT_ANY) },
};
STATIC MP_DEFINE_CONST_DICT(wlan_locals_dict, wlan_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_wlan = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_WLAN,
        .make_new = wlan_make_new,
        .locals_dict = (mp_obj_t)&wlan_locals_dict,
    },

    .n_gethostbyname = wlan_gethostbyname,
    .n_socket = wlan_socket_socket,
    .n_close = wlan_socket_close,
    .n_bind = wlan_socket_bind,
    .n_listen = wlan_socket_listen,
    .n_accept = wlan_socket_accept,
    .n_connect = wlan_socket_connect,
    .n_send = wlan_socket_send,
    .n_recv = wlan_socket_recv,
    .n_sendto = wlan_socket_sendto,
    .n_recvfrom = wlan_socket_recvfrom,
    .n_setsockopt = wlan_socket_setsockopt,
    .n_settimeout = wlan_socket_settimeout,
    .n_ioctl = wlan_socket_ioctl,
};

//STATIC const mp_irq_methods_t wlan_irq_methods = {
//    .init = wlan_irq,
//    .enable = wlan_lpds_irq_enable,
//    .disable = wlan_lpds_irq_disable,
//    .flags = wlan_irq_flags,
//};
//
///******************************************************************************/
//// Micro Python bindings; WLAN socket

static int wlan_gethostbyname(const char *name, mp_uint_t len, uint8_t *out_ip, mp_uint_t family) {
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

static int wlan_socket_socket(mod_network_socket_obj_t *s, int *_errno) {
    int32_t sd = socket(s->sock_base.u_param.domain, s->sock_base.u_param.type, s->sock_base.u_param.proto);
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }

    // enable address reusing
    uint32_t option = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    s->sock_base.sd = sd;
    return 0;
}

static void wlan_socket_close(mod_network_socket_obj_t *s) {
    // this is to prevent the finalizer to close a socket that failed when being created
    if (s->sock_base.sd >= 0) {
        modusocket_socket_delete(s->sock_base.sd);
        if (s->sock_base.is_ssl) {
            mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
            if (ss->sock_base.connected) {
                mbedtls_ssl_close_notify(&ss->ssl);
            }
            mbedtls_net_free(&ss->context_fd);
            mbedtls_ssl_free(&ss->ssl);
            mbedtls_ssl_config_free(&ss->conf);
            mbedtls_ctr_drbg_free(&ss->ctr_drbg);
            mbedtls_entropy_free(&ss->entropy);
        } else {
            close(s->sock_base.sd);
        }
        s->sock_base.connected = false;
        s->sock_base.sd = -1;
    }
}

static int wlan_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = bind(s->sock_base.sd, &addr, sizeof(addr));
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int wlan_socket_listen(mod_network_socket_obj_t *s, mp_int_t backlog, int *_errno) {
    int ret = listen(s->sock_base.sd, backlog);
    if (ret != 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int wlan_socket_accept(mod_network_socket_obj_t *s, mod_network_socket_obj_t *s2, byte *ip, mp_uint_t *port, int *_errno) {
    // accept incoming connection
    int32_t sd;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    sd = accept(s->sock_base.sd, &addr, &addr_len);
    // save the socket descriptor
    s2->sock_base.sd = sd;
    if (sd < 0) {
        *_errno = errno;
        return -1;
    }

    s2->sock_base.connected = true;

    // return ip and port
    UNPACK_SOCKADDR(addr, ip, *port);
    return 0;
}

static int wlan_socket_connect(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = connect(s->sock_base.sd, &addr, sizeof(addr));

    // printf("Connected.\n");

    if (s->sock_base.is_ssl && (ret == 0)) {
        mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
        mbedtls_ssl_set_bio(&ss->ssl, &ss->context_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        // printf("Performing the SSL/TLS handshake...\n");

        while ((ret = mbedtls_ssl_handshake(&ss->ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                // printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
                *_errno = errno;
                return -1;
            }
        }

        // printf("Verifying peer X.509 certificate...\n");

        int flags;
        if ((flags = mbedtls_ssl_get_verify_result(&ss->ssl)) != 0) {
            /* In real life, we probably want to close connection if ret != 0 */
            // printf("Failed to verify peer certificate!\n");
            ret = -1;
        } else {
            // printf("Certificate verified.\n");
        }
    }

    if (ret != 0) {
        *_errno = errno;
        // printf("Connect failed with %d\n", ret);
        return -1;
    }

    s->sock_base.connected = true;

    return 0;
}

static int wlan_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    mp_int_t bytes = 0;
    if (len > 0) {
        if (s->sock_base.is_ssl) {
            mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
            while ((bytes = mbedtls_ssl_write(&ss->ssl, (const unsigned char *)buf, len)) <= 0) {
                if (bytes != MBEDTLS_ERR_SSL_WANT_READ && bytes != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    // printf("mbedtls_ssl_write returned -0x%x\n", -bytes);
                    break;
                } else {
                    *_errno = EAGAIN;
                    return -1;
                }
            }
        } else {
            bytes = send(s->sock_base.sd, (const void *)buf, len, 0);
        }
    }
    if (bytes <= 0) {
        *_errno = errno;
        return -1;
    }
    return bytes;
}

static int wlan_socket_recv(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    int ret;
    if (s->sock_base.is_ssl) {
        mp_obj_ssl_socket_t *ss = (mp_obj_ssl_socket_t *)s;
        do {
            ret = mbedtls_ssl_read(&ss->ssl, (unsigned char *)buf, len);
            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_TIMEOUT || ret == -0x4C) { // FIXME
                // printf("Nothing to read\n");
                *_errno = EAGAIN;
                return -1;
            } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
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
        ret = recv(s->sock_base.sd, buf, MIN(len, WLAN_MAX_RX_SIZE), 0);
        if (ret < 0) {
            *_errno = errno;
            return -1;
        }
    }
    return ret;
}

static int wlan_socket_sendto( mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    MAKE_SOCKADDR(addr, ip, port)
    int ret = sendto(s->sock_base.sd, (byte*)buf, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return ret;
}

static int wlan_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    mp_int_t ret = recvfrom(s->sock_base.sd, buf, MIN(len, WLAN_MAX_RX_SIZE), 0, &addr, &addr_len);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    UNPACK_SOCKADDR(addr, ip, *port);
    return ret;
}

static int wlan_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    int ret = setsockopt(s->sock_base.sd, level, opt, optval, optlen);
    if (ret < 0) {
        *_errno = errno;
        return -1;
    }
    return 0;
}

static int wlan_socket_settimeout(mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    int ret;
    if (timeout_ms <= 0) {
        uint32_t option = fcntl(s->sock_base.sd, F_GETFL, 0);
        if (timeout_ms == 0) {
            // set non-blocking mode
            option |= O_NONBLOCK;
        } else {
            // set blocking mode
            option &= ~O_NONBLOCK;
        }
        ret = fcntl(s->sock_base.sd, F_SETFL, option);
    } else {
        // set timeout
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;              // seconds
        tv.tv_usec = (timeout_ms % 1000) * 1000;    // microseconds
        ret = setsockopt(s->sock_base.sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    if (ret != 0) {
        *_errno = errno;
        return -1;
    }

    s->sock_base.timeout = timeout_ms;
    return 0;
}

static int wlan_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret;
    if (request == MP_IOCTL_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        int32_t sd = s->sock_base.sd;

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
        *_errno = EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}
