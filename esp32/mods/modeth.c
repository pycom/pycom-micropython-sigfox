/*
 * Copyright (c) 2019, Pycom Limited.
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

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"

#include "netutils.h"

#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth.h"

#include "ksz8851conf.h"
#include "ksz8851.h"

#include "modeth.h"
#include "mpexception.h"
#include "lwipsocket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#define ETHERNET_TASK_STACK_SIZE        4608
#define ETHERNET_TASK_PRIORITY          5
#define ETHERNET_RX_PACKET_BUFF_SIZE    (1500)
#define ETHERNET_CHECK_LINK_PERIOD_MS   2000
#define ETHERNET_CMD_QUEUE_SIZE         200

static void TASK_ETHERNET (void *pvParameters);
static mp_obj_t eth_init_helper(eth_obj_t *self, const mp_arg_val_t *args);
static IRAM_ATTR void ksz8851_evt_callback(uint32_t ksz8851_evt);
static void process_tx(uint8_t* buff, uint16_t len);
static void process_rx(void);
static void eth_validate_hostname (const char *hostname);


eth_obj_t eth_obj = {
        .mac = {0},
        .hostname = {0},
        .link_status = false,
        .sem = NULL,
        .trigger = 0,
        .events = 0,
        .handler = NULL,
        .handler_arg = NULL
};

static uint8_t modeth_rxBuff[ETHERNET_RX_PACKET_BUFF_SIZE];
#if defined(FIPY) || defined(GPY)
// Variable saving DNS info
static tcpip_adapter_dns_info_t eth_sta_inf_dns_info;
#endif
uint8_t ethernet_mac[ETH_MAC_SIZE] = {0};
xQueueHandle eth_cmdQueue = NULL;

void eth_pre_init (void) {
    tcpip_adapter_init();
    eth_obj.sem = xSemaphoreCreateBinary();
    //Create cmd Queue
    eth_cmdQueue = xQueueCreate(ETHERNET_CMD_QUEUE_SIZE, sizeof(modeth_cmd_ctx_t));
    // create eth Task
    xTaskCreatePinnedToCore(TASK_ETHERNET, "ethernet_task", ETHERNET_TASK_STACK_SIZE / sizeof(StackType_t), NULL, ETHERNET_TASK_PRIORITY, &ethernetTaskHandle, 1);
}

void modeth_get_mac(uint8_t *mac)
{
    memcpy(mac, ethernet_mac, ETH_MAC_SIZE);
}

STATIC const mp_arg_t eth_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_hostname,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
};
STATIC mp_obj_t eth_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(eth_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), eth_init_args, args);

    // setup the object
    eth_obj_t *self = &eth_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_eth;

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }
    // start the peripheral
    eth_init_helper(self, &args[1]);
    return (mp_obj_t)self;
}

STATIC mp_obj_t modeth_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(eth_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &eth_init_args[1], args);
    return eth_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(modeth_init_obj, 1, modeth_init);

STATIC mp_obj_t eth_init_helper(eth_obj_t *self, const mp_arg_val_t *args) {

    const char *hostname;

    if (args[0].u_obj != mp_const_none) {
        hostname = mp_obj_str_get_str(args[0].u_obj);
        eth_validate_hostname(hostname);
        tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname);
    }

    //Notify task to start right away
    xTaskNotifyGive(ethernetTaskHandle);
    return mp_const_none;
}

static esp_err_t modeth_event_handler(void *ctx, system_event_t *event)
{
    tcpip_adapter_ip_info_t ip;

    printf("ID:%d\n", event->event_id);

    switch (event->event_id) {
    case SYSTEM_EVENT_ETH_CONNECTED:
        mp_printf(&mp_plat_print,"Ethernet Link Up\n");
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
        mp_printf(&mp_plat_print, "Ethernet Link Down\n");
        mod_network_deregister_nic(&eth_obj);
        break;
    case SYSTEM_EVENT_ETH_START:
        mp_printf(&mp_plat_print, "Ethernet Started\n");
        break;
    case SYSTEM_EVENT_ETH_GOT_IP:
        memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(ESP_IF_ETH, &ip));
        mp_printf(&mp_plat_print, "Ethernet Got IP Addr\n");
        mp_printf(&mp_plat_print, "~~~~~~~~~~~\n");
        mp_printf(&mp_plat_print, "ETHIP:" IPSTR, IP2STR(&ip.ip));
        mp_printf(&mp_plat_print, "ETHMASK:" IPSTR, IP2STR(&ip.netmask));
        mp_printf(&mp_plat_print, "ETHGW:" IPSTR, IP2STR(&ip.gw));
        mp_printf(&mp_plat_print, "~~~~~~~~~~~\n");
#if defined(FIPY) || defined(GPY)
        // Save DNS info for restoring if wifi inf is usable again after LTE disconnect
        tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &eth_sta_inf_dns_info);
#endif
        mod_network_register_nic(&eth_obj);
        break;
    case SYSTEM_EVENT_ETH_STOP:
        mp_printf(&mp_plat_print, "Ethernet Stopped\n");
        break;
    default:
        break;
    }
    return ESP_OK;
}

STATIC void eth_set_default_inf(void)
{
#if defined(FIPY) || defined(GPY)
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &eth_sta_inf_dns_info);
    tcpip_adapter_up(TCPIP_ADAPTER_IF_ETH);
#endif
}
static void process_tx(uint8_t* buff, uint16_t len)
{
    if (eth_obj.link_status) {
        ksz8851BeginPacketSend(len);
        ksz8851SendPacketData(buff, len);
        ksz8851EndPacketSend();
    }
}
static void process_rx(void)
{
    uint32_t len, frameCnt;
    uint32_t isr = ksz8851_regrd(REG_INT_STATUS);
    uint32_t ier = ksz8851_regrd(REG_INT_MASK);

    /* Clear RX interrupt */
    isr |= INT_RX;
    ksz8851_regwr(REG_INT_STATUS, isr);
    frameCnt = (ksz8851_regrd(REG_RX_FRAME_CNT_THRES) & RX_FRAME_CNT_MASK) >> 8;
    while (frameCnt > 0)
    {
        ksz8851RetrievePacketData(modeth_rxBuff, &len);
        if(len)
        {
            tcpip_adapter_eth_input(modeth_rxBuff, len, NULL);
        }
        else
        {
            printf("Error recieve  \n");
        }
        frameCnt--;
    }
    //Re-enable RX interrupt
    ier |= INT_RX;
    ksz8851_regwr(REG_INT_MASK, ier);
}
static IRAM_ATTR void ksz8851_evt_callback(uint32_t ksz8851_evt)
{
    modeth_cmd_ctx_t ctx;
    portBASE_TYPE tmp;
    uint16_t ier, isr;

    if(ksz8851_evt & KSZ8851_LINK_CHG_INT)
    {
        ctx.cmd = ETH_CMD_CHK_LINK;
        ctx.buf = NULL;
        xQueueSendToFrontFromISR(eth_cmdQueue, &ctx, &tmp);
        if (tmp != pdFALSE) {
            portYIELD_FROM_ISR();
        }
        // unblock task if link up
        if(!eth_obj.link_status)
        {
            xTaskNotifyFromISR(ethernetTaskHandle, 0, eIncrement, NULL);
        }
    }

    if(ksz8851_evt & KSZ8851_RX_INT)
    {
        ctx.cmd = ETH_CMD_RX;
        ctx.buf = NULL;
        xQueueSendFromISR(eth_cmdQueue, &ctx, &tmp);
        if (tmp != pdFALSE) {
            portYIELD_FROM_ISR();
        }
    }

    if(ksz8851_evt & KSZ8851_OVERRUN_INT)
    {
        isr = ksz8851_regrd(REG_INT_STATUS);
        ier = ksz8851_regrd(REG_INT_MASK);

        /* Clear LINK interrupt */
        isr |= INT_RX_OVERRUN;
        ksz8851_regwr(REG_INT_STATUS, isr);
        //Re-enable LINK interrupt
        ier |= INT_RX_OVERRUN;
        ksz8851_regwr(REG_INT_MASK, ier);
    }
}


