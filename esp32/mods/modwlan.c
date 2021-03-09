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

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event_loop.h"
#include "ff.h"
#include "lfs.h"
#include "vfs_littlefs.h"
#include "esp_wpa2.h"
#include "esp_smartconfig.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "modwlan.h"
#include "py/stream.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "antenna.h"
#include "modussl.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwipsocket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mpirq.h"
#include "mptask.h"
#include "pycom_config.h"
#include "pycom_general_util.h"
#include "app_sys_evt.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define BSSID_MAX_SIZE                        6
#define MAX_WLAN_KEY_SIZE                    65
#define MAX_AP_CONNECTED_STA                4
#define MAX_WIFI_CHANNELS                    14

#define MAX_WIFI_PROM_PKT_SIZE                4096

#define MAX_WIFI_PKT_PARAMS                    18

#define SMART_CONF_TASK_STACK_SIZE              4096

#define SMART_CONF_TASK_PRIORITY                5

#define CHECK_ESP_ERR( x, gotofun ) if(ESP_OK != x) { goto gotofun; }

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
wlan_obj_t wlan_obj = {
    .started = false,
    .soft_ap_stopped = true,
    .sta_stopped = true,
    .disconnected = true,
    .irq_flags = 0,
    .irq_enabled = false,
    .pwrsave = false,
    .is_promiscuous = false,
    .sta_conn_timeout = false,
    .country = NULL
};

/* TODO: Maybe we can add possibility to create IRQs for wifi events */

static EventGroupHandle_t wifi_event_group;

static uint16_t mod_wlan_ap_number_of_connections = 0;

/* Variables holding wlan conn params for wakeup recovery of connections */
static uint8_t wlan_conn_recover_auth;
static int32_t wlan_conn_recover_timeout;
static uint8_t wlan_conn_recover_scan_channel;
static bool wlan_conn_recover_hidden = false;

static wlan_wpa2_ent_obj_t wlan_wpa2_ent;
static TimerHandle_t wlan_conn_timeout_timer = NULL;
static TimerHandle_t wlan_smartConfig_timeout = NULL;
static uint8_t wlan_prom_data_buff[2][MAX_WIFI_PROM_PKT_SIZE] = {0};
static wlan_internal_prom_t wlan_prom_packet[2];

static uint8_t token = 0;

// Event bits
const int CONNECTED_BIT = BIT0;

static bool is_inf_up = false;

//mutex for Timeout Counter protection
SemaphoreHandle_t timeout_mutex;
#if defined(FIPY) || defined(GPY)
// Variable saving DNS info
static tcpip_adapter_dns_info_t wlan_sta_inf_dns_info;
#endif
SemaphoreHandle_t smartConfigTimeout_mutex;

static const int ESPTOUCH_DONE_BIT = BIT1;
static const int ESPTOUCH_STOP_BIT = BIT2;
static const int ESPTOUCH_SLEEP_STOP_BIT = BIT3;
static bool wlan_smart_config_enabled = false;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void wlan_validate_mode (uint mode);
STATIC void wlan_set_mode (uint mode);
STATIC void wlan_validate_bandwidth (wifi_bandwidth_t mode);
STATIC bool wlan_set_bandwidth (wifi_bandwidth_t mode);
STATIC void wlan_validate_hostname (const char *hostname);
STATIC bool wlan_set_hostname (const char *hostname);
STATIC esp_err_t wlan_update_hostname ();
STATIC void wlan_setup_ap (const char *ssid, uint32_t auth, const char *key, uint32_t channel, bool add_mac, bool hidden);
STATIC void wlan_validate_ssid_len (uint32_t len);
STATIC uint32_t wlan_set_ssid_internal (const char *ssid, uint8_t len, bool add_mac);
STATIC void wlan_validate_security (uint8_t auth, const char *key);
STATIC void wlan_set_security_internal (uint8_t auth, const char *key);
STATIC void wlan_validate_channel (uint8_t channel);
STATIC void wlan_set_antenna (uint8_t antenna);
static esp_err_t wlan_event_handler(void *ctx, system_event_t *event);
STATIC void wlan_do_connect (const char* ssid, const char* bssid, const wifi_auth_mode_t auth, const char* key, int32_t timeout, const wlan_wpa2_ent_obj_t * const wpa2_ent, const char *hostname, uint8_t channel);
static void wlan_init_wlan_recover_params(void);
static void wlan_timer_callback( TimerHandle_t xTimer );
static void wlan_validate_country(const char * country);
static void wlan_validate_country_policy(uint8_t policy);
STATIC void wlan_stop_sta_conn_timer();
STATIC void wlan_set_default_inf(void);
STATIC void wlan_stop_smartConfig_timer();
static void smart_config_callback(smartconfig_status_t status, void *pdata);
static void TASK_SMART_CONFIG (void *pvParameters);
STATIC void wlan_callback_handler(void* arg);
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
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    app_sys_register_evt_cb(APP_SYS_EVT_WIFI, wlan_event_handler);
    wlan_obj.base.type = (mp_obj_t)&mod_network_nic_type_wlan;
    wlan_wpa2_ent.ca_certs_path = NULL;
    wlan_wpa2_ent.client_cert_path = NULL;
    wlan_wpa2_ent.client_key_path = NULL;
    wlan_wpa2_ent.identity = NULL;
    wlan_wpa2_ent.username = NULL;
    wlan_init_wlan_recover_params();
    wlan_obj.disconnected = true;
    wlan_obj.soft_ap_stopped = true;
    wlan_obj.sta_stopped = true;
    wlan_obj.started = false;
    wlan_obj.is_promiscuous = false;
    wlan_obj.events = 0;
    (wlan_prom_packet[0].data) = (uint8_t*) (&(wlan_prom_data_buff[0][0]));
    (wlan_prom_packet[1].data) = (uint8_t*) (&(wlan_prom_data_buff[1][0]));
    wlan_obj.mutex = xSemaphoreCreateMutex();
    timeout_mutex = xSemaphoreCreateMutex();
    smartConfigTimeout_mutex = xSemaphoreCreateMutex();
    // create Smart Config Task
    xTaskCreatePinnedToCore(TASK_SMART_CONFIG, "SmartConfig", SMART_CONF_TASK_STACK_SIZE / sizeof(StackType_t), NULL, SMART_CONF_TASK_PRIORITY, &SmartConfTaskHandle, 1);
}

void wlan_resume (bool reconnect)
{
    // Configure back WLAN as it was before if reconnect is TRUE
    if(reconnect) {
        // If SmartConfig enabled then re-start it
        if(wlan_smart_config_enabled) {
            // Do initial configuration as at this point the Wifi Driver is not initialized
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
            wlan_set_antenna(wlan_obj.antenna);
            wlan_set_mode(wlan_obj.mode);
            wlan_set_bandwidth(wlan_obj.bandwidth);
            if(wlan_obj.country != NULL) {
                esp_wifi_set_country(wlan_obj.country);
            }
            xTaskNotifyGive(SmartConfTaskHandle);
        }
        // Otherwise set up WLAN with the same parameters as it was before
        else {
            // In wlan_setup the wlan_obj.country is overwritten with the value coming from setup_config, need to save it out
            wifi_country_t country;
            wifi_country_t* country_ptr = NULL;
            if(wlan_obj.country != NULL) {
                memcpy(&country, wlan_obj.country, sizeof(wifi_country_t));
                country_ptr = &country;
            }

            wlan_internal_setup_t setup_config = {
                    wlan_obj.mode,
                    (const char *)(wlan_obj.ssid_o),
                    (const char *)(wlan_obj.key),
                    (const char *)(wlan_obj.ssid),
                    (const char *)(wlan_obj.key),
                    wlan_obj.auth,
                    wlan_obj.channel,
                    wlan_obj.antenna,
                    false,
                    wlan_conn_recover_hidden,
                    wlan_obj.bandwidth,
                    country_ptr,
                    &(wlan_obj.max_tx_pwr)
            };

            // Initialize & reconnect to the previous connection
            wlan_setup(&setup_config);
        }
    }
}

void wlan_setup (wlan_internal_setup_t *config) {

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_wifi_get_mac(WIFI_IF_STA, wlan_obj.mac);

    wlan_set_antenna(config->antenna);
    wlan_set_mode(config->mode);
    wlan_set_bandwidth(config->bandwidth);

    if (config->country != NULL) {
        esp_err_t ret = esp_wifi_set_country(config->country);
        if(ESP_OK != ret)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        if(wlan_obj.country == NULL) {
            wlan_obj.country = (wifi_country_t*)malloc(sizeof(wifi_country_t));
        }
        memcpy(wlan_obj.country, config->country, sizeof(wifi_country_t));
    }

    if(config->max_tx_pr != NULL)
    {
        if (*(config->max_tx_pr) > 0) {
            // set the max_tx_power
            if(ESP_OK == esp_wifi_set_max_tx_power(*(config->max_tx_pr)))
            {
                wlan_obj.max_tx_pwr = *(config->max_tx_pr);
            }
        }
    }
    switch(config->mode)
    {
        case WIFI_MODE_AP:
           MP_THREAD_GIL_EXIT();
           // wlan_setup_ap must be called only when GIL is not locked
           wlan_setup_ap (config->ssid_ap, config->auth, config->key_ap, config->channel, config->add_mac, config->hidden);
           // Start Wifi
           esp_wifi_start();
           wlan_obj.started = true;
           MP_THREAD_GIL_ENTER();
        break;
        case WIFI_MODE_APSTA:
        case WIFI_MODE_STA:
            MP_THREAD_GIL_EXIT();
            if(config->mode == WIFI_MODE_APSTA)
            {
                // wlan_setup_ap must be called only when GIL is not locked
                wlan_setup_ap (config->ssid_ap, config->auth, config->key_ap, config->channel, config->add_mac, config->hidden);
            }
            // Start Wifi
            esp_wifi_start();
            wlan_obj.started = true;
            if (config->ssid_sta != NULL) {
                if (config->auth == WIFI_AUTH_WPA2_ENTERPRISE) {

                    // Take back the GIL as the pycom_util_read_file() uses MicroPython APIs (allocates memory with GC)
                    MP_THREAD_GIL_ENTER();

                    if (wlan_wpa2_ent.ca_certs_path != NULL) {
                        pycom_util_read_file(wlan_wpa2_ent.ca_certs_path, &wlan_obj.vstr_ca);
                    }

                    if (wlan_wpa2_ent.client_key_path != NULL && wlan_wpa2_ent.client_cert_path != NULL) {
                        pycom_util_read_file(wlan_wpa2_ent.client_key_path, &wlan_obj.vstr_key);
                        pycom_util_read_file(wlan_wpa2_ent.client_cert_path, &wlan_obj.vstr_cert);
                    }

                    MP_THREAD_GIL_EXIT();
                }

                // connect to the requested access point
                wlan_do_connect (config->ssid_sta, NULL, config->auth, config->key_sta, 30000, &wlan_wpa2_ent, NULL, 0);
            }

            MP_THREAD_GIL_ENTER();
            break;
        default:
            break;
    }
}

