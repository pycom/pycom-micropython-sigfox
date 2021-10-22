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

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"

#include "netutils.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_eth.h"

#include "gpio.h"
#include "machpin.h"
#include "pins.h"
#include "ksz8851conf.h"
#include "ksz8851.h"

#include "modeth.h"
#include "mpexception.h"
#include "lwipsocket.h"
//#include "modmachine.h"
//#include "mperror.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp32chipinfo.h"
#include "app_sys_evt.h"

/*****************************************************************************
* DEFINE CONSTANTS
*****************************************************************************/
#define ETHERNET_TASK_STACK_SIZE        3072
#define ETHERNET_TASK_PRIORITY          24 // 12
#define ETHERNET_CHECK_LINK_PERIOD_MS   2000
#define ETHERNET_CMD_QUEUE_SIZE         100

//EVENT bits
#define ETHERNET_EVT_CONNECTED        0x0001
#define ETHERNET_EVT_STARTED          0x0002

//#define DEBUG_MODETH
//#define MSG(fmt, ...) printf("[%u] modeth: " fmt, mp_hal_ticks_ms(), ##__VA_ARGS__)
#define MSG(fmt, ...) (void)0

/*****************************************************************************
* DECLARE PRIVATE FUNCTIONS
*****************************************************************************/
static void TASK_ETHERNET (void *pvParameters);
static mp_obj_t eth_init_helper(eth_obj_t *self, const mp_arg_val_t *args);
static IRAM_ATTR void ksz8851_evt_callback(uint32_t ksz8851_evt);
static void process_tx(uint8_t* buff, uint16_t len);
static uint32_t process_rx(void);
static void eth_validate_hostname (const char *hostname);
static esp_err_t modeth_event_handler(void *ctx, system_event_t *event);

/*****************************************************************************
* DECLARE PRIVATE DATA
*****************************************************************************/
eth_obj_t DRAM_ATTR eth_obj = {
        .mac = {0},
        .hostname = {0},
        .link_status = false,
        .sem = NULL,
        .trigger = 0,
        .events = 0,
        .handler = NULL,
        .handler_arg = NULL
};

static uint8_t* modeth_rxBuff = NULL;
#if defined(FIPY) || defined(GPY)
// Variable saving DNS info
static tcpip_adapter_dns_info_t eth_sta_inf_dns_info;
#endif
uint8_t ethernet_mac[ETH_MAC_SIZE] = {0};
xQueueHandle DRAM_ATTR eth_cmdQueue = NULL;
static DRAM_ATTR EventGroupHandle_t eth_event_group;

/*****************************************************************************
* DEFINE PUBLIC FUNCTIONS
*****************************************************************************/

void eth_pre_init (void) {
    // init tcpip stack
    tcpip_adapter_init();
    // init tcpip eth evt handlers to default
    esp_event_set_default_eth_handlers();
    // register eth app evt handler
    app_sys_register_evt_cb(APP_SYS_EVT_ETH, modeth_event_handler);
    //Create cmd Queue
    eth_cmdQueue = xQueueCreate(ETHERNET_CMD_QUEUE_SIZE, sizeof(modeth_cmd_ctx_t));
    //Create event group
    eth_event_group = xEventGroupCreate();

    // create eth Task
    xTaskCreatePinnedToCore(TASK_ETHERNET, "ethernet_task", ETHERNET_TASK_STACK_SIZE / sizeof(StackType_t), NULL, ETHERNET_TASK_PRIORITY, &ethernetTaskHandle, 1);
}

void modeth_get_mac(uint8_t *mac)
{
    memcpy(mac, ethernet_mac, ETH_MAC_SIZE);
}

