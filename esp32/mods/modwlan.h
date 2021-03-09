/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODWLAN_H_
#define MODWLAN_H_

#include <tcpip_adapter.h>

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define SIMPLELINK_SPAWN_TASK_PRIORITY              3
#define SIMPLELINK_TASK_STACK_SIZE                  2048
#define SL_STOP_TIMEOUT                             35
#define SL_STOP_TIMEOUT_LONG                        575

#define MODWLAN_WIFI_EVENT_ANY                      0x01

#define MODWLAN_SSID_LEN_MAX                        32

//Triggers

#define MOD_WLAN_TRIGGER_PKT_MGMT                    0x00000001    // 1
#define MOD_WLAN_TRIGGER_PKT_CTRL                    0x00000002    // 2
#define MOD_WLAN_TRIGGER_PKT_DATA                    0x00000004    // 4
#define MOD_WLAN_TRIGGER_PKT_MISC                    0x00000008    // 8
#define MOD_WLAN_TRIGGER_PKT_DATA_MPDU               0x00000010    // 16
#define MOD_WLAN_TRIGGER_PKT_DATA_AMPDU              0x00000020    // 32
#define MOD_WLAN_TRIGGER_PKT_ANY                     0x0000003F

#define MOD_WLAN_SMART_CONFIG_DONE                   0x00000040    // 64
#define MOD_WLAN_SMART_CONFIG_TIMEOUT                0x00000080    // 128

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct _wlan_wpa2_ent_obj_t {
    const char *ca_certs_path;
    const char *client_key_path;
    const char *client_cert_path;
    const char *identity;
    const char *username;
} wlan_wpa2_ent_obj_t;

typedef enum
{
    WLAN_PHY_11_B = 1,
    WLAN_PHY_11_G,
    WLAN_PHY_11_N,
    WLAN_PHY_LOW_RATE
}_wlan_protocol_t;

typedef struct _wlan_obj_t {
    mp_obj_base_t           base;
    wlan_wpa2_ent_obj_t     wpa2_ent;
    vstr_t                  vstr_ca;
    vstr_t                  vstr_cert;
    vstr_t                  vstr_key;

    uint32_t                status;

    uint32_t                ip;

    int8_t                  mode;
    int8_t                  bandwidth;
    uint8_t                 auth;
    uint8_t                 channel;
    uint8_t                 antenna;
    int8_t                  max_tx_pwr;
    wifi_country_t*         country;

    // my own ssid, key and mac
    uint8_t                 ssid[(MODWLAN_SSID_LEN_MAX + 1)];
    uint8_t                 key[65];
    uint8_t                 mac[6];
    uint8_t                 mac_ap[6];
    uint8_t                 hostname[TCPIP_HOSTNAME_MAX_SIZE];

    // the sssid (or name) and mac of the other device
    uint8_t                 ssid_o[33];
    uint8_t                 bssid[6];

    uint8_t                 irq_flags;
    bool                    irq_enabled;
    bool                    disconnected;
    bool                    sta_conn_timeout;
    bool                    soft_ap_stopped;
    bool                    sta_stopped;
    bool                    pwrsave;
    bool                    started;
    bool                    is_promiscuous;
    uint32_t                trigger;
    int32_t                 events;
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
    SemaphoreHandle_t       mutex;
} wlan_obj_t;

typedef struct wlan_internal_prom_t
{
    wifi_pkt_rx_ctrl_t                rx_ctrl;
    uint8_t*                        data;
    wifi_promiscuous_pkt_type_t        pkt_type;
}wlan_internal_prom_t;

#pragma pack(1)
typedef struct wlan_internal_setup_t
{
    int32_t             mode;
    const char *        ssid_sta;
    const char *        key_sta;
    const char *        ssid_ap;
    const char *        key_ap;
    uint32_t             auth;
    uint32_t             channel;
    uint32_t             antenna;
    bool                 add_mac;
    bool                 hidden;
    wifi_bandwidth_t     bandwidth;
    wifi_country_t*        country;
    int8_t*                max_tx_pr;
}wlan_internal_setup_t;
#pragma pack()

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern wlan_obj_t wlan_obj;
extern TaskHandle_t SmartConfTaskHandle;
/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void wlan_pre_init (void);
extern void wlan_setup (wlan_internal_setup_t * config);
extern void wlan_update(void);
extern void wlan_get_mac (uint8_t *macAddress);
extern void wlan_off_on (void);
extern mp_obj_t wlan_deinit(mp_obj_t self_in);
extern void wlan_resume (bool reconnect);

#endif /* MODWLAN_H_ */