void wlan_get_mac (uint8_t *macAddress) {
    if (macAddress) {
        memcpy (macAddress, wlan_obj.mac, sizeof(wlan_obj.mac));
    }
}

void wlan_off_on (void) {
    // no need to lock the WLAN object on every API call since the servers and the MicroPtyhon
    // task have the same priority
    /* TODO: */
}

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

STATIC bool wlan_is_inf_up(void)
{
	return is_inf_up;
}

STATIC esp_err_t wlan_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START: /**< ESP32 station start */
            wlan_obj.sta_stopped = false;
            break;
        case SYSTEM_EVENT_STA_STOP:                 /**< ESP32 station stop */
            wlan_obj.sta_stopped = true;
            break;
        case SYSTEM_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
        {
            system_event_sta_connected_t *_event = (system_event_sta_connected_t *)&event->event_info;
            memcpy(wlan_obj.bssid, _event->bssid, 6);
            memcpy(wlan_obj.ssid_o, _event->ssid, 32);
            wlan_obj.channel = _event->channel;
            wlan_obj.auth = _event->authmode;
            wlan_obj.disconnected = false;
            /* Stop Conn timeout counter*/
            wlan_stop_sta_conn_timer();
        }
            break;
        case SYSTEM_EVENT_STA_GOT_IP: /**< ESP32 station got IP from connected AP */
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            mod_network_register_nic(&wlan_obj);
#if defined(FIPY) || defined(GPY)
            // Save DNS info for restoring if wifi inf is usable again after LTE disconnect
            tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &wlan_sta_inf_dns_info);
#endif
            is_inf_up = true;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from AP */
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            system_event_sta_disconnected_t *disconn = &event->event_info.disconnected;
        	is_inf_up = false;
            switch (disconn->reason) {
                case WIFI_REASON_AUTH_FAIL:
                case WIFI_REASON_ASSOC_LEAVE:
                    wlan_obj.disconnected = true;
                    mod_network_deregister_nic(&wlan_obj);
                    break;
                default:
                    // let other errors through and try to reconnect.
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

        case SYSTEM_EVENT_AP_START:                 /**< ESP32 soft-AP start */
            mod_wlan_ap_number_of_connections = 0;
            wlan_obj.soft_ap_stopped = false;
            break;
        case SYSTEM_EVENT_AP_STOP:                  /**< ESP32 soft-AP stop */
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            wlan_obj.soft_ap_stopped = true;
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:          /**< a station connected to ESP32 soft-AP */
            mod_wlan_ap_number_of_connections++;
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:       /**< a station disconnected from ESP32 soft-AP */
            mod_wlan_ap_number_of_connections--;
            if(mod_wlan_ap_number_of_connections == 0) {
                xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            }
            break;
        case SYSTEM_EVENT_WIFI_READY:                /**< ESP32 WiFi ready */
        case SYSTEM_EVENT_SCAN_DONE:                /**< ESP32 finish scanning AP */
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:      /**< the auth mode of AP connected by ESP32 station changed */
        case SYSTEM_EVENT_STA_LOST_IP:              /**< ESP32 station lost IP and the IP is reset to 0 */
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:       /**< ESP32 station wps succeeds in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:        /**< ESP32 station wps fails in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:       /**< ESP32 station wps timeout in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_PIN:           /**< ESP32 station wps pin code in enrollee mode */

        case SYSTEM_EVENT_AP_STAIPASSIGNED:         /**< ESP32 soft-AP assign an IP to a connected station */
        case SYSTEM_EVENT_AP_PROBEREQRECVED:        /**< Receive probe request packet in soft-AP interface */
        case SYSTEM_EVENT_GOT_IP6:                  /**< ESP32 station or ap or ethernet interface v6IP addr is preferred */
        case SYSTEM_EVENT_ETH_START:                /**< ESP32 ethernet start */
        case SYSTEM_EVENT_ETH_STOP:                 /**< ESP32 ethernet stop */
        case SYSTEM_EVENT_ETH_CONNECTED:            /**< ESP32 ethernet phy link up */
        case SYSTEM_EVENT_ETH_DISCONNECTED:         /**< ESP32 ethernet phy link down */
        case SYSTEM_EVENT_ETH_GOT_IP:               /**< ESP32 ethernet got IP from connected AP */

                /*  TODO: some of These events will be a good candidate for Diagnostics log send to Pybytes*/

            break;
        default:
            break;
    }
    return ESP_OK;
}

STATIC void wlan_timer_callback( TimerHandle_t xTimer )
{
    if(xTimer == wlan_conn_timeout_timer)
    {
        //if still disconnected
        if (wlan_obj.disconnected) {
            /* Terminate connection */
            esp_wifi_disconnect();
            wlan_obj.sta_conn_timeout = true;
        }
        /* Stop Timer*/
        wlan_stop_sta_conn_timer();
    }
    else if (xTimer == wlan_smartConfig_timeout)
    {
        /* Stop Timer*/
        wlan_stop_smartConfig_timer();
        //set event flag
        wlan_obj.events |= MOD_WLAN_SMART_CONFIG_TIMEOUT;
        // trigger interrupt
        if(wlan_obj.trigger & MOD_WLAN_SMART_CONFIG_TIMEOUT)
        {
            mp_irq_queue_interrupt(wlan_callback_handler, &wlan_obj);
        }
        //stop smart config
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_STOP_BIT);
    }
    else
    {
        //Nothing
    }
}
STATIC void wlan_stop_sta_conn_timer()
{
    xSemaphoreTake(timeout_mutex, portMAX_DELAY);
    /* Clear Connection timeout counter if applicable*/
    if (wlan_conn_timeout_timer != NULL) {
        xTimerStop(wlan_conn_timeout_timer, 0);
        xTimerDelete(wlan_conn_timeout_timer, 0);
        wlan_conn_timeout_timer = NULL;
    }
    xSemaphoreGive(timeout_mutex);
}
STATIC void wlan_stop_smartConfig_timer()
{
    xSemaphoreTake(smartConfigTimeout_mutex, portMAX_DELAY);
    /* Clear Connection timeout counter if applicable*/
    if (wlan_smartConfig_timeout != NULL) {
        xTimerStop(wlan_smartConfig_timeout, 0);
        xTimerDelete(wlan_smartConfig_timeout, 0);
        wlan_smartConfig_timeout = NULL;
    }
    xSemaphoreGive(smartConfigTimeout_mutex);
}

// Must be called only when GIL is not locked
STATIC void wlan_setup_ap (const char *ssid, uint32_t auth, const char *key, uint32_t channel, bool add_mac, bool hidden) {
    uint32_t ssid_len = wlan_set_ssid_internal (ssid, strlen(ssid), add_mac);
    wlan_set_security_internal(auth, key);

    // get the current config and then change it
    wifi_config_t config;
    esp_wifi_get_config(WIFI_IF_AP, &config);
    strcpy((char *)config.ap.ssid, (char *)wlan_obj.ssid);
    config.ap.ssid_len = ssid_len;
    config.ap.authmode = wlan_obj.auth;
    strcpy((char *)config.ap.password, (char *)wlan_obj.key);
    config.ap.channel = channel;
    wlan_obj.channel = channel;
    config.ap.max_connection = MAX_AP_CONNECTED_STA;
    config.ap.ssid_hidden = (uint8_t)hidden;
    esp_wifi_set_config(WIFI_IF_AP, &config);
    //get mac of AP
    esp_wifi_get_mac(WIFI_IF_AP, wlan_obj.mac_ap);

    // Need to take back the GIL as mod_network_register_nic() uses MicroPython API and wlan_setup_ap() is called when GIL is not locked
    MP_THREAD_GIL_ENTER();
    mod_network_register_nic(&wlan_obj);
    MP_THREAD_GIL_EXIT();
}