eth_speed_mode_t get_eth_link_speed(void)
{
    portDISABLE_INTERRUPTS();
    uint16_t speed = ksz8851_regrd(REG_PORT_STATUS);
    portENABLE_INTERRUPTS();
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

/*****************************************************************************
* DEFINE PRIVATE FUNCTIONS
*****************************************************************************/

static esp_err_t modeth_event_handler(void *ctx, system_event_t *event)
{
    tcpip_adapter_ip_info_t ip;

    switch (event->event_id) {
    case SYSTEM_EVENT_ETH_CONNECTED:
        MSG("EH Ethernet Link Up\n");
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
        MSG("EH Ethernet Link Down\n");
        mod_network_deregister_nic(&eth_obj);
        xEventGroupClearBits(eth_event_group, ETHERNET_EVT_CONNECTED);
        break;
    case SYSTEM_EVENT_ETH_START:
        MSG("EH Ethernet Started\n");
        break;
    case SYSTEM_EVENT_ETH_GOT_IP:
        memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(ESP_IF_ETH, &ip));
        MSG("EH Got IP Addr: " IPSTR " " IPSTR " " IPSTR "\n", IP2STR(&ip.ip), IP2STR(&ip.netmask), IP2STR(&ip.gw));
#if defined(FIPY) || defined(GPY)
        MSG("EH save DNS\n");
        // Save DNS info for restoring if wifi inf is usable again after LTE disconnect
        tcpip_adapter_get_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &eth_sta_inf_dns_info);
#endif
        mod_network_register_nic(&eth_obj);
        xEventGroupSetBits(eth_event_group, ETHERNET_EVT_CONNECTED);
        break;
    case SYSTEM_EVENT_ETH_STOP:
        MSG("EH Ethernet Stopped\n");
        xEventGroupClearBits(eth_event_group, ETHERNET_EVT_STARTED);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void eth_set_default_inf(void)
{
#if defined(FIPY) || defined(GPY)
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_ETH, TCPIP_ADAPTER_DNS_MAIN, &eth_sta_inf_dns_info);
    tcpip_adapter_up(TCPIP_ADAPTER_IF_ETH);
#endif
}

/* print an ethernet frame in a similar style as wireshark */
void print_frame(unsigned char* buf, size_t len){
    for ( uint16_t i = 0; i < len; ++i)
    {
        printf("%02x ", buf[i]);
        if (i%8 == 7)
            printf("  ");
        if (i%16==15)
            printf("\n");
    }
    printf("\n");
}

static void process_tx(uint8_t* buff, uint16_t len)
{
    MSG("process_tx(%u)\n", len);
#ifdef DEBUG_MODETH
    print_frame(buff, len);
#endif
    // disable int before reading buffer
    portDISABLE_INTERRUPTS();
    //ksz8851_regwr(REG_INT_MASK, 0);

    if (eth_obj.link_status) {
        ksz8851BeginPacketSend(len);
        ksz8851SendPacketData(buff, len);
        ksz8851EndPacketSend();
    }

    // re-enable int
    //ksz8851_regwr(REG_INT_MASK, INT_MASK);
    portENABLE_INTERRUPTS();

}

static uint32_t process_rx(void)
{
    uint32_t len, frameCnt;
    uint32_t totalLen = 0;

    // disable int before reading buffer
    portDISABLE_INTERRUPTS();
    //ksz8851_regwr(REG_INT_MASK, 0);

    frameCnt = (ksz8851_regrd(REG_RX_FRAME_CNT_THRES) & RX_FRAME_CNT_MASK) >> 8;
    uint32_t frameCntTotal = frameCnt;
    uint32_t frameCntZeroLen = 0;
    while (frameCnt > 0)
    {
        ksz8851RetrievePacketData(modeth_rxBuff, &len, frameCnt, frameCntTotal);
        if(len)
        {
            totalLen += len;
            tcpip_adapter_eth_input(modeth_rxBuff, len, NULL);
        } else {
            frameCntZeroLen++;
        }
        frameCnt--;
    }

    // re-enable int
    //ksz8851_regwr(REG_INT_MASK, INT_MASK);
    portENABLE_INTERRUPTS();

    MSG("process_rx frames=%u (zero=%u) totalLen=%u last: len=%u \n", frameCntTotal, frameCntZeroLen, totalLen, len);
#ifdef DEBUG_MODETH
    if (frameCntTotal){
        // print last frame
        print_frame(modeth_rxBuff, len);
    }
#endif

    return totalLen;
}