eth_speed_mode_t get_eth_link_speed(void)
{
    uint16_t speed = ksz8851_regrd(REG_PORT_STATUS);
    if((speed & (PORT_STAT_SPEED_100MBIT)))
    {
        return ETH_SPEED_MODE_100M;
    }
    else
    {
        return ETH_SPEED_MODE_10M;
    }
}

bool is_eth_link_up(void)
{
    return eth_obj.link_status;
}

static void TASK_ETHERNET (void *pvParameters) {

    static uint32_t thread_notification;
    system_event_t evt;
    modeth_cmd_ctx_t queue_entry;
    uint32_t ier, isr;

    /*TODO: Interrupt registration*/

    // Block task till notification is recieved
    thread_notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (thread_notification)
    {
        ESP_ERROR_CHECK(esp_event_loop_init(modeth_event_handler, NULL));
        esp_event_set_default_eth_handlers();
        //esp_read_mac(ethernet_mac, ESP_MAC_ETH);
        ethernet_mac[0] = KSZ8851_MAC0;
        ethernet_mac[1] = KSZ8851_MAC1;
        ethernet_mac[2] = KSZ8851_MAC2;
        ethernet_mac[3] = KSZ8851_MAC3;
        ethernet_mac[4] = KSZ8851_MAC4;
        ethernet_mac[5] = KSZ8851_MAC5;
        //save mac
        memcpy(eth_obj.mac, ethernet_mac, ETH_MAC_SIZE);

        //Init spi
        ksz8851SpiInit();
        // Init Driver
        ksz8851Init();
        /* link status  */
        ksz8851RegisterEvtCb(ksz8851_evt_callback);

        evt.event_id = SYSTEM_EVENT_ETH_START;
        esp_event_send(&evt);

        for(;;)
        {
            if(!eth_obj.link_status)
            {
                // block till link is up again
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            }

            if (xQueueReceive(eth_cmdQueue, &queue_entry, 0) == pdTRUE)
            {
                switch(queue_entry.cmd)
                {
                    case ETH_CMD_TX:
                        process_tx(queue_entry.buf, queue_entry.len);
                        break;
                    case ETH_CMD_RX:
                        process_rx();
                        break;
                    case ETH_CMD_CHK_LINK:
                        isr = ksz8851_regrd(REG_INT_STATUS);
                        ier = ksz8851_regrd(REG_INT_MASK);

                        /* Clear LINK interrupt */
                        isr |= INT_PHY;
                        ksz8851_regwr(REG_INT_STATUS, isr);

                        if(ksz8851GetLinkStatus())
                        {
                            eth_obj.link_status = true;
                            evt.event_id = SYSTEM_EVENT_ETH_CONNECTED;
                            esp_event_send(&evt);
                        }
                        else
                        {
                            eth_obj.link_status = false;
                            evt.event_id = SYSTEM_EVENT_ETH_DISCONNECTED;
                            esp_event_send(&evt);
                        }
                        //Re-enable LINK interrupt
                        ier |= INT_PHY;
                        ksz8851_regwr(REG_INT_MASK, ier);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

STATIC void eth_validate_hostname (const char *hostname) {
    //dont set hostname it if is null, so its a valid hostname
    if (hostname == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

    uint8_t len = strlen(hostname);
    if(len == 0 || len > TCPIP_HOSTNAME_MAX_SIZE){
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

STATIC mp_obj_t eth_ifconfig (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t wlan_ifconfig_args[] = {
        { MP_QSTR_config,           MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(wlan_ifconfig_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), wlan_ifconfig_args, args);

    tcpip_adapter_dns_info_t dns_info;
    // get the configuration
    if (args[0].u_obj == MP_OBJ_NULL) {
        // get
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
        if (ESP_OK == tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip_info)) {
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
        if (MP_OBJ_IS_TYPE(args[0].u_obj, &mp_type_tuple)) {
            // set a static ip
            mp_obj_t *items;
            mp_obj_get_array_fixed_n(args[0].u_obj, 4, &items);

            tcpip_adapter_ip_info_t ip_info;
            netutils_parse_ipv4_addr(items[0], (uint8_t *)&ip_info.ip.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[1], (uint8_t *)&ip_info.netmask.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[2], (uint8_t *)&ip_info.gw.addr, NETUTILS_BIG);
            netutils_parse_ipv4_addr(items[3], (uint8_t *)&dns_info.ip, NETUTILS_BIG);

            tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
            tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &ip_info);
            tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &dns_info);

        } else {
            // check for the correct string
            const char *mode = mp_obj_str_get_str(args[0].u_obj);
            if (strcmp("dhcp", mode) && strcmp("auto", mode)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
            }

            if (ESP_OK != tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(eth_ifconfig_obj, 1, eth_ifconfig);

STATIC mp_obj_t eth_hostname (mp_uint_t n_args, const mp_obj_t *args) {
    eth_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_str((const char *)self->hostname, strlen((const char *)self->hostname));
    }
    else
    {
        const char *hostname = mp_obj_str_get_str(args[1]);
        if(hostname == NULL)
        {
            return mp_obj_new_bool(false);
        }
        eth_validate_hostname(hostname);
        tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(eth_hostname_obj, 1, 2, eth_hostname);

STATIC mp_obj_t modeth_mac (mp_obj_t self_in) {
    eth_obj_t *self = self_in;

    return mp_obj_new_bytes((const byte *)self->mac, sizeof(self->mac));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeth_mac_obj, modeth_mac);

STATIC const mp_map_elem_t eth_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&modeth_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig),            (mp_obj_t)&eth_ifconfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hostname),            (mp_obj_t)&eth_hostname_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&modeth_mac_obj },


    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA),                         MP_OBJ_NEW_SMALL_INT(WIFI_MODE_STA) },
};
STATIC MP_DEFINE_CONST_DICT(eth_locals_dict, eth_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_eth = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_ETH,
        .make_new = eth_make_new,
        .locals_dict = (mp_obj_t)&eth_locals_dict,
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
    .inf_up = NULL,
    .set_default_inf = eth_set_default_inf
};