STATIC void wlan_validate_mode (uint mode) {
    if (mode < WIFI_MODE_STA || mode > WIFI_MODE_APSTA) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_mode (uint mode) {
    wlan_obj.mode = mode;
    esp_wifi_set_mode(mode);
    wifi_ps_type_t wifi_ps_type;
    if (mode != WIFI_MODE_STA || wlan_obj.pwrsave == false) {
        wifi_ps_type = WIFI_PS_NONE;
    } else {
        wifi_ps_type = WIFI_PS_MIN_MODEM;
    }
    // set the power saving mode
    esp_wifi_set_ps(wifi_ps_type);
}

STATIC void wlan_validate_bandwidth (wifi_bandwidth_t bandwidth) {
    if (bandwidth < WIFI_BW_HT20 || bandwidth > WIFI_BW_HT40) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC bool wlan_set_bandwidth (wifi_bandwidth_t bandwidth)
{
    if (wlan_obj.mode == WIFI_MODE_STA)
    {
        if(esp_wifi_set_bandwidth(WIFI_IF_STA, bandwidth) != ESP_OK)
            return false;
    } else
    {
        if(esp_wifi_set_bandwidth(WIFI_IF_AP, bandwidth) != ESP_OK)
            return false;
    }
    wlan_obj.bandwidth = bandwidth;
    return true;
}

STATIC void wlan_validate_hostname (const char *hostname) {
    //dont set hostname it if is null, so its a valid hostname
    if (hostname == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    uint8_t len = strlen(hostname);
    if(len == 0 || len > TCPIP_HOSTNAME_MAX_SIZE){
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC bool wlan_set_hostname (const char *hostname) {
    if (wlan_obj.mode == WIFI_MODE_STA)
    {
        if(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname) != ESP_OK)
            return false;
    } else
    {
        if(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname) != ESP_OK)
            return false;
    }

    strlcpy((char*)wlan_obj.hostname, hostname, TCPIP_HOSTNAME_MAX_SIZE);
    return true;
}

STATIC esp_err_t wlan_update_hostname () {
    char* hostname = NULL;
    tcpip_adapter_if_t interface = wlan_obj.mode == WIFI_MODE_STA ? TCPIP_ADAPTER_IF_STA : TCPIP_ADAPTER_IF_AP;

    esp_err_t err = tcpip_adapter_get_hostname(interface, (const char**)&hostname);
    if(err != ESP_OK)
    {
        return err;
    }

    if(hostname != NULL)
    {
        strlcpy((char*)wlan_obj.hostname, hostname, TCPIP_HOSTNAME_MAX_SIZE);
    }
    return err;
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
    if (auth < WIFI_AUTH_WEP && auth > WIFI_AUTH_WPA2_ENTERPRISE) {
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

STATIC void wlan_set_security_internal (uint8_t auth, const char *key) {
    wlan_obj.auth = auth;
//    uint8_t wep_key[32];
    if (key != NULL) {
        strcpy((char *)wlan_obj.key, key);
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
    if (channel < 1 || channel > MAX_WIFI_CHANNELS) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC void wlan_set_antenna (uint8_t antenna) {
    wlan_obj.antenna = antenna;
    antenna_select(antenna);
}

STATIC void wlan_validate_certificates (wlan_wpa2_ent_obj_t *wpa2_ent) {
    if ((wpa2_ent->client_key_path == NULL || wpa2_ent->client_cert_path == NULL) && wpa2_ent->username == NULL) {
        goto cred_error;
    } else if (wpa2_ent->client_key_path != NULL && wpa2_ent->client_cert_path != NULL && wpa2_ent->username != NULL) {
        goto cred_error;
    }

    if (wpa2_ent->identity == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "indentiy required for WPA2_ENT authentication"));
    } else if (strlen(wpa2_ent->identity) > 127) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid identity length %d", strlen(wpa2_ent->identity)));
    }

    return;

cred_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid WAP2_ENT credentials"));
}

static void wlan_validate_country(const char * country)
{
    uint8_t str_len = strlen(country);
    if(str_len > 2)
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Wrong Country string length!"));
    }
}

static void wlan_validate_country_policy(uint8_t policy)
{
    if(policy > (uint8_t)WIFI_COUNTRY_POLICY_MANUAL)
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Country Policy!"));
    }
}

STATIC void wlan_do_connect (const char* ssid, const char* bssid, const wifi_auth_mode_t auth, const char* key,
                             int32_t timeout, const wlan_wpa2_ent_obj_t * const wpa2_ent, const char* hostname, uint8_t channel) {

    esp_wpa2_config_t wpa2_config = WPA2_CONFIG_INIT_DEFAULT();
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    // first close any active connections
    esp_wifi_disconnect();

    strcpy((char *)wifi_config.sta.ssid, ssid);

    if (key) {
        strcpy((char *)wifi_config.sta.password, key);
        strcpy((char *)wlan_obj.key, key);
    }
    if (bssid) {
        memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
        wifi_config.sta.bssid_set = true;
    }

    wifi_config.sta.channel = channel;
    if(channel > 0)
    {
        wifi_config.sta.sort_method = WIFI_FAST_SCAN;
    }
    else
    {
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    }

    if (ESP_OK != esp_wifi_set_config(WIFI_IF_STA, &wifi_config)) {
        goto os_error;
    }

    // The certificate files are already read at this point because this function runs outside of GIL, and the file_read functions uses MicroPython APIs
    if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
        // CA Certificate is not mandatory
        if (wpa2_ent->ca_certs_path != NULL) {
            if (wlan_obj.vstr_ca.buf != NULL) {
                if (ESP_OK != esp_wifi_sta_wpa2_ent_set_ca_cert((unsigned char*)wlan_obj.vstr_ca.buf, (int)wlan_obj.vstr_ca.len)) {
                    goto os_error;
                }
            } else {
                goto invalid_file;
            }
        }

        // client certificate is necessary only in EAP-TLS method, this is ensured by wlan_validate_certificates() function
        if (wpa2_ent->client_key_path != NULL && wpa2_ent->client_cert_path != NULL) {
            if ((wlan_obj.vstr_cert.buf != NULL) && (wlan_obj.vstr_key.buf != NULL)) {
                if (ESP_OK != esp_wifi_sta_wpa2_ent_set_cert_key((unsigned char*)wlan_obj.vstr_cert.buf, (int)wlan_obj.vstr_cert.len,
                                                                 (unsigned char*)wlan_obj.vstr_key.buf, (int)wlan_obj.vstr_key.len, NULL, 0)) {
                    goto os_error;
                }
            } else {
                goto invalid_file;
            }
        }

        if (ESP_OK != esp_wifi_sta_wpa2_ent_set_identity((unsigned char *)wpa2_ent->identity, strlen(wpa2_ent->identity))) {
            goto os_error;
        }

        if (wpa2_ent->username != NULL || key != NULL) {
            if (ESP_OK != esp_wifi_sta_wpa2_ent_set_username((unsigned char *)wpa2_ent->username, strlen(wpa2_ent->username))) {
                goto os_error;
            }

            if (ESP_OK != esp_wifi_sta_wpa2_ent_set_password((unsigned char *)key, strlen(key))) {
                goto os_error;
            }
        }

        if (ESP_OK != esp_wifi_sta_wpa2_ent_enable(&wpa2_config)) {
            goto os_error;
        }
    }

    if(hostname != NULL)
    {
        wlan_set_hostname(hostname);
    }
    else
    {
        wlan_update_hostname();
    }

    if (ESP_OK != esp_wifi_connect()) {
        goto os_error;
    }

    wlan_obj.auth = auth;
    memcpy(&wlan_obj.wpa2_ent, wpa2_ent, sizeof(wlan_wpa2_ent_obj_t));

    if(timeout > 0)
    {
        wlan_conn_recover_timeout = timeout;
        /*create Timer */
        wlan_conn_timeout_timer = xTimerCreate("Wlan_Timer", timeout / portTICK_PERIOD_MS, 0, 0, wlan_timer_callback);
        /* reset timeout Flag */
        wlan_obj.sta_conn_timeout = false;
        /*start Timer */
        xTimerStart(wlan_conn_timeout_timer, 0);
    }

    return;

invalid_file:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid file path"));

os_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}


static void wlan_init_wlan_recover_params(void)
{
    wlan_conn_recover_auth = WIFI_AUTH_MAX;
    wlan_conn_recover_timeout = -1;
    wlan_conn_recover_scan_channel = 0;
    wlan_conn_recover_hidden = false;
}

STATIC void wlan_callback_handler(void* arg)
{
    wlan_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {

        mp_call_function_1(self->handler, self->handler_arg);
    }
}

STATIC void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type)
{

    wifi_pkt_rx_ctrl_t* wlan_ctrl_pkt = (wifi_pkt_rx_ctrl_t*)buf;

    bool trigger = false;
    static uint8_t old_token = 0xFF;

    switch (type)
    {
    case WIFI_PKT_MGMT:
        wlan_obj.events |= MOD_WLAN_TRIGGER_PKT_MGMT;
        if(((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_MGMT)
            || (((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_ANY) == MOD_WLAN_TRIGGER_PKT_ANY))
        {
            trigger = true;
        }
        break;
    case WIFI_PKT_CTRL:
        wlan_obj.events |= MOD_WLAN_TRIGGER_PKT_CTRL;
        if(((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_CTRL)
            || (((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_ANY) == MOD_WLAN_TRIGGER_PKT_ANY))
        {
            trigger = true;
        }
        break;
    case WIFI_PKT_DATA:
        wlan_obj.events |= MOD_WLAN_TRIGGER_PKT_DATA;
        if(((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_DATA)
            || (((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_ANY) == MOD_WLAN_TRIGGER_PKT_ANY))
        {
            trigger = true;
        }
        break;
    case WIFI_PKT_MISC:
        wlan_obj.events |= MOD_WLAN_TRIGGER_PKT_MISC;
        if(((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_MISC)
            || (((wlan_obj.trigger) & MOD_WLAN_TRIGGER_PKT_ANY) == MOD_WLAN_TRIGGER_PKT_ANY))
        {
            trigger = true;
        }
        break;
    default:
        break;
    }

    if (trigger && (token != old_token))
    {
        xSemaphoreTake(wlan_obj.mutex, portMAX_DELAY);

        wlan_prom_packet[token].rx_ctrl.rssi = wlan_ctrl_pkt->rssi;
        wlan_prom_packet[token].rx_ctrl.rate = wlan_ctrl_pkt->rate;
        wlan_prom_packet[token].rx_ctrl.sig_mode = wlan_ctrl_pkt->sig_mode;
        wlan_prom_packet[token].rx_ctrl.mcs = wlan_ctrl_pkt->mcs;
        wlan_prom_packet[token].rx_ctrl.cwb = wlan_ctrl_pkt->cwb;
        wlan_prom_packet[token].rx_ctrl.aggregation = wlan_ctrl_pkt->aggregation;
        wlan_prom_packet[token].rx_ctrl.stbc = wlan_ctrl_pkt->stbc;
        wlan_prom_packet[token].rx_ctrl.fec_coding = wlan_ctrl_pkt->fec_coding;
        wlan_prom_packet[token].rx_ctrl.sgi = wlan_ctrl_pkt->sgi;
        wlan_prom_packet[token].rx_ctrl.noise_floor = wlan_ctrl_pkt->noise_floor;
        wlan_prom_packet[token].rx_ctrl.ampdu_cnt = wlan_ctrl_pkt->ampdu_cnt;
        wlan_prom_packet[token].rx_ctrl.channel = wlan_ctrl_pkt->channel;
        wlan_prom_packet[token].rx_ctrl.secondary_channel = wlan_ctrl_pkt->secondary_channel;
        wlan_prom_packet[token].rx_ctrl.timestamp = wlan_ctrl_pkt->timestamp;
        wlan_prom_packet[token].rx_ctrl.ant = wlan_ctrl_pkt->ant;
        wlan_prom_packet[token].rx_ctrl.sig_len = wlan_ctrl_pkt->sig_len;
        wlan_prom_packet[token].rx_ctrl.rx_state = wlan_ctrl_pkt->rx_state;

        wlan_prom_packet[token].pkt_type = type;


        if(type != WIFI_PKT_CTRL)
        {
            memcpy(&(wlan_prom_packet[token].data[0]), &(((wifi_promiscuous_pkt_t*)buf)->payload), wlan_prom_packet[token].rx_ctrl.sig_len);
        }

        xSemaphoreGive(wlan_obj.mutex);

        old_token = token;

        mp_irq_queue_interrupt(wlan_callback_handler, &wlan_obj);

    }
}

STATIC void wlan_set_default_inf(void)
{
#if defined(FIPY) || defined(GPY)
    if (wlan_obj.mode == WIFI_MODE_STA || wlan_obj.mode == WIFI_MODE_APSTA) {
        tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &wlan_sta_inf_dns_info);
        tcpip_adapter_up(TCPIP_ADAPTER_IF_STA);
    }
#endif
}

//STATIC void wlan_get_sl_mac (void) {
//    // Get the MAC address
////    uint8_t macAddrLen = SL_MAC_ADDR_LEN;
////    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macAddrLen, wlan_obj.mac);
//}
static void TASK_SMART_CONFIG (void *pvParameters) {

    EventBits_t uxBits;
    bool connected;
    static uint32_t thread_notification;

smartConf_init:
    connected = false;
    // Block task till notification is received
    thread_notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (thread_notification) {
        if (wlan_obj.started == false) {
            CHECK_ESP_ERR(esp_wifi_start(), smartConf_init)
            wlan_obj.started = true;
        }
        else
        {
            // disconnect any AP connected
            CHECK_ESP_ERR(esp_wifi_disconnect(), smartConf_init)
        }
        CHECK_ESP_ERR(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH), smartConf_init)
        CHECK_ESP_ERR(esp_smartconfig_start(smart_config_callback), smartConf_init)
        goto smartConf_start;
    }
    goto smartConf_init;

smartConf_start:
    wlan_smart_config_enabled = true;
    /*create Timer */
    wlan_smartConfig_timeout = xTimerCreate("smartConfig_Timer", 60000 / portTICK_PERIOD_MS, 0, 0, wlan_timer_callback);
    /*start Timer */
    xTimerStart(wlan_smartConfig_timeout, 0);
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT | ESPTOUCH_STOP_BIT | ESPTOUCH_SLEEP_STOP_BIT, true, false, portMAX_DELAY);
        if(uxBits & ESPTOUCH_STOP_BIT) {
            wlan_smart_config_enabled = false;
            esp_smartconfig_stop();
            //mp_printf(&mp_plat_print, "\nSmart Config Aborted or Timed-out\n");
            goto smartConf_init;
        }
        if(uxBits & ESPTOUCH_SLEEP_STOP_BIT) {
            esp_smartconfig_stop();
            //mp_printf(&mp_plat_print, "\nSmart Config Aborted because sleep operation has been requested\n");
            goto smartConf_init;
        }
        if(uxBits & CONNECTED_BIT) {
            //mp_printf(&mp_plat_print, "WiFi Connected to ap\n");
            connected = true;
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            //mp_printf(&mp_plat_print, "smartconfig over\n");
            wlan_smart_config_enabled = false;
            esp_smartconfig_stop();
            wlan_stop_smartConfig_timer();
            //set event flag
            wlan_obj.events |= MOD_WLAN_SMART_CONFIG_DONE;
            // trigger interrupt
            if(wlan_obj.trigger & MOD_WLAN_SMART_CONFIG_DONE)
            {
                mp_irq_queue_interrupt(wlan_callback_handler, &wlan_obj);
            }
            if (connected) {
                //save wifi credentials
                config_set_sta_wifi_ssid(wlan_obj.ssid_o, false);
                config_set_wifi_auth(wlan_obj.auth, false);
                switch(wlan_obj.mode)
                    {
                    case WIFI_MODE_STA:
                        config_set_wifi_mode(PYCOM_WIFI_CONF_MODE_STA, false);
                        break;
                    case WIFI_MODE_AP:
                        config_set_wifi_mode(PYCOM_WIFI_CONF_MODE_STA, false);
                        break;
                    case WIFI_MODE_APSTA:
                        config_set_wifi_mode(PYCOM_WIFI_CONF_MODE_STA, false);
                        break;
                    default:
                        break;
                    }
                config_set_wifi_sta_pwd(wlan_obj.key, true);
                //set Connected bit back as it has been consumed.
                xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            }
            goto smartConf_init;
        }
    }

}

static void smart_config_callback(smartconfig_status_t status, void *pdata)
{
    wifi_config_t *wifi_config;

    switch (status) {
        case SC_STATUS_WAIT:
            //mp_printf(&mp_plat_print, "SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            //mp_printf(&mp_plat_print, "SC_STATUS_FINDING_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            //mp_printf(&mp_plat_print, "SC_STATUS_GETTING_SSID_PSWD\n");
            break;
        case SC_STATUS_LINK:
            //mp_printf(&mp_plat_print, "SC_STATUS_LINK\n");
            wifi_config = pdata;
            //save password/ssid/auth
            memcpy(wlan_obj.key, wifi_config->sta.password, 64);
            memcpy(wlan_obj.ssid, wifi_config->sta.ssid, (MODWLAN_SSID_LEN_MAX));
            wlan_obj.auth = wifi_config->sta.threshold.authmode;
            esp_wifi_disconnect();
            esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config);
            esp_wifi_connect();
            break;
        case SC_STATUS_LINK_OVER:
            //mp_printf(&mp_plat_print, "SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                //mp_printf(&mp_plat_print, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

/******************************************************************************/
// Micro Python bindings; WLAN class

/// \class WLAN - WiFi driver

STATIC mp_obj_t wlan_init_helper(wlan_obj_t *self, const mp_arg_val_t *args) {
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
    uint8_t antenna;
    if (args[4].u_obj == MP_OBJ_NULL) {
        // first gen module, so select the internal antenna
        if (micropy_hw_antenna_diversity_pin_num == MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM) {
            antenna = ANTENNA_TYPE_INTERNAL;
        } else {
            antenna = ANTENNA_TYPE_MANUAL;
        }
    } else if (args[4].u_obj == mp_const_none) {
        antenna = ANTENNA_TYPE_MANUAL;
    } else {
        antenna = mp_obj_get_int(args[4].u_obj);
    }
    antenna_validate_antenna(antenna);

    wlan_obj.pwrsave = args[5].u_bool;
    bool hidden = args[6].u_bool;

    if (mode != WIFI_MODE_STA) {
        if (ssid == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "AP SSID not given"));
        }
        if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "WPA2_ENT not supported in AP mode"));
        }
    }

    int32_t bandwidth = args[7].u_int;
    wlan_validate_bandwidth(bandwidth);

    // get max_tx_power
    int8_t max_tx_pwr = args[8].u_int;

    //get the Country
    wifi_country_t* ptrcountry_info = NULL;
    wifi_country_t country_info;

    if(args[9].u_obj != mp_const_none)
    {
        if(!MP_OBJ_IS_TYPE(args[9].u_obj, &mp_type_tuple))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
        }
        size_t tuple_len = 4;
        mp_obj_t *tuple = NULL;
        mp_obj_tuple_get(args[9].u_obj, &tuple_len, &tuple);

        const char* country = mp_obj_str_get_str(tuple[0]);
        uint8_t start_chn = mp_obj_get_int(tuple[1]);
        uint8_t num_chn = mp_obj_get_int(tuple[2]);
        wifi_country_policy_t policy =  mp_obj_get_int(tuple[3]);

        //validate data
        wlan_validate_country(country);
        wlan_validate_channel(start_chn);
        wlan_validate_channel(num_chn);
        wlan_validate_country_policy(policy);

        memcpy(&(country_info.cc[0]), country, strlen(country));
        country_info.schan = start_chn;
        country_info.nchan = num_chn;
        country_info.policy = policy;

        ptrcountry_info = &country_info;
    }

    wlan_conn_recover_hidden = hidden;

    wlan_internal_setup_t setup = {
            mode,
            (const char *)ssid,
            (const char *)key,
            (const char *)ssid,
            (const char *)key,
            auth,
            channel,
            antenna,
            false,
            hidden,
            bandwidth,
            ptrcountry_info,
            &max_tx_pwr
    };

    // initialize the wlan subsystem
    wlan_setup(&setup);

    return mp_const_none;
}