static void processInterrupt(void) {
    modeth_cmd_ctx_t ctx;
    ctx.buf = NULL;
    ctx.isr = 0;
    uint16_t processed = 0;

    portDISABLE_INTERRUPTS();

#ifdef DEBUG_MODETH
    uint32_t int_pin_before = pin_get_value(KSZ8851_INT_PIN);
#endif

    // read interrupt status
    ctx.isr = ksz8851_regrd(REG_INT_STATUS);

    // clear interrupt status
    ksz8851_regwr(REG_INT_STATUS, 0xFFFF);

#ifdef DEBUG_MODETH
    uint32_t int_pin_after = pin_get_value(KSZ8851_INT_PIN);

    // read rx reason
    uint16_t rxqcr = ksz8851_regrd(REG_RXQ_CMD);
#endif


    // FIXME: capture errQUEUE_FULL


    if (ctx.isr & INT_RX) {
        ctx.cmd = ETH_CMD_RX;
        xQueueSendToFront(eth_cmdQueue, &ctx, portMAX_DELAY);
        processed++;
    }

    if (ctx.isr & INT_RX_OVERRUN) {
        ctx.cmd = ETH_CMD_OVERRUN;
        xQueueSendToFront(eth_cmdQueue, &ctx, portMAX_DELAY);
        processed++;
    }

    if (ctx.isr & INT_PHY) {
        ctx.cmd = ETH_CMD_CHK_LINK;
        xQueueSendToFront(eth_cmdQueue, &ctx, portMAX_DELAY);
        processed++;
    }

    if ( ! processed ) {
        // This shouldn't happen regularly.
        // It migth be possible to happen in this case:
        // - interupt fires
        // - cmd is put is received via the queue
        // - another interupt fires and puts a new cmd into the queue
        // - processInterrupt for the first one is exectued, but handles both (all) events
        // - later the second cmd is handled but processInterrupt doesn't find anything to do
        // - this case shouldn't be a real problem though
        ctx.cmd = ETH_CMD_OTHER;
        xQueueSend(eth_cmdQueue, &ctx, portMAX_DELAY);
        processed++;
    }

    portENABLE_INTERRUPTS();

#ifdef MODETH
    if ( ctx.isr != 0x2008 || rxqcr != 0x630 )
        MSG("processInterrupt isr=0x%04x rxqcr=0x%04x %s%s%s pin:%u/%u\n", ctx.isr, rxqcr,
            (rxqcr & RXQ_STAT_TIME_INT) ? "t": "",
            (rxqcr & RXQ_STAT_BYTE_CNT_INT) ? "b": "",
            (rxqcr & RXQ_STAT_FRAME_CNT_INT) ? "f": "",
            int_pin_before,
            int_pin_after
        );
#endif
}


/* callback runs from interrupt context */
static IRAM_ATTR void ksz8851_evt_callback(uint32_t ksz8851_evt)
{
    modeth_cmd_ctx_t ctx;
    ctx.cmd = ETH_CMD_HW_INT;
    ctx.buf = NULL;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToFrontFromISR(eth_cmdQueue, &ctx, &xHigherPriorityTaskWoken);
    // seems is needed for link up at the start ... TODO is it actually the best solution to ulTaskNotifyTake() from Task_ETHERNET to wait for link up?
    // xTaskNotifyFromISR(ethernetTaskHandle, 0, eIncrement, NULL);
}