STATIC const mp_arg_t wlan_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_MODE_STA} },
    { MP_QSTR_ssid,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_auth,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_channel,      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_antenna,      MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_power_save,   MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
    { MP_QSTR_hidden,       MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
    { MP_QSTR_bandwidth,    MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = WIFI_BW_HT40} },
    { MP_QSTR_max_tx_pwr,   MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_country,        MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
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

    if (n_kw > 0 || !wlan_obj.started) {
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

mp_obj_t wlan_deinit(mp_obj_t self_in) {

    if (wlan_obj.started)
    {
        bool called_from_sleep = false;

        // stop smart config if enabled
        if(wlan_smart_config_enabled) {
            // If the input parameter is not the object itself but a simple boolean, it is interpreted that
            // wlan_deinit is called from machine_sleep()
            if(mp_obj_get_type(self_in) == &mp_type_bool) {
                called_from_sleep = (bool)mp_obj_get_int(self_in);
                if(called_from_sleep == true) {
                    // stop smart config with special event
                    xEventGroupSetBits(wifi_event_group, ESPTOUCH_SLEEP_STOP_BIT);
                    vTaskDelay(100/portTICK_PERIOD_MS);
                }
            }
            else {
                // stop smart config with original STOP event
                xEventGroupSetBits(wifi_event_group, ESPTOUCH_STOP_BIT);
                vTaskDelay(100/portTICK_PERIOD_MS);
            }
        }

        mod_network_deregister_nic(&wlan_obj);
        esp_wifi_stop();

        /* wait for sta and Soft-AP to stop */
        while(!wlan_obj.sta_stopped || !wlan_obj.soft_ap_stopped)
        {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        /* stop and free wifi resource */
        esp_wifi_deinit();
        // Only free up memory area of country information if this deinit is not called from machine.sleep()
        if(called_from_sleep == false) {
            free(wlan_obj.country);
            wlan_obj.country = NULL;
        }
        wlan_obj.started = false;
        mod_network_deregister_nic(&wlan_obj);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_deinit_obj, wlan_deinit);

STATIC mp_obj_t wlan_scan(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const qstr wlan_scan_info_fields[] = {
        MP_QSTR_ssid, MP_QSTR_bssid, MP_QSTR_sec, MP_QSTR_channel, MP_QSTR_rssi
    };

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssid,                 MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bssid,                MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_channel,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_show_hidden,          MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_type,                 MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_scantime,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    /* Scan Config */
    wifi_scan_config_t scan_config =
    {
        NULL,
        NULL,
        0,
        false,
        WIFI_SCAN_TYPE_ACTIVE,
    };
    wifi_scan_config_t * ptr_config;
    mp_buffer_info_t bufinfo;
    mp_obj_t *stime;
    size_t stimelen;


    if(args[0].u_obj != mp_const_none)
    {
        scan_config.ssid = (uint8_t*)mp_obj_str_get_str(args[0].u_obj);
    }

    if(args[1].u_obj != mp_const_none)
    {
        if(MP_OBJ_IS_TYPE(args[1].u_obj, &mp_type_bytes))
        {
            mp_get_buffer_raise(args[1].u_obj, &bufinfo, MP_BUFFER_READ);
            scan_config.bssid = bufinfo.buf;
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid bssid"));
        }
    }

    if(args[2].u_obj != mp_const_none)
    {
        scan_config.channel = mp_obj_get_int(args[2].u_obj);
    }

    if(args[3].u_obj != mp_const_none)
    {
        scan_config.show_hidden = mp_obj_get_int(args[3].u_obj) >= 1;
    }

    if(args[4].u_obj != mp_const_none)
    {
        if((wifi_scan_type_t)(mp_obj_get_int(args[4].u_obj)) == WIFI_SCAN_TYPE_PASSIVE)
        {
            scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        }
        else
        {
            scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        }
    }
    else if(wlan_obj.mode == WIFI_MODE_STA && !wlan_obj.disconnected)
    {
        scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    }
    else
    {
        // Nothing
    }

    if(args[5].u_obj != mp_const_none)
    {
        if(MP_OBJ_IS_TYPE(args[5].u_obj, &mp_type_tuple) && scan_config.scan_type == WIFI_SCAN_TYPE_ACTIVE)
        {
            mp_obj_get_array(args[5].u_obj, &stimelen, &stime);

            if(stimelen != 0 && stimelen <= 2)
            {
                if(stimelen == 2 && scan_config.scan_type == WIFI_SCAN_TYPE_ACTIVE)
                {
                    scan_config.scan_time.active.min = mp_obj_get_int(stime[0]);
                    scan_config.scan_time.active.max = mp_obj_get_int(stime[1]);
                }
                else
                {
                    goto scan_time_err;
                }
            }
            else
            {
                goto scan_time_err;
            }
        }
        else if(MP_OBJ_IS_INT(args[5].u_obj) && scan_config.scan_type == WIFI_SCAN_TYPE_PASSIVE)
        {
            uint32_t passive = mp_obj_get_int(args[5].u_obj);

            if(passive >= 0)
            {
                scan_config.scan_time.passive = mp_obj_get_int(args[5].u_obj);
            }
            else
            {
                goto scan_time_err;
            }
        }
        else
        {
            goto scan_time_err;
        }
    }

    ptr_config = &scan_config;

    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    MP_THREAD_GIL_EXIT();
    esp_err_t err = esp_wifi_scan_start(ptr_config, true);
    MP_THREAD_GIL_ENTER();

    switch(err)
    {
    case ESP_OK:
        /* Success */
        break;
    case ESP_ERR_WIFI_TIMEOUT:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Scan operation timed out!"));
        break;
    default:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Scan operation Failed!"));
        break;
    }

    uint16_t ap_num;
    wifi_ap_record_t *ap_record_buffer;
    wifi_ap_record_t *ap_record;
    mp_obj_t nets = mp_obj_new_list(0, NULL);

    esp_wifi_scan_get_ap_num(&ap_num); // get the number of scanned APs

    if (ap_num > 0) {
        ap_record_buffer = malloc(ap_num * sizeof(wifi_ap_record_t));
        if (ap_record_buffer == NULL) {
            mp_raise_OSError(MP_ENOMEM);
        }

        // get the scanned AP list
        if (ESP_OK == esp_wifi_scan_get_ap_records(&ap_num, (wifi_ap_record_t *)ap_record_buffer)) {
            for (int i = 0; i < ap_num; i++) {
                ap_record = &ap_record_buffer[i];
                mp_obj_t tuple[5];
                tuple[0] = mp_obj_new_str((const char *)ap_record->ssid, strlen((char *)ap_record->ssid));
                tuple[1] = mp_obj_new_bytes((const byte *)ap_record->bssid, sizeof(ap_record->bssid));
                tuple[2] = mp_obj_new_int(ap_record->authmode);
                tuple[3] = mp_obj_new_int(ap_record->primary);
                tuple[4] = mp_obj_new_int(ap_record->rssi);

                // add the network to the list
                mp_obj_list_append(nets, mp_obj_new_attrtuple(wlan_scan_info_fields, 5, tuple));
            }
        }
        free(ap_record_buffer);
    }

    return nets;

scan_time_err:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid scan timings"));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_scan_obj, 1, wlan_scan);

STATIC mp_obj_t wlan_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_ssid,                 MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_auth,                                   MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bssid,                MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_ca_certs,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_keyfile,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_certfile,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_identity,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_hostname,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_channel,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the ssid
    const char *ssid = mp_obj_str_get_str(args[0].u_obj);
    wlan_validate_ssid_len(strlen(ssid));

    wlan_wpa2_ent.ca_certs_path = NULL;
    wlan_wpa2_ent.client_cert_path = NULL;
    wlan_wpa2_ent.client_key_path = NULL;
    wlan_wpa2_ent.identity = NULL;
    wlan_wpa2_ent.username = NULL;

    // get the auth
    const char *key = NULL;
    const char *user = NULL;
    uint8_t auth = WIFI_AUTH_MAX;
    if (args[1].u_obj != mp_const_none) {
        mp_obj_t *sec;
        uint32_t a_len;
        mp_obj_get_array(args[1].u_obj, &a_len, &sec);
        if (a_len == 1) {
            auth = mp_obj_get_int(sec[0]);
            if (auth != WIFI_AUTH_WPA2_ENTERPRISE) {
                goto auth_error;
            }
        } else if (a_len == 2) {
            if (sec[0] != mp_const_none) {
                auth = mp_obj_get_int(sec[0]);
                if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
                    goto auth_error;
                }
            }
            key = mp_obj_str_get_str(sec[1]);
        } else if (a_len == 3) {
            auth = mp_obj_get_int(sec[0]);
            if (auth != WIFI_AUTH_WPA2_ENTERPRISE) {
                goto auth_error;
            }
            if (sec[1] != mp_const_none) {
                user = mp_obj_str_get_str(sec[1]);
            }
            if (sec[2] != mp_const_none) {
                key = mp_obj_str_get_str(sec[2]);
            }
        } else {
            goto auth_error;
        }
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

    // get the ca_certificate
    if (args[4].u_obj != mp_const_none) {
        wlan_wpa2_ent.ca_certs_path = mp_obj_str_get_str(args[4].u_obj);
    }

    // get the private key
    if (args[5].u_obj != mp_const_none) {
        wlan_wpa2_ent.client_key_path = mp_obj_str_get_str(args[5].u_obj);
    }

    // get the client certificate
    if (args[6].u_obj != mp_const_none) {
        wlan_wpa2_ent.client_cert_path = mp_obj_str_get_str(args[6].u_obj);
    }

    // get the identity
    if (args[7].u_obj != mp_const_none) {
        wlan_wpa2_ent.identity = mp_obj_str_get_str(args[7].u_obj);
    }

    const char *hostname = NULL;
    if (args[8].u_obj != mp_const_none) {
        hostname = mp_obj_str_get_str(args[8].u_obj);
        wlan_validate_hostname(hostname);
    }

    if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
        wlan_wpa2_ent.username = user;
        wlan_validate_certificates(&wlan_wpa2_ent);
    }

    uint8_t channel = 0;
    if(args[9].u_obj != mp_const_none)
    {
        channel = mp_obj_get_int(args[9].u_obj);
        wlan_conn_recover_scan_channel = channel;
    }

    wlan_conn_recover_auth = auth;
    wlan_conn_recover_timeout = timeout;

    // stop smart config if enabled
    if(wlan_smart_config_enabled)
    {
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_STOP_BIT);
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
    // connect to the requested access point
    wlan_do_connect (ssid, bssid, auth, key, timeout, &wlan_wpa2_ent, hostname, channel);

    return mp_const_none;

auth_error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid authentication tuple"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_connect_obj, 1, wlan_connect);

STATIC mp_obj_t wlan_disconnect(mp_obj_t self_in) {
    // check for the correct wlan mode
    if (wlan_obj.mode == WIFI_MODE_AP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    esp_wifi_disconnect();
    /* Stop Conn timeout counter*/
    wlan_stop_sta_conn_timer();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_disconnect_obj, wlan_disconnect);

STATIC mp_obj_t wlan_isconnected(mp_obj_t self_in) {
    if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
        return mp_const_true;
    }

    if(wlan_obj.sta_conn_timeout)
    {
        wlan_obj.sta_conn_timeout = false;
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "Connection to AP Timeout!"));
    }

    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_isconnected_obj, wlan_isconnected);

STATIC mp_obj_t wlan_smartConfig(mp_obj_t self_in) {

    //Notify task to start right away
    xTaskNotifyGive(SmartConfTaskHandle);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_smartConfig_obj, wlan_smartConfig);

STATIC mp_obj_t wlan_smartConfkey(mp_obj_t self_in) {

    wlan_obj_t* self = (wlan_obj_t*)self_in;
    if((xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT))
    {
        return mp_obj_new_str((char *)self->key, strlen((char *)self->key));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_smartConfkey_obj, wlan_smartConfkey);

STATIC mp_obj_t wlan_ifconfig (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t wlan_ifconfig_args[] = {
        { MP_QSTR_id,               MP_ARG_INT,     {.u_int = 0} },
        { MP_QSTR_config,           MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_ifconfig_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), wlan_ifconfig_args, args);

    // check the interface id
    tcpip_adapter_if_t adapter_if;
    if (args[0].u_int > 1) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    } else if (args[0].u_int == 0) {
        adapter_if = TCPIP_ADAPTER_IF_STA;
    } else {
        adapter_if = TCPIP_ADAPTER_IF_AP;
    }

    tcpip_adapter_dns_info_t dns_info;
    // get the configuration
    if (args[1].u_obj == MP_OBJ_NULL) {
        // get
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_get_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
        if (ESP_OK == tcpip_adapter_get_ip_info(adapter_if, &ip_info)) {
            mp_obj_t ifconfig[4] = {
                netutils_format_ipv4_addr((uint8_t *)&ip_info.ip.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&ip_info.gw.addr, NETUTILS_BIG),
                netutils_format_ipv4_addr((uint8_t *)&dns_info.ip, NETUTILS_BIG)
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

            tcpip_adapter_ip_info_t ip_info;
            netutils_parse_ipv4_addr(items[0], (uint8_t *)&ip_info.ip.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[1], (uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[2], (uint8_t *)&ip_info.gw.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[3], (uint8_t *)&dns_info.ip, NETUTILS_BIG);

            if (adapter_if == TCPIP_ADAPTER_IF_STA) {
                tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
                tcpip_adapter_set_ip_info(adapter_if, &ip_info);
                tcpip_adapter_set_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
            } else {
                tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
                tcpip_adapter_set_ip_info(adapter_if, &ip_info);
                tcpip_adapter_set_dns_info(adapter_if, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
                tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
            }
        } else {
            // check for the correct string
            const char *mode = mp_obj_str_get_str(args[1].u_obj);
            if (strcmp("dhcp", mode) && strcmp("auto", mode)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
            }

            if (ESP_OK != tcpip_adapter_dhcpc_start(adapter_if)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
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

STATIC mp_obj_t wlan_bandwidth (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->bandwidth);
    } else {
        uint bandwidth = mp_obj_get_int(args[1]);
        wlan_validate_bandwidth(bandwidth);
        return mp_obj_new_bool(wlan_set_bandwidth(bandwidth));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_bandwidth_obj, 1, 2, wlan_bandwidth);

STATIC mp_obj_t wlan_ssid (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        if (self->mode == WIFI_MODE_STA && !wlan_obj.disconnected)
        {
            return mp_obj_new_str((const char *)wlan_obj.ssid_o, strlen((const char *)wlan_obj.ssid_o));
        }
        else if(self->mode == WIFI_MODE_AP && !wlan_obj.soft_ap_stopped)
        {
            return mp_obj_new_str((const char *)wlan_obj.ssid, strlen((const char *)wlan_obj.ssid));
        }
        else if(self->mode == WIFI_MODE_APSTA)
        {
            mp_obj_t wlanssid[2];

            if(!wlan_obj.disconnected)
            {
                wlanssid[0] = mp_obj_new_str((const char *)wlan_obj.ssid_o, strlen((const char *)wlan_obj.ssid_o));
            }
            else
            {
                wlanssid[0] = mp_const_none;
            }

            if(!wlan_obj.soft_ap_stopped)
            {
                wlanssid[1] = mp_obj_new_str((const char *)wlan_obj.ssid, strlen((const char *)wlan_obj.ssid));
            }
            else
            {
                wlanssid[1] = mp_const_none;
            }

            return mp_obj_new_tuple(2, wlanssid);
        }
        else
        {
            return mp_const_none;
        }
    }
    else
    {
        const char *ssid = mp_obj_str_get_str(args[1]);
        wlan_validate_ssid_len(strlen(ssid));
        mp_uint_t ssid_len = wlan_set_ssid_internal (ssid, strlen(ssid), false);
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

STATIC mp_obj_t wlan_wifi_protocol (mp_uint_t n_args, const mp_obj_t *args)
{

    STATIC const qstr wlan_protocol_fields[] = {
        MP_QSTR_protocol_11B, MP_QSTR_protocol_11G, MP_QSTR_protocol_11N
    };

    wlan_obj_t *self = args[0];
    wifi_interface_t interface;
    uint8_t bitmap;

    switch(self->mode)
    {
    case WIFI_MODE_STA:
        interface = ESP_IF_WIFI_STA;
        break;
    case WIFI_MODE_AP:
        interface  = ESP_IF_WIFI_AP;
        break;
    default:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
        break;
    }

    if(n_args == 1)
    {

        if(ESP_OK != esp_wifi_get_protocol(interface, &bitmap))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }

        mp_obj_t tuple[3] =
        {
            mp_obj_new_bool(0),
            mp_obj_new_bool(0),
            mp_obj_new_bool(0)
        };

        if(bitmap & WIFI_PROTOCOL_11B)
        {
            tuple[0] = mp_obj_new_bool(1);
        }

        if(bitmap & WIFI_PROTOCOL_11G)
        {
            tuple[1] = mp_obj_new_bool(1);
        }

        if(bitmap & WIFI_PROTOCOL_11N)
        {
            tuple[2] = mp_obj_new_bool(1);
        }

        return mp_obj_new_attrtuple(wlan_protocol_fields, 3, tuple);
    }
    else
    {
        if(!MP_OBJ_IS_TYPE(args[1], &mp_type_tuple))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
        }

        size_t tuple_len = 3;
        mp_obj_t *tuple = NULL;
        mp_obj_tuple_get(args[1], &tuple_len, &tuple);

        bitmap = 0;

        if(mp_obj_is_true(tuple[0]))
        {
            bitmap |= WIFI_PROTOCOL_11B;
        }

        if(mp_obj_is_true(tuple[1]))
        {
            bitmap |= WIFI_PROTOCOL_11G;
        }

        if(mp_obj_is_true(tuple[2]))
        {
            bitmap |= WIFI_PROTOCOL_11N;
        }

        esp_err_t err = esp_wifi_set_protocol(interface, bitmap);

        if(ESP_OK != err)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }

        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_wifi_protocol_obj, 1, 2, wlan_wifi_protocol);

STATIC mp_obj_t wlan_ap_sta_list (mp_obj_t self_in) {
    STATIC const qstr wlan_sta_ifo_fields[] = {
        MP_QSTR_mac, MP_QSTR_rssi, MP_QSTR_wlan_protocol
    };
    uint8_t index;
    wifi_sta_list_t sta_list;
    wlan_obj_t * self = self_in;

    mp_obj_t sta_out_list = mp_obj_new_list(0, NULL);
    /* Check if AP mode is enabled */
    if (self->mode == WIFI_MODE_AP || self->mode == WIFI_MODE_APSTA) {
        /* Get STAs connected to AP*/
        esp_wifi_ap_get_sta_list(&sta_list);

        mp_obj_t tuple[3];
        for(index = 0; index < MAX_AP_CONNECTED_STA && index < sta_list.num; index++)
        {
            tuple[0] = mp_obj_new_bytes((const byte *)sta_list.sta[index].mac, 6);
            tuple[1] = MP_OBJ_NEW_SMALL_INT(sta_list.sta[index].rssi);
            if(sta_list.sta[index].phy_11b)
            {
                tuple[2] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_B);
            }
            else if(sta_list.sta[index].phy_11g)
            {
                tuple[2] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_G);
            }
            else if(sta_list.sta[index].phy_11n)
            {
                tuple[2] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_N);
            }
            else if(sta_list.sta[index].phy_lr)
            {
                tuple[2] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_LOW_RATE);
            }
            else
            {
                tuple[2] = mp_const_none;
            }

            /*insert tuple */
            mp_obj_list_append(sta_out_list, mp_obj_new_attrtuple(wlan_sta_ifo_fields, 3, tuple));
        }
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    return sta_out_list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_ap_sta_list_obj, wlan_ap_sta_list);

STATIC mp_obj_t wlan_ap_tcpip_sta_list (mp_obj_t self_in) {
    STATIC const qstr wlan_sta_ifo_fields[] = {
        MP_QSTR_mac, MP_QSTR_IP
    };
    uint8_t index;
    wifi_sta_list_t wifi_sta_list;
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_sta_list_t sta_list;
    wlan_obj_t * self = self_in;

    mp_obj_t sta_out_list = mp_obj_new_list(0, NULL);
    /* Check if AP mode is enabled */
    if (self->mode == WIFI_MODE_AP || self->mode == WIFI_MODE_APSTA) {
        tcpip_adapter_get_sta_list(&wifi_sta_list, &sta_list);

        mp_obj_t tuple[2];
        for(index = 0; index < MAX_AP_CONNECTED_STA && index < sta_list.num; index++)
        {
            tuple[0] = mp_obj_new_bytes((const byte *)sta_list.sta[index].mac, 6);
            tuple[1] = netutils_format_ipv4_addr((uint8_t *)&sta_list.sta[index].ip.addr, NETUTILS_BIG);

            /*insert tuple */
            mp_obj_list_append(sta_out_list, mp_obj_new_attrtuple(wlan_sta_ifo_fields, 2, tuple));
        }
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    return sta_out_list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_ap_tcpip_sta_list_obj, wlan_ap_tcpip_sta_list);

STATIC mp_obj_t wlan_joined_ap_info (mp_obj_t self_in)
{
    STATIC const qstr wlan_sta_ifo_fields[] = {
        MP_QSTR_bssid, MP_QSTR_ssid, MP_QSTR_primary_chn, MP_QSTR_rssi, MP_QSTR_auth, MP_QSTR_standard
    };

    wifi_ap_record_t record_ap;

    if(ESP_OK != esp_wifi_sta_get_ap_info(&record_ap))
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    mp_obj_t tuple[6];

    tuple[0] = mp_obj_new_bytes(record_ap.bssid, 6);
    tuple[1] = mp_obj_new_str((const char*)record_ap.ssid, (size_t)strlen((const char*)record_ap.ssid));
    tuple[2] = MP_OBJ_NEW_SMALL_INT(record_ap.primary);
    tuple[3] = MP_OBJ_NEW_SMALL_INT(record_ap.rssi);
    tuple[4] = MP_OBJ_NEW_SMALL_INT(record_ap.authmode);
    if(record_ap.phy_11b)
    {
        tuple[5] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_B);
    }
    else if(record_ap.phy_11g)
    {
        tuple[5] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_G);
    }
    else if(record_ap.phy_11n)
    {
        tuple[5] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_N);
    }
    else if(record_ap.phy_lr)
    {
        tuple[5] = MP_OBJ_NEW_SMALL_INT(WLAN_PHY_LOW_RATE);
    }
    else
    {
        tuple[5] = mp_const_none;
    }

    return mp_obj_new_attrtuple(wlan_sta_ifo_fields, 6, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_joined_ap_info_obj, wlan_joined_ap_info);

STATIC mp_obj_t wlan_auth (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        if (self->auth == WIFI_AUTH_OPEN) {
            return mp_const_none;
        } else {
            mp_obj_t security[2];
            security[0] = mp_obj_new_int(self->auth);
            security[1] = mp_obj_new_str((const char *)self->key, strlen((const char *)self->key));
            return mp_obj_new_tuple(2, security);
        }
    } else {
        // get the auth config
        uint8_t auth = WIFI_AUTH_OPEN;
        const char *key = NULL;
        if (args[1] != mp_const_none) {
            mp_obj_t *sec;
            mp_obj_get_array_fixed_n(args[1], 2, &sec);
            auth = mp_obj_get_int(sec[0]);
            key = mp_obj_str_get_str(sec[1]);
            wlan_validate_security(auth, key);
        }
        wlan_set_security_internal(auth, key);
        wifi_config_t config;
        esp_wifi_get_config(WIFI_IF_AP, &config);
        config.ap.authmode = wlan_obj.auth;
        strcpy((char *)config.ap.password, (char *)wlan_obj.key);
        esp_wifi_set_config(WIFI_IF_AP, &config);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_auth_obj, 1, 2, wlan_auth);

STATIC mp_obj_t wlan_hostname (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_str((const char *)self->hostname, strlen((const char *)self->hostname));
    } else {
        const char *hostname = mp_obj_str_get_str(args[1]);
        if(hostname == NULL)
            return mp_obj_new_bool(false);

        wlan_validate_hostname(hostname);
        return mp_obj_new_bool(wlan_set_hostname(hostname));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_hostname_obj, 1, 2, wlan_hostname);

STATIC mp_obj_t wlan_send_raw (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_Buffer,                     MP_ARG_KW_ONLY  | MP_ARG_REQUIRED    |MP_ARG_OBJ,},
        { MP_QSTR_interface,                  MP_ARG_KW_ONLY  | MP_ARG_INT,    {.u_int = WIFI_MODE_STA}},
        { MP_QSTR_use_sys_seq,              MP_ARG_KW_ONLY  | MP_ARG_BOOL,    {.u_bool = true}},
    };

    mp_buffer_info_t value_bufinfo;
    wifi_interface_t ifx;

    wlan_obj_t* self = (wlan_obj_t*)pos_args[0];

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    switch(self->mode)
    {
    case WIFI_MODE_STA:
        ifx = ESP_IF_WIFI_STA;
        break;
    case WIFI_MODE_AP:
        ifx  = ESP_IF_WIFI_AP;
        break;
    case WIFI_MODE_APSTA:
        if(args[1].u_int == WIFI_MODE_AP)
        {
            ifx  = ESP_IF_WIFI_AP;
        }
        else
        {
            ifx = ESP_IF_WIFI_STA;
        }
        break;
    default:
        ifx = ESP_IF_WIFI_STA;
        break;
    }

    if(!MP_OBJ_IS_TYPE(args[0].u_obj, &mp_type_bytes))
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
    }
    else
    {
        mp_get_buffer_raise(args[0].u_obj, &value_bufinfo, MP_BUFFER_READ);

        if(value_bufinfo.len > 1500 || value_bufinfo.len < 24)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Buffer size should be between 24 and 1500 bytes!"));
        }
    }

    //send packet
    if(ESP_OK != esp_wifi_80211_tx(ifx, value_bufinfo.buf, value_bufinfo.len, args[2].u_bool))
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_send_raw_obj, 1, wlan_send_raw);

STATIC mp_obj_t wlan_channel (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->channel);
    }
    else
    {
        uint8_t channel;

        if (self->mode == WIFI_MODE_AP)
        {
            channel  = mp_obj_get_int(args[1]);
            wlan_validate_channel(channel);
            wifi_config_t config;
            esp_wifi_get_config(WIFI_IF_AP, &config);
            config.ap.channel = channel;
            wlan_obj.channel = channel;
            esp_wifi_set_config(WIFI_IF_AP, &config);
        }
        else if (self->mode == WIFI_MODE_STA)
        {
            if(!self->is_promiscuous)
            {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Wifi is not in promiscuous mode!"));
            }

            if(self->bandwidth == WIFI_BW_HT20)
            {
                channel = mp_obj_get_int(args[1]);
                wlan_validate_channel(channel);
                esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            }
            else
            {
                uint8_t secondary_chn;

                if(n_args < 3)
                {
                    secondary_chn = WIFI_SECOND_CHAN_NONE;
                }
                else
                {
                    secondary_chn = mp_obj_get_int(args[2]);

                    if(secondary_chn > WIFI_SECOND_CHAN_BELOW)
                    {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Secondary Channel position!"));
                    }
                }

                channel = mp_obj_get_int(args[1]);

                wlan_validate_channel(channel);

                esp_wifi_set_channel(channel, secondary_chn);
            }
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_channel_obj, 1, 2, wlan_channel);

STATIC mp_obj_t wlan_antenna (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->antenna);
    } else {
        uint8_t antenna  = mp_obj_get_int(args[1]);
        antenna_validate_antenna(antenna);
        wlan_set_antenna(antenna);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_antenna_obj, 1, 2, wlan_antenna);