static void TASK_ETHERNET (void *pvParameters) {
    MSG("TE\n");

    static uint32_t thread_notification;
    system_event_t evt;
    modeth_cmd_ctx_t queue_entry;
    uint16_t timeout = 0;
    uint16_t max_timeout = 50u; // 5

    // Block task till notification is recieved
    thread_notification = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    MSG("TE tn=%u\n", thread_notification);

    if (thread_notification)
    {
        if(ESP_OK != esp_read_mac(ethernet_mac, ESP_MAC_ETH))
        {
            // Set mac to default
            ethernet_mac[0] = KSZ8851_MAC0;
            ethernet_mac[1] = KSZ8851_MAC1;
            ethernet_mac[2] = KSZ8851_MAC2;
            ethernet_mac[3] = KSZ8851_MAC3;
            ethernet_mac[4] = KSZ8851_MAC4;
            ethernet_mac[5] = KSZ8851_MAC5;
        }
        else
        {
            // check for MAC address limitation of KSZ8851 (5th Byte should not be 0x06)
            if(ethernet_mac[4] == 0x06)
            {
                // OR this byte with last byte
                ethernet_mac[4] |= (ethernet_mac[5] | 0x01 /*Just in case if last byte = 0*/ );
            }
        }
        //save mac
        memcpy(eth_obj.mac, ethernet_mac, ETH_MAC_SIZE);

        //Init spi
        ksz8851SpiInit();
        /* link status  */
        ksz8851RegisterEvtCb(ksz8851_evt_callback);
eth_start:
        MSG("TE eth_start\n");
        xQueueReset(eth_cmdQueue);
        xEventGroupWaitBits(eth_event_group, ETHERNET_EVT_STARTED, false, true, portMAX_DELAY);
        MSG("TE init driver\n");
        // Init Driver
        ksz8851Init();

        evt.event_id = SYSTEM_EVENT_ETH_START;
        esp_event_send(&evt);

        MSG("TE ls=%u 10M=%u 100M=%u\n", get_eth_link_speed(), ETH_SPEED_MODE_10M, ETH_SPEED_MODE_100M);
        for(;;)
        {
            // if(!eth_obj.link_status && (xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_STARTED))
            // {
            //     // block till link is up again
            //     MSG("TE link not up\n");
            //     ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            // }

            // MSG("TE x\n");
            if(!(xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_STARTED))
            {
                MSG("TE not started\n");
                // deinit called, free memory and block till re-init
                xQueueReset(eth_cmdQueue);
                heap_caps_free(modeth_rxBuff);
                eth_obj.link_status = false;
                evt.event_id = SYSTEM_EVENT_ETH_DISCONNECTED;
                esp_event_send(&evt);
                //Disable  interrupts
                portDISABLE_INTERRUPTS();
                ksz8851_regwr(REG_INT_MASK, 0x0000);
                portENABLE_INTERRUPTS();
                MSG("TE goto eth_start\n");
                goto eth_start;
            }

            if (xQueueReceive(eth_cmdQueue, &queue_entry, 200 / portTICK_PERIOD_MS) == pdTRUE)
            {


                switch(queue_entry.cmd)
                {
                    case ETH_CMD_TX:
                        //MSG("TE TX %u\n", queue_entry.len);
                        process_tx(queue_entry.buf, queue_entry.len);
                        break;
                    case ETH_CMD_HW_INT:
                        processInterrupt();
                        break;
                    case ETH_CMD_RX:
                        //MSG("TE RX {0x%x}\n", queue_entry.isr);
                        process_rx();
                        break;
                    case ETH_CMD_CHK_LINK:
                        MSG("TE CHK_LINK {0x%x}\n", queue_entry.isr);
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
                        break;
                    case ETH_CMD_OVERRUN:
                        MSG("TE OVERRUN {0x%x} ========================================\n", queue_entry.isr);
                        xQueueReset(eth_cmdQueue);
                        eth_obj.link_status = false;
                        ksz8851PowerDownMode();
                        evt.event_id = SYSTEM_EVENT_ETH_DISCONNECTED;
                        esp_event_send(&evt);
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        ksz8851Init();
                        break;
                    default:
                        MSG("TE def cmd:0x%x isr:0x%04x\n", queue_entry.cmd, queue_entry.isr);
                        break;
                }
            }
            else
            {
                timeout = 0;
                // Checking if interrupt line is locked up in Low state
                //TODO: This workaround should be removed once the lockup is resolved
                while((!pin_get_value(KSZ8851_INT_PIN)) && timeout < max_timeout)
                {
                    MSG("TE TO %u\n", timeout);
                    processInterrupt();
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    timeout++;
                }
                if(timeout >= max_timeout)
                {
                    printf("ETH interrupt pin stuck\n");
                    xQueueReset(eth_cmdQueue);
                    eth_obj.link_status = false;
                    ksz8851PowerDownMode();
                    evt.event_id = SYSTEM_EVENT_ETH_DISCONNECTED;
                    esp_event_send(&evt);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    // TODO: should we ksz8851SpiInit() here?
                    ksz8851Init();

                    // // workaround for the workaround. Even the ksz8851 chip reset above is not enough to reliable restablish eth communication, let's reset the whole chip
                    // printf("ksz8851 lockup detected ... resetting device\n");
                    // vTaskDelay(100 / portTICK_PERIOD_MS);
                    // machine_reset();
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

/*****************************************************************************
* MICROPYTHON FUNCTIONS
*****************************************************************************/

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
    MSG("ME ih\n");
    const char *hostname;

    if (!ethernetTaskHandle){
        MSG("ME ih epi\n");
        eth_pre_init();
    }

    if (args[0].u_obj != mp_const_none) {
        MSG("ME ih 0\n");
        hostname = mp_obj_str_get_str(args[0].u_obj);
        eth_validate_hostname(hostname);
        tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname);
    }

    if (!(xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_STARTED)) {
        MSG("ME ih !started (%u)\n", esp32_get_chip_rev());
        //alloc memory for rx buff
        if (esp32_get_chip_rev() > 0) {
            modeth_rxBuff = heap_caps_malloc(ETHERNET_RX_PACKET_BUFF_SIZE, MALLOC_CAP_SPIRAM);
        }
        else
        {
            modeth_rxBuff = heap_caps_malloc(ETHERNET_RX_PACKET_BUFF_SIZE, MALLOC_CAP_INTERNAL);
        }

        if(modeth_rxBuff == NULL)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Cant allocate memory for eth rx Buffer!"));
        }

        MSG("ME ih set(started)\n");
        xEventGroupSetBits(eth_event_group, ETHERNET_EVT_STARTED);

        //Notify task to start right away
        MSG("ME ih tnGive\n");
        xTaskNotifyGive(ethernetTaskHandle);
    }

    MSG("ME ih done\n");
    return mp_const_none;
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

// STATIC mp_obj_t modeth_ksz8851_reg_wr (mp_uint_t n_args, const mp_obj_t *args) {

//     if ((xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_STARTED)) {
//         if(mp_obj_get_int(args[2]) == 0)
//         {
//             //modeth_sem_lock();
//             return mp_obj_new_int(ksz8851_regrd(mp_obj_get_int(args[1])));
//             //modeth_sem_unlock();
//         }
//         else
//         {
//             if (n_args > 3) {
//                 //modeth_sem_lock();
//                 ksz8851_regwr(mp_obj_get_int(args[1]), mp_obj_get_int(args[3]));
//                 //modeth_sem_unlock();
//             }
//             return mp_const_none;
//         }
//     }

//     nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Ethernet module not initialized!"));
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(modeth_ksz8851_reg_wr_obj, 3, 4, modeth_ksz8851_reg_wr);

STATIC mp_obj_t modeth_deinit (mp_obj_t self_in) {

    system_event_t evt;

    if ((xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_STARTED)) {
        mod_network_deregister_nic(&eth_obj);
        evt.event_id = SYSTEM_EVENT_ETH_STOP;
        esp_event_send(&evt);

        ksz8851PowerDownMode();
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeth_deinit_obj, modeth_deinit);

STATIC mp_obj_t modeth_isconnected(mp_obj_t self_in) {
    if (xEventGroupGetBits(eth_event_group) & ETHERNET_EVT_CONNECTED) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeth_isconnected_obj, modeth_isconnected);

STATIC const mp_map_elem_t eth_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&modeth_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig),            (mp_obj_t)&eth_ifconfig_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_hostname),            (mp_obj_t)&eth_hostname_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&modeth_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&modeth_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&modeth_isconnected_obj },
#ifdef DEBUG_MODETH
    { MP_OBJ_NEW_QSTR(MP_QSTR_register),            (mp_obj_t)&modeth_ksz8851_reg_wr_obj },
#endif
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
    .inf_up = is_eth_link_up,
    .set_default_inf = eth_set_default_inf
};