STATIC mp_obj_t wlan_mac (mp_uint_t n_args, const mp_obj_t *args) {
    wlan_obj_t *self = args[0];
    STATIC const qstr wlan_mac_fields[] = {
        MP_QSTR_sta_mac, MP_QSTR_ap_mac
    };

    if (n_args == 1)
    {
        mp_obj_t tuple[2] =
        {
                mp_obj_new_bytes((const byte *)self->mac, sizeof(self->mac)),
                mp_obj_new_bytes((const byte *)self->mac_ap, sizeof(self->mac_ap))
        };

        return mp_obj_new_attrtuple(wlan_mac_fields, 2, tuple);
    }
    else if(n_args == 3)
    {
        wifi_interface_t interface;

        switch(mp_obj_get_int(args[2]))
        {
        case WIFI_MODE_STA:
            interface = ESP_IF_WIFI_STA;
            break;
        case WIFI_MODE_AP:
            interface  = ESP_IF_WIFI_AP;
            break;
        default:
            goto op_not_possible;
        }

        if(!MP_OBJ_IS_TYPE(args[1], &mp_type_bytearray))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
        }

        mp_buffer_info_t value_bufinfo;
        mp_get_buffer_raise(args[1], &value_bufinfo, MP_BUFFER_READ);

        if(value_bufinfo.len != 6 )
        {
            goto invalid_value;
        }

        uint8_t mac[6];
        // Check if we have STA and AP with same mac
        if(mp_obj_get_int(args[2]) == WIFI_MODE_STA)
        {
            esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
        }
        else
        {
            esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
        }

        uint8_t* buffer = (uint8_t*)value_bufinfo.buf;

        if(!memcmp(mac, buffer, value_bufinfo.len))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Station and AP cannot have the same Mac address!"));
        }


        //bit 0 of first byte should not be = 1
        if((*buffer & ((uint8_t)1)))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Bit 0 of First Byte in Mac addr cannot be 1"));
        }


        if(ESP_OK != esp_wifi_set_mac(interface, (uint8_t*)value_bufinfo.buf))
        {
            goto op_not_possible;
        }

        if(interface == ESP_IF_WIFI_STA)
        {
            memcpy(self->mac, (uint8_t*)value_bufinfo.buf, sizeof(self->mac));
        }
        else
        {
            memcpy(self->mac_ap, (uint8_t*)value_bufinfo.buf, sizeof(self->mac_ap));
        }

        return mp_const_none;

    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
    }

op_not_possible:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));

invalid_value:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_mac_obj, 1, 3, wlan_mac);

STATIC mp_obj_t wlan_max_tx_power (mp_uint_t n_args, const mp_obj_t *args) {

    int8_t pwr;

    if(n_args > 1)
    {
        pwr = mp_obj_get_int(args[1]);
        if(ESP_OK != esp_wifi_set_max_tx_power(pwr))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
        }
        wlan_obj.max_tx_pwr = pwr;
        return mp_const_none;
    }
    else
    {
        if(ESP_OK != esp_wifi_get_max_tx_power(&pwr))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
        }
        return MP_OBJ_NEW_SMALL_INT(pwr);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_max_tx_power_obj, 1, 2, wlan_max_tx_power);

STATIC mp_obj_t wlan_country(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const qstr wlan_scan_info_fields[] = {
        MP_QSTR_country, MP_QSTR_schan, MP_QSTR_nchan, MP_QSTR_max_tx_pwr, MP_QSTR_policy
    };

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_country,                     MP_ARG_KW_ONLY  | MP_ARG_OBJ,                    {.u_obj = mp_const_none}},
        { MP_QSTR_schan,                    MP_ARG_KW_ONLY  | MP_ARG_OBJ,                    {.u_obj = mp_const_none}},
        { MP_QSTR_nchan,                      MP_ARG_KW_ONLY  | MP_ARG_OBJ,                    {.u_obj = mp_const_none}},
        { MP_QSTR_max_tx_pwr,                  MP_ARG_KW_ONLY  | MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_policy,                     MP_ARG_KW_ONLY  | MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if(args[0].u_obj == mp_const_none && args[1].u_obj == mp_const_none && args[2].u_obj == mp_const_none && args[3].u_obj == mp_const_none && args[4].u_obj == mp_const_none)
    {
        wifi_country_t outconfig;
        esp_wifi_get_country(&outconfig);
        mp_obj_t tuple[5];

        // fill tuple
        tuple[0] = mp_obj_new_str((const char*)outconfig.cc, 2);
        tuple[1] = MP_OBJ_NEW_SMALL_INT(outconfig.schan);
        tuple[2] = MP_OBJ_NEW_SMALL_INT(outconfig.nchan);
        tuple[3] = MP_OBJ_NEW_SMALL_INT(outconfig.max_tx_power);
        tuple[4] = MP_OBJ_NEW_SMALL_INT(outconfig.policy);

        mp_obj_t data = mp_obj_new_attrtuple(wlan_scan_info_fields, 5, tuple);

        return data;
    }

    wifi_country_t country_config;

    //country
    if (args[0].u_obj != mp_const_none) {
        const char * country = mp_obj_str_get_str(args[0].u_obj);
        wlan_validate_country(country);
        memcpy(&(country_config.cc[0]), country, strlen(country));
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "No Country Specified!"));
    }

    // start channel
    if (args[1].u_obj != mp_const_none) {
        uint8_t startchn = mp_obj_get_int(args[1].u_obj);
        wlan_validate_channel(startchn);
        country_config.schan = startchn;
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Please Specify start channel!"));
    }

    // num of channels
    if (args[2].u_obj != mp_const_none) {
        uint8_t numchn = mp_obj_get_int(args[2].u_obj);
        wlan_validate_channel(numchn);
        country_config.nchan = numchn;
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Please Specify number of channels!"));
    }

    // max tx power
    if(args[3].u_obj != mp_const_none)
    {
        country_config.max_tx_power = mp_obj_get_int(args[3].u_obj);
    }
    else
    {
        country_config.max_tx_power = CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER;
    }

    //policy
    if(args[4].u_obj != mp_const_none)
    {
        wlan_validate_country_policy(mp_obj_get_int(args[4].u_obj));
        country_config.policy = (wifi_country_policy_t)mp_obj_get_int(args[4].u_obj);
    }
    else
    {
        country_config.policy = WIFI_COUNTRY_POLICY_AUTO;
    }

    /* Set configuration */
    if(ESP_OK != esp_wifi_set_country(&country_config))
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }

    if(wlan_obj.country == NULL) {
        wlan_obj.country = (wifi_country_t*)malloc(sizeof(wifi_country_t));
    }
    memcpy(wlan_obj.country, &country_config, sizeof(wifi_country_t));

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_country_obj, 1, wlan_country);

STATIC mp_obj_t wlan_promiscuous_mode (mp_uint_t n_args, const mp_obj_t *args)
{
    wlan_obj_t* self = (wlan_obj_t*)args[0];

    if (n_args > 1)
    {
        /* Set promiscuous mode */
        if(mp_obj_is_true(args[1]))
        {
            if(ESP_OK == esp_wifi_set_promiscuous(true))
            {
                self->is_promiscuous = true;
                esp_wifi_set_promiscuous_rx_cb(promiscuous_callback);
            }
            else
            {
                goto error;
            }
        }
        else
        {
            if(ESP_OK == esp_wifi_set_promiscuous(false))
            {
                self->is_promiscuous = false;
            }
            else
            {
                goto error;
            }
        }

        return mp_const_none;
    }
    else
    {
        /* return the current status */
        return mp_obj_new_bool(self->is_promiscuous);
    }

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_promiscuous_mode_obj, 1, 2, wlan_promiscuous_mode);

STATIC mp_obj_t wlan_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    wlan_obj_t *self = pos_args[0];

    wifi_promiscuous_filter_t filter =
    {
            .filter_mask = 0
    };

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        if (self->trigger <= MOD_WLAN_TRIGGER_PKT_ANY)
        {
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_MGMT)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_MGMT;
            }
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_CTRL)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_CTRL;
            }
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_DATA)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_DATA;
            }
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_DATA_MPDU)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_DATA_MPDU;
            }
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_DATA_AMPDU)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_DATA_AMPDU;
            }
            if(self->trigger & MOD_WLAN_TRIGGER_PKT_MISC)
            {
                filter.filter_mask |= WIFI_PROMIS_FILTER_MASK_MISC;
            }
            esp_wifi_set_promiscuous_filter(&filter);
        }
        self->handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    } else {  // disable the callback
        self->trigger = 0;
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    mp_irq_add(self, args[1].u_obj);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(wlan_callback_obj, 1, wlan_callback);

STATIC mp_obj_t wlan_events(mp_obj_t self_in) {
    wlan_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_events_obj, wlan_events);

STATIC mp_obj_t wlan_packet(mp_obj_t self_in) {

    wlan_obj_t* self = self_in;

    STATIC const qstr wlan_pkt_info_fields[] = {
            MP_QSTR_rssi, MP_QSTR_rate, MP_QSTR_sig_mode, MP_QSTR_mcs, MP_QSTR_cwb, MP_QSTR_aggregation, MP_QSTR_stbc, MP_QSTR_fec_coding, MP_QSTR_sgi, MP_QSTR_noise_floor, MP_QSTR_ampdu_cnt, MP_QSTR_channel,
            MP_QSTR_sec_channel, MP_QSTR_time_stamp, MP_QSTR_ant, MP_QSTR_sig_len, MP_QSTR_rx_state, MP_QSTR_data
        };

    uint8_t loc_tocken = token;

    xSemaphoreTake(self->mutex, portMAX_DELAY);

    token ^= 1;

    xSemaphoreGive(self->mutex);

    mp_obj_t tuple[MAX_WIFI_PKT_PARAMS];

    tuple[0] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.rssi);
    tuple[1] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.rate);
    tuple[2] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.sig_mode);
    tuple[3] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.mcs);
    tuple[4] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.cwb);
    tuple[5] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.aggregation);
    tuple[6] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.stbc);
    tuple[7] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.fec_coding);
    tuple[8] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.sgi);
    tuple[9] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.noise_floor);
    tuple[10] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.ampdu_cnt);
    tuple[11] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.channel);
    tuple[12] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.secondary_channel);
    tuple[13] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.timestamp);
    tuple[14] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.ant);
    tuple[15] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.sig_len);
    tuple[16] = mp_obj_new_int(wlan_prom_packet[loc_tocken].rx_ctrl.rx_state);

    if(wlan_prom_packet[loc_tocken].pkt_type != WIFI_PKT_CTRL)
    {
        tuple[17] = mp_obj_new_bytes((const uint8_t*)(wlan_prom_packet[loc_tocken].data), wlan_prom_packet[loc_tocken].rx_ctrl.sig_len);
    }
    else
    {
        tuple[17] = mp_const_none;
    }


    return mp_obj_new_attrtuple(wlan_pkt_info_fields, MAX_WIFI_PKT_PARAMS, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(wlan_packet_obj, wlan_packet);

STATIC mp_obj_t wlan_ctrl_pkt_filter(mp_uint_t n_args, const mp_obj_t *args) {

    wifi_promiscuous_filter_t  filter_ctrl_mask;

    if(n_args > 1)
    {
        filter_ctrl_mask.filter_mask  = mp_obj_get_int(args[1]);

        if((filter_ctrl_mask.filter_mask < WIFI_PROMIS_CTRL_FILTER_MASK_WRAPPER) || (filter_ctrl_mask.filter_mask > WIFI_PROMIS_CTRL_FILTER_MASK_ALL))
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Invalid Filter mask!"));
        }
        else
        {
            esp_wifi_set_promiscuous_ctrl_filter(&filter_ctrl_mask);
        }
    }
    else
    {
        esp_wifi_set_promiscuous_ctrl_filter(&filter_ctrl_mask);

        return mp_obj_new_int(filter_ctrl_mask.filter_mask);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(wlan_ctrl_pkt_filter_obj, 1, 2, wlan_ctrl_pkt_filter);


STATIC const mp_map_elem_t wlan_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&wlan_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&wlan_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_scan),                (mp_obj_t)&wlan_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&wlan_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&wlan_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&wlan_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig),            (mp_obj_t)&wlan_ifconfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mode),                (mp_obj_t)&wlan_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bandwidth),           (mp_obj_t)&wlan_bandwidth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ssid),                (mp_obj_t)&wlan_ssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bssid),               (mp_obj_t)&wlan_bssid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_auth),                (mp_obj_t)&wlan_auth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hostname),            (mp_obj_t)&wlan_hostname_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_channel),             (mp_obj_t)&wlan_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_antenna),             (mp_obj_t)&wlan_antenna_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&wlan_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ap_sta_list),         (mp_obj_t)&wlan_ap_sta_list_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ap_tcpip_sta_list),   (mp_obj_t)&wlan_ap_tcpip_sta_list_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_max_tx_power),        (mp_obj_t)&wlan_max_tx_power_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_country),             (mp_obj_t)&wlan_country_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_joined_ap_info),      (mp_obj_t)&wlan_joined_ap_info_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_protocol),       (mp_obj_t)&wlan_wifi_protocol_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send_raw),            (mp_obj_t)&wlan_send_raw_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_promiscuous),         (mp_obj_t)&wlan_promiscuous_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),            (mp_obj_t)&wlan_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),              (mp_obj_t)&wlan_events_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wifi_packet),         (mp_obj_t)&wlan_packet_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ctrl_pkt_filter),     (mp_obj_t)&wlan_ctrl_pkt_filter_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_smartConfig),         (mp_obj_t)&wlan_smartConfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Connected_ap_pwd),    (mp_obj_t)&wlan_smartConfkey_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA),                         MP_OBJ_NEW_SMALL_INT(WIFI_MODE_STA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP),                          MP_OBJ_NEW_SMALL_INT(WIFI_MODE_AP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA_AP),                      MP_OBJ_NEW_SMALL_INT(WIFI_MODE_APSTA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WEP),                         MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WEP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA),                         MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA2),                        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WPA2_ENT),                    MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_ENTERPRISE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_INT_ANT),                     MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_INTERNAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EXT_ANT),                     MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_EXTERNAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MAN_ANT),                     MP_OBJ_NEW_SMALL_INT(ANTENNA_TYPE_MANUAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_HT20),                        MP_OBJ_NEW_SMALL_INT(WIFI_BW_HT20) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_HT40),                        MP_OBJ_NEW_SMALL_INT(WIFI_BW_HT40) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_11_B),                    MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_B) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_11_G),                    MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_G) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_11_N),                    MP_OBJ_NEW_SMALL_INT(WLAN_PHY_11_N) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PHY_LOW_RATE),                MP_OBJ_NEW_SMALL_INT(WLAN_PHY_LOW_RATE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SCAN_PASSIVE),                MP_OBJ_NEW_SMALL_INT(WIFI_SCAN_TYPE_PASSIVE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SCAN_ACTIVE),                 MP_OBJ_NEW_SMALL_INT(WIFI_SCAN_TYPE_ACTIVE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_COUNTRY_POL_AUTO),            MP_OBJ_NEW_SMALL_INT(WIFI_COUNTRY_POLICY_AUTO) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_COUNTRY_POL_MAN),             MP_OBJ_NEW_SMALL_INT(WIFI_COUNTRY_POLICY_MANUAL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SECONDARY_CHN_ABOVE),         MP_OBJ_NEW_SMALL_INT(WIFI_SECOND_CHAN_ABOVE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SECONDARY_CHN_BELOW),         MP_OBJ_NEW_SMALL_INT(WIFI_SECOND_CHAN_BELOW) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SECONDARY_CHN_NONE),             MP_OBJ_NEW_SMALL_INT(WIFI_SECOND_CHAN_NONE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_MGMT),                 MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_MGMT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_CTRL),                 MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_CTRL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_DATA),                 MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_DATA_MPDU),            MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_DATA_MPDU) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_DATA_AMPDU),        MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_DATA_AMPDU) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_MISC),                 MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_MISC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_PKT_ANY),                 MP_OBJ_NEW_SMALL_INT(MOD_WLAN_TRIGGER_PKT_ANY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_ALL),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_ALL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_WRAPPER),     MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_WRAPPER) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_BAR),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_BAR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_BA),             MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_BA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_PSPOLL),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_PSPOLL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_CTS),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_CTS) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_ACK),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_ACK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_CFEND),         MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_CFEND) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FILTER_CTRL_PKT_CFENDACK),     MP_OBJ_NEW_SMALL_INT(WIFI_PROMIS_CTRL_FILTER_MASK_CFENDACK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SMART_CONF_DONE),             MP_OBJ_NEW_SMALL_INT(MOD_WLAN_SMART_CONFIG_DONE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SMART_CONF_TIMEOUT),             MP_OBJ_NEW_SMALL_INT(MOD_WLAN_SMART_CONFIG_TIMEOUT) },
};
STATIC MP_DEFINE_CONST_DICT(wlan_locals_dict, wlan_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_wlan = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_WLAN,
        .make_new = wlan_make_new,
        .locals_dict = (mp_obj_t)&wlan_locals_dict,
    },

    .n_gethostbyname = lwipsocket_gethostbyname,
    .n_socket = lwipsocket_socket_socket,
    .n_close = lwipsocket_socket_close,
    .n_bind = lwipsocket_socket_bind,
    .n_listen = lwipsocket_socket_listen,
    .n_accept = lwipsocket_socket_accept,
    .n_connect = lwipsocket_socket_connect,
    .n_send = lwipsocket_socket_send,
    .n_recv = lwipsocket_socket_recv,
    .n_sendto = lwipsocket_socket_sendto,
    .n_recvfrom = lwipsocket_socket_recvfrom,
    .n_setsockopt = lwipsocket_socket_setsockopt,
    .n_settimeout = lwipsocket_socket_settimeout,
    .n_ioctl = lwipsocket_socket_ioctl,
    .n_setupssl = lwipsocket_socket_setup_ssl,
	.inf_up = wlan_is_inf_up,
	.set_default_inf = wlan_set_default_inf
};

//STATIC const mp_irq_methods_t wlan_irq_methods = {
//    .init = wlan_irq,
//    .enable = wlan_lpds_irq_enable,
//    .disable = wlan_lpds_irq_disable,
//    .flags = wlan_irq_flags,
//};
