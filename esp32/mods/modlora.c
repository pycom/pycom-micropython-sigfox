/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "bufhelper.h"
#include "mpexception.h"
#include "radio.h"
#include "modnetwork.h"
#include "py/stream.h"
#include "modusocket.h"
#include "pycom_config.h"
#include "mpirq.h"
#include "modlora.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "lwip/sockets.h"       // for the socket error codes

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lora/mac/LoRaMacTest.h"
#include "lora/mac/region/Region.h"
#include "lora/mac/region/RegionAS923.h"
#include "lora/mac/region/RegionAU915.h"
#include "lora/mac/region/RegionUS915.h"
#include "lora/mac/region/RegionUS915-Hybrid.h"
#include "lora/mac/region/RegionEU868.h"
#include "lora/mac/region/RegionCN470.h"
#include "lora/mac/region/RegionIN865.h"
#include "lora/mac/region/RegionEU433.h"

// openThread includes
#ifdef LORA_OPENTHREAD_ENABLED
#include "lora/ot-log.h"
#include "modmesh.h"
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

#include "random.h"
/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define TX_OUTPUT_POWER_MAX                         20          // dBm
#define TX_OUTPUT_POWER_MIN                         2           // dBm

#define LORA_FIX_LENGTH_PAYLOAD_ON                  (true)
#define LORA_FIX_LENGTH_PAYLOAD_OFF                 (false)
#define LORA_TX_TIMEOUT_MAX                         (9000)      // 9 seconds
#define LORA_RX_TIMEOUT                             (0)         // No timeout

// [SF6..SF12]
#define LORA_SPREADING_FACTOR_MIN                   (6)
#define LORA_SPREADING_FACTOR_MAX                   (12)

#define LORA_CHECK_SOCKET(s)                        if (s->sock_base.u.sd < 0) {  \
                                                        *_errno = MP_EBADF;     \
                                                        return -1;              \
                                                    }

#define OVER_THE_AIR_ACTIVATION_DUTYCYCLE           10000  // 10 [s] value in ms

#define DEF_LORAWAN_NETWORK_ID                      0
#define DEF_LORAWAN_APP_PORT                        2

#define LORA_JOIN_WAIT_MS                           (50)

#define LORAWAN_SOCKET_GET_FD(sd)                   (sd & 0xFF)

#define LORAWAN_SOCKET_IS_CONFIRMED(sd)             ((sd & 0x40000000) == 0x40000000)
#define LORAWAN_SOCKET_SET_CONFIRMED(sd)            (sd |= 0x40000000)
#define LORAWAN_SOCKET_CLR_CONFIRMED(sd)            (sd &= ~0x40000000)

#define LORAWAN_SOCKET_SET_PORT(sd, port)           (sd &= 0xFFFF00FF); \
                                                    (sd |= (port << 8))

#define LORAWAN_SOCKET_GET_PORT(sd)                 ((sd >> 8) & 0xFF)


#define LORAWAN_SOCKET_SET_DR(sd, dr)               (sd &= 0xFF00FFFF); \
                                                    (sd |= (dr << 16))

#define LORAWAN_SOCKET_GET_DR(sd)                   ((sd >> 16) & 0xFF)


// callback events
#define MODLORA_RX_EVENT                            (0x01)
#define MODLORA_TX_EVENT                            (0x02)
#define MODLORA_TX_FAILED_EVENT                     (0x04)

#define MODLORA_NVS_NAMESPACE                       "LORA_NVM"

#define MESH_CLI_OUTPUT_SIZE                            (1024)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef enum {
    E_LORA_STACK_MODE_LORA = 0,
    E_LORA_STACK_MODE_LORAWAN
} lora_stack_mode_t;

typedef enum {
    E_LORA_STATE_NOINIT = 0,
    E_LORA_STATE_IDLE,
    E_LORA_STATE_JOIN,
    E_LORA_STATE_LINK_CHECK,
    E_LORA_STATE_RX,
    E_LORA_STATE_RX_DONE,
    E_LORA_STATE_RX_TIMEOUT,
    E_LORA_STATE_RX_ERROR,
    E_LORA_STATE_TX,
    E_LORA_STATE_TX_DONE,
    E_LORA_STATE_TX_TIMEOUT,
    E_LORA_STATE_SLEEP,
    E_LORA_STATE_RESET
} lora_state_t;

typedef enum {
    E_LORA_MODE_ALWAYS_ON = 0,
    E_LORA_MODE_TX_ONLY,
    E_LORA_MODE_SLEEP
} lora_mode_t;

typedef enum {
    E_LORA_BW_125_KHZ = 0,
    E_LORA_BW_250_KHZ = 1,
    E_LORA_BW_500_KHZ = 2
} lora_bandwidth_t;

typedef enum {
    E_LORA_CODING_4_5 = 1,
    E_LORA_CODING_4_6 = 2,
    E_LORA_CODING_4_7 = 3,
    E_LORA_CODING_4_8 = 4
} lora_coding_rate_t;

typedef enum {
    E_LORA_ACTIVATION_OTAA = 0,
    E_LORA_ACTIVATION_ABP
} lora_activation_t;

typedef struct {
    mp_obj_base_t     base;
    mp_obj_t          handler;
    mp_obj_t          handler_arg;
    LoRaMacRegion_t   region;
    lora_stack_mode_t stack_mode;
    DeviceClass_t     device_class;
    lora_state_t      state;
    uint32_t          frequency;
    uint32_t          rx_timestamp;
    uint32_t          net_id;
    uint32_t          tx_time_on_air;
    uint32_t          tx_counter;
    uint32_t          tx_frequency;
    int16_t           rssi;
    int8_t            snr;
    uint8_t           sfrx;
    uint8_t           sftx;
    uint8_t           preamble;
    uint8_t           bandwidth;
    uint8_t           coding_rate;
    uint8_t           sf;
    int8_t            tx_power;
    uint8_t           pwr_mode;

    struct {
        bool Enabled;
        bool Running;
        uint8_t State;
        bool IsTxConfirmed;
        uint16_t DownLinkCounter;
        bool LinkCheck;
        uint8_t DemodMargin;
        uint8_t NbGateways;
    } ComplianceTest;

    uint8_t           activation;
    uint8_t           tx_retries;
    uint8_t           otaa_dr;

    union {
        struct {
            // for OTAA
            uint8_t           DevEui[8];
            uint8_t           AppEui[8];
            uint8_t           AppKey[16];
        } otaa;

        struct {
            // for ABP
            uint32_t          DevAddr;
            uint8_t           NwkSKey[16];
            uint8_t           AppSKey[16];
        } abp;
    } u;

    bool              txiq;
    bool              rxiq;
    bool              adr;
    bool              public;
    bool              joined;
    bool              reset;
    uint8_t           events;
    uint8_t           trigger;
    uint8_t           tx_trials;
} lora_obj_t;

typedef struct {
    uint32_t    index;
    uint32_t    size;
    uint8_t     data[LORA_PAYLOAD_SIZE_MAX];
    uint8_t     port;
} lora_partial_rx_packet_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static QueueHandle_t xCbQueue;
static EventGroupHandle_t LoRaEvents;

static RadioEvents_t RadioEvents;
static lora_cmd_data_t task_cmd_data;
static LoRaMacPrimitives_t LoRaMacPrimitives;
static LoRaMacCallback_t LoRaMacCallbacks;

static lora_obj_t lora_obj;
static lora_partial_rx_packet_t lora_partial_rx_packet;
static lora_rx_data_t rx_data_isr;

static TimerEvent_t TxNextActReqTimer;

static nvs_handle modlora_nvs_handle;
static const char *modlora_nvs_data_key[E_LORA_NVS_NUM_KEYS] = { "JOINED", "UPLNK", "DWLNK", "DEVADDR",
                                                                 "NWSKEY", "APPSKEY", "NETID", "ADRACK",
                                                                 "MACPARAMS", "CHANNELS", "SRVACK", "MACNXTTX",
                                                                 "MACBUFIDX", "MACRPTIDX", "MACBUF", "MACRPTBUF",
                                                                 "REGION", "CHANMASK", "CHANMASKREM" };
/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern TaskHandle_t xLoRaTaskHndl;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LoRa (void *pvParameters);
static void TASK_LoRa_Timer (void *pvParameters);
static void OnTxDone (void);
static void OnRxDone (uint8_t *payload, uint32_t timestamp, uint16_t size, int16_t rssi, int8_t snr, uint8_t sf);
static void OnTxTimeout (void);
static void OnRxTimeout (void);
static void OnRxError (void);
static void lora_radio_setup (lora_init_cmd_data_t *init_data);
static void lora_validate_mode (uint32_t mode);
static void lora_validate_frequency (uint32_t frequency);
static void lora_validate_power (uint8_t tx_power);
static void lora_validate_bandwidth (uint8_t bandwidth);
static void lora_validate_sf (uint8_t sf);
static void lora_validate_coding_rate (uint8_t coding_rate);
static void lora_set_config (lora_cmd_data_t *cmd_data);
static void lora_get_config (lora_cmd_data_t *cmd_data);
static void lora_send_cmd (lora_cmd_data_t *cmd_data);
static int32_t lora_send (const byte *buf, uint32_t len, uint32_t timeout_ms);
static int32_t lora_recv (byte *buf, uint32_t len, int32_t timeout_ms, uint32_t *port);
static bool lora_rx_any (void);
static bool lora_tx_space (void);
static void lora_callback_handler (void *arg);
static bool lorawan_nvs_open (void);

static int lora_socket_socket (mod_network_socket_obj_t *s, int *_errno);
static void lora_socket_close (mod_network_socket_obj_t *s);
static int lora_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
static int lora_socket_recvfrom (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno);
static int lora_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
static int lora_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
static int lora_socket_bind (mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int lora_socket_setsockopt (mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
static int lora_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);
static int lora_socket_sendto (struct _mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno);

static bool lora_lbt_is_free(void);
STATIC mp_obj_t lora_nvram_erase (mp_obj_t self_in);

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
#if defined(FIPY) || defined(LOPY4)
SemaphoreHandle_t xLoRaSigfoxSem;
#endif

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modlora_init0(void) {
    xCmdQueue = xQueueCreate(LORA_CMD_QUEUE_SIZE_MAX, sizeof(lora_cmd_data_t));
    xRxQueue = xQueueCreate(LORA_DATA_QUEUE_SIZE_MAX, sizeof(lora_rx_data_t));
    xCbQueue = xQueueCreate(LORA_CB_QUEUE_SIZE_MAX, sizeof(modlora_timerCallback));
    LoRaEvents = xEventGroupCreate();
#if defined(FIPY) || defined(LOPY4)
    xLoRaSigfoxSem = xSemaphoreCreateMutex();
#endif

    if (!lorawan_nvs_open()) {
        mp_printf(&mp_plat_print, "Error opening LoRa NVS namespace!\n");
    }

    // target board initialisation
    BoardInitMcu();
    BoardInitPeriph();

    xTaskCreatePinnedToCore(TASK_LoRa, "LoRa", LORA_STACK_SIZE / sizeof(StackType_t), NULL, LORA_TASK_PRIORITY, &xLoRaTaskHndl, 1);
    xTaskCreatePinnedToCore(TASK_LoRa_Timer, "LoRa_Timer_callback", LORA_TIMER_STACK_SIZE / sizeof(StackType_t), NULL, LORA_TIMER_TASK_PRIORITY, &xLoRaTimerTaskHndl, 1);
}

bool modlora_nvs_set_uint(uint32_t key_idx, uint32_t value) {
    if (ESP_OK == nvs_set_u32(modlora_nvs_handle, modlora_nvs_data_key[key_idx], value)) {
        return true;
    }
    return false;
}

bool modlora_nvs_set_blob(uint32_t key_idx, const void *value, uint32_t length) {
    if (ESP_OK == nvs_set_blob(modlora_nvs_handle, modlora_nvs_data_key[key_idx], value, length)) {
        return true;
    }
    return false;
}

bool modlora_nvs_get_uint(uint32_t key_idx, uint32_t *value) {
    esp_err_t err;
    if (ESP_OK == (err = nvs_get_u32(modlora_nvs_handle, modlora_nvs_data_key[key_idx], value))) {
        return true;
    }
    return false;
}

bool modlora_nvs_get_blob(uint32_t key_idx, void *value, uint32_t *length) {
    esp_err_t err;
    if (ESP_OK == (err = nvs_get_blob(modlora_nvs_handle, modlora_nvs_data_key[key_idx], value, length))) {
        return true;
    }
    return false;
}

void modlora_sleep_module(void)
{
    lora_cmd_data_t cmd_data;
    /* Set Modem mode to LORA in order to got to Sleep Mode */
    Radio.SetModem(MODEM_LORA);
    cmd_data.cmd = E_LORA_CMD_SLEEP;
    /* Send Sleep Command to Lora Task */
    lora_send_cmd (&cmd_data);
}

bool modlora_is_module_sleep(void)
{
    if (lora_obj.state == E_LORA_STATE_SLEEP)
    {
        return true;
    }
    else
    {
        return false;
    }
}

IRAM_ATTR void modlora_set_timer_callback(modlora_timerCallback cb)
{
    if(cb != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        xQueueSendFromISR(xCbQueue, &cb, &xHigherPriorityTaskWoken);

        if( xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

#ifdef LORA_OPENTHREAD_ENABLED
int lora_ot_recv(uint8_t *buf, int8_t *rssi) {

    // put Lora into RX mode
    int len = lora_recv (buf, OT_RADIO_FRAME_MAX_SIZE, 0, NULL);

    if (len > 0) {

        // put rssi on signed 8bit, saturate at -128dB
        if (lora_obj.rssi < INT8_MIN)
            *rssi = INT8_MIN;
        else
            *rssi = lora_obj.rssi;

        otPlatLog(OT_LOG_LEVEL_INFO, 0, "radio rcv: %d, %d", len, *rssi);
    }
    return len;
}

void lora_ot_send(const uint8_t *buf, uint16_t len) {

    // send max 255 bytes
    len = LORA_PAYLOAD_SIZE_MAX < len ? LORA_PAYLOAD_SIZE_MAX : len;

    lora_send(buf, len, 0);

    //otPlatLog(OT_LOG_LEVEL_INFO, 0, "radio TX: %d", len);
}
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool lorawan_nvs_open (void) {
    if (nvs_open(MODLORA_NVS_NAMESPACE, NVS_READWRITE, &modlora_nvs_handle) != ESP_OK) {
        return false;
    }

    uint32_t data;
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u32(modlora_nvs_handle, "JOINED", &data)) {
        // initialize the value to 0
        nvs_set_u32(modlora_nvs_handle, "JOINED", false);
        if (ESP_OK != nvs_commit(modlora_nvs_handle)) {
            return false;
        }
    }
    return true;
}

static int32_t lorawan_send (const byte *buf, uint32_t len, uint32_t timeout_ms, bool confirmed, uint32_t dr, uint32_t port) {
    lora_cmd_data_t cmd_data;

    cmd_data.cmd = E_LORA_CMD_LORAWAN_TX;
    memcpy (cmd_data.info.tx.data, buf, len);
    cmd_data.info.tx.len = len;
    cmd_data.info.tx.dr = dr;
    if (lora_obj.ComplianceTest.Enabled && lora_obj.ComplianceTest.Running) {
        cmd_data.info.tx.port = 224;  // MAC commands port
        if (lora_obj.ComplianceTest.IsTxConfirmed) {
            cmd_data.info.tx.confirmed = true;
        } else {
            cmd_data.info.tx.confirmed = false;
        }
    } else {
        cmd_data.info.tx.confirmed = confirmed;
        cmd_data.info.tx.port = port;    // data port
    }

    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    xEventGroupClearBits(LoRaEvents, LORA_STATUS_COMPLETED | LORA_STATUS_ERROR | LORA_STATUS_MSG_SIZE);

    // just pass to the LoRa queue
    if (!xQueueSend(xCmdQueue, (void *)&cmd_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        return 0;
    }

    // validate the message size with the requested data rate
    if (false == ValidatePayloadLength(len, dr, 0)) {
        // message too long
        return -1;
    }

    if (timeout_ms != 0) {
        uint32_t result = xEventGroupWaitBits(LoRaEvents,
                                              LORA_STATUS_COMPLETED | LORA_STATUS_ERROR | LORA_STATUS_MSG_SIZE,
                                              pdTRUE,   // clear on exit
                                              pdFALSE,  // do not wait for all bits
                                              (TickType_t)portMAX_DELAY);

        if (result & LORA_STATUS_MSG_SIZE) {
            return -1;
        } else if (result & LORA_STATUS_ERROR) {
            return 0;
        }
    }
    // return the number of bytes sent
    return len;
}

static void McpsConfirm (McpsConfirm_t *McpsConfirm) {
    uint32_t status = LORA_STATUS_COMPLETED;
    if (McpsConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK) {
        // save the values before calling the event handler
        lora_obj.sftx = McpsConfirm->Datarate;
        lora_obj.tx_trials = McpsConfirm->NbRetries;
        lora_obj.tx_time_on_air = McpsConfirm->TxTimeOnAir;
        lora_obj.tx_power = McpsConfirm->TxPower;
        lora_obj.tx_frequency = McpsConfirm->UpLinkFrequency;
        lora_obj.tx_counter = McpsConfirm->UpLinkCounter;

        switch (McpsConfirm->McpsRequest) {
            case MCPS_UNCONFIRMED: {
                lora_obj.events |= MODLORA_TX_EVENT;
                if (lora_obj.trigger & MODLORA_TX_EVENT) {
                    mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
                }
                lora_obj.state = E_LORA_STATE_IDLE;
                xEventGroupSetBits(LoRaEvents, status);
                break;
            }
            case MCPS_CONFIRMED:
                lora_obj.tx_trials = McpsConfirm->NbRetries;
                if (McpsConfirm->AckReceived) {
                    lora_obj.events |= MODLORA_TX_EVENT;
                    if (lora_obj.trigger & MODLORA_TX_EVENT) {
                        mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
                    }
                    lora_obj.state = E_LORA_STATE_IDLE;
                    xEventGroupSetBits(LoRaEvents, status);
                } else {
                    // the ack wasn't received, so the stack will re-transmit
                }
                break;
            case MCPS_PROPRIETARY:
                break;
            default:
                break;
        }
    } else {
        lora_obj.events |= MODLORA_TX_FAILED_EVENT;
        if (lora_obj.trigger & MODLORA_TX_FAILED_EVENT) {
            mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
        }
        lora_obj.state = E_LORA_STATE_IDLE;
        status |= LORA_STATUS_ERROR;
        xEventGroupSetBits(LoRaEvents, status);
    }
#if defined(FIPY) || defined(LOPY4)
    xSemaphoreGive(xLoRaSigfoxSem);
#endif
}

static void McpsIndication (McpsIndication_t *mcpsIndication) {
    bool bDoEcho = true;

    if (mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK) {
        return;
    }

    switch (mcpsIndication->McpsIndication) {
        case MCPS_UNCONFIRMED:
            break;
        case MCPS_CONFIRMED:
            break;
        case MCPS_PROPRIETARY:
            break;
        case MCPS_MULTICAST:
            break;
        default:
            break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot

    lora_obj.rx_timestamp = mcpsIndication->TimeStamp;
    lora_obj.rssi = mcpsIndication->Rssi;
    lora_obj.snr = mcpsIndication->Snr;
    lora_obj.sfrx = mcpsIndication->RxDatarate;

    if ((mcpsIndication->Port == 224) && (lora_obj.ComplianceTest.Enabled == true)
        && (lora_obj.ComplianceTest.Running == true)) {
       MibRequestConfirm_t mibReq;
        mibReq.Type = MIB_CHANNELS_DATARATE;
        LoRaMacMibGetRequestConfirm( &mibReq );
        if (mcpsIndication->Buffer[0] == 4)     { // echo service
            if (ValidatePayloadLength(mcpsIndication->BufferSize, mibReq.Param.ChannelsDatarate, 0)) {
                lora_obj.ComplianceTest.DownLinkCounter++;
            } else {
                // do not increment the downlink counter and don't send the echo either
                bDoEcho = false;
            }
        } else {
            // increament the downlink counter anyhow
            lora_obj.ComplianceTest.DownLinkCounter++;
        }
    }

    // printf("MCPS indication!=%d :%d\n", mcpsIndication->BufferSize, mcpsIndication->Port);

    if (mcpsIndication->RxData && mcpsIndication->BufferSize > 0) {
        if (mcpsIndication->Port > 0 && mcpsIndication->Port < 224) {
            if (mcpsIndication->BufferSize <= LORA_PAYLOAD_SIZE_MAX) {
                memcpy((void *)rx_data_isr.data, mcpsIndication->Buffer, mcpsIndication->BufferSize);
                rx_data_isr.len = mcpsIndication->BufferSize;
                rx_data_isr.port = mcpsIndication->Port;
                xQueueSend(xRxQueue, (void *)&rx_data_isr, 0);
                lora_obj.events |= MODLORA_RX_EVENT;
                if (lora_obj.trigger & MODLORA_RX_EVENT) {
                    mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
                }
            }
            // printf("Data on port 1 or 2 received\n");
        } else if (mcpsIndication->Port == 224) {
            if (lora_obj.ComplianceTest.Enabled == true) {
                if (lora_obj.ComplianceTest.Running == false) {
                    // printf("Checking start test msg\n");
                    // Check compliance test enable command (i)
                    if( ( mcpsIndication->BufferSize == 4 ) &&
                        ( mcpsIndication->Buffer[0] == 0x01 ) &&
                        ( mcpsIndication->Buffer[1] == 0x01 ) &&
                        ( mcpsIndication->Buffer[2] == 0x01 ) &&
                        ( mcpsIndication->Buffer[3] == 0x01 ) )
                    {
                        lora_obj.ComplianceTest.IsTxConfirmed = false;
                        lora_obj.ComplianceTest.DownLinkCounter = 0;
                        lora_obj.ComplianceTest.LinkCheck = false;
                        lora_obj.ComplianceTest.DemodMargin = 0;
                        lora_obj.ComplianceTest.NbGateways = 0;
                        lora_obj.ComplianceTest.Running = true;
                        lora_obj.ComplianceTest.State = 1;

                        // flush the rx queue
                        xQueueReset(xRxQueue);

                        // enable ADR during test mode
                        MibRequestConfirm_t mibReq;
                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = true;
                        LoRaMacMibSetRequestConfirm( &mibReq );

                        // always disable duty cycle limitation during test mode
                        LoRaMacTestSetDutyCycleOn(false);

                        // printf("Compliance enabled\n");
                    }
                } else {
                    lora_obj.ComplianceTest.State = mcpsIndication->Buffer[0];
                    switch (lora_obj.ComplianceTest.State) {
                    case 0: // Check compliance test disable command (ii)
                        lora_obj.ComplianceTest.IsTxConfirmed = false;
                        lora_obj.ComplianceTest.DownLinkCounter = 0;
                        lora_obj.ComplianceTest.Running = false;

                        // set adr back to its original value
                        MibRequestConfirm_t mibReq;
                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = lora_obj.adr;
                        LoRaMacMibSetRequestConfirm(&mibReq);
                        LoRaMacTestSetDutyCycleOn(true);
                        // printf("Compliance disabled\n");
                        break;
                    case 1: // (iii, iv)
                        lora_obj.ComplianceTest.Running = true;
                        // printf("Compliance running\n");
                        break;
                    case 2: // Enable confirmed messages (v)
                        lora_obj.ComplianceTest.IsTxConfirmed = true;
                        lora_obj.ComplianceTest.State = 1;
                        // printf("Confirmed messages enabled\n");
                        break;
                    case 3:  // Disable confirmed messages (vi)
                        lora_obj.ComplianceTest.IsTxConfirmed = false;
                        lora_obj.ComplianceTest.State = 1;
                        // printf("Confirmed messages disabled\n");
                        break;
                    case 4: // (vii)
                        // return the payload
                        if (bDoEcho) {
                            if (mcpsIndication->BufferSize <= LORA_PAYLOAD_SIZE_MAX) {
                                memcpy((void *)rx_data_isr.data, mcpsIndication->Buffer, mcpsIndication->BufferSize);
                                rx_data_isr.len = mcpsIndication->BufferSize;
                                xQueueSend(xRxQueue, (void *)&rx_data_isr, 0);
                            }
                        } else {
                            // set the state back to 1
                            lora_obj.ComplianceTest.State = 1;
                        }
                        // printf("Crypto message received\n");
                        break;
                    case 5: // (viii)
                        // trigger a link check
                        lora_obj.state = E_LORA_STATE_LINK_CHECK;
                        // printf("Link check\n");
                        break;
                    case 6: // (ix)
                        // trigger a join request
                        lora_obj.joined = false;
                        lora_obj.ComplianceTest.State = 6;
                        // printf("Trigger join\n");
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
}

static void MlmeConfirm (MlmeConfirm_t *MlmeConfirm) {
    if (MlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK) {
        switch (MlmeConfirm->MlmeRequest) {
            case MLME_JOIN:
                TimerStop(&TxNextActReqTimer);
                lora_obj.joined = true;
                lora_obj.ComplianceTest.State = 1;
                lora_obj.ComplianceTest.Running = false;
                lora_obj.ComplianceTest.DownLinkCounter = 0;
                break;
            case MLME_LINK_CHECK:
                // Check DemodMargin
                // Check NbGateways
                if (lora_obj.ComplianceTest.Running == true) {
                    lora_obj.ComplianceTest.LinkCheck = true;
                    lora_obj.ComplianceTest.DemodMargin = MlmeConfirm->DemodMargin;
                    lora_obj.ComplianceTest.NbGateways = MlmeConfirm->NbGateways;
                }
                // printf("Link check confirm\n");
                break;
            default:
                break;
        }
    }
#if defined(FIPY) || defined(LOPY4)
    xSemaphoreGive(xLoRaSigfoxSem);
#endif
}

static void OnTxNextActReqTimerEvent(void) {
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm(&mibReq);

    if (status == LORAMAC_STATUS_OK) {
        if (mibReq.Param.IsNetworkJoined == true) {
            lora_obj.joined = true;
            lora_obj.ComplianceTest.State = 1;
            lora_obj.ComplianceTest.Running = false;
            lora_obj.ComplianceTest.DownLinkCounter = 0;
        } else {
            lora_obj.state = E_LORA_STATE_JOIN;
        }
    }
}

static void MlmeIndication( MlmeIndication_t *mlmeIndication )
{
    switch( mlmeIndication->MlmeIndication )
    {
        case MLME_SCHEDULE_UPLINK:
        {// The MAC signals that we shall provide an uplink as soon as possible
            printf("Trying to send uplink\n");
            OnTxNextActReqTimerEvent( );
            break;
        }
        default:
            break;
    }
}

static void TASK_LoRa (void *pvParameters) {
    MibRequestConfirm_t mibReq;
    MlmeReq_t mlmeReq;
    McpsReq_t mcpsReq;
    bool isReset;

    lora_obj.state = E_LORA_STATE_NOINIT;
    lora_obj.pwr_mode = E_LORA_MODE_ALWAYS_ON;

    for ( ; ; ) {
        vTaskDelay (2 / portTICK_PERIOD_MS);

        if(lora_obj.reset)
        {
            Radio.Reset();
            lora_obj.state = E_LORA_STATE_RESET;
            lora_obj.reset = false;
        }
        switch (lora_obj.state) {
        case E_LORA_STATE_NOINIT:
        case E_LORA_STATE_IDLE:
        case E_LORA_STATE_RX:
        case E_LORA_STATE_SLEEP:
        case E_LORA_STATE_RESET:
            // receive from the command queue and act accordingly
            if (xQueueReceive(xCmdQueue, &task_cmd_data, 0)) {
                switch (task_cmd_data.cmd) {
                case E_LORA_CMD_INIT:
                    isReset = lora_obj.state == E_LORA_STATE_RESET? true:false;
                    // save the new configuration first
                    lora_set_config(&task_cmd_data);
                    if (task_cmd_data.info.init.stack_mode == E_LORA_STACK_MODE_LORAWAN) {
                        LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
                        LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
                        LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
                        LoRaMacPrimitives.MacMlmeIndication = MlmeIndication;
                        LoRaMacCallbacks.GetBatteryLevel = BoardGetBatteryLevel;
                        LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks, task_cmd_data.info.init.region);

                        TimerStop(&TxNextActReqTimer);
                        TimerInit(&TxNextActReqTimer, OnTxNextActReqTimerEvent);
                        TimerSetValue(&TxNextActReqTimer, OVER_THE_AIR_ACTIVATION_DUTYCYCLE);

                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = task_cmd_data.info.init.adr;
                        LoRaMacMibSetRequestConfirm(&mibReq);

                        mibReq.Type = MIB_PUBLIC_NETWORK;
                        mibReq.Param.EnablePublicNetwork = task_cmd_data.info.init.public;
                        LoRaMacMibSetRequestConfirm(&mibReq);

                        mibReq.Type = MIB_DEVICE_CLASS;
                        mibReq.Param.Class = task_cmd_data.info.init.device_class;
                        LoRaMacMibSetRequestConfirm(&mibReq);

                        LoRaMacTestSetDutyCycleOn(false);

                        // check if we have already joined the network
                        if (lora_obj.joined) {
                            uint32_t length;
                            bool result = true;
                            result &= modlora_nvs_get_uint(E_LORA_NVS_ELE_NET_ID, (uint32_t *)&lora_obj.net_id);
                            result &= modlora_nvs_get_uint(E_LORA_NVS_ELE_DEVADDR, (uint32_t *)&lora_obj.u.abp.DevAddr);
                            length = 16;
                            result &= modlora_nvs_get_blob(E_LORA_NVS_ELE_NWSKEY, (void *)lora_obj.u.abp.NwkSKey, &length);
                            length = 16;
                            result &= modlora_nvs_get_blob(E_LORA_NVS_ELE_APPSKEY, (void *)lora_obj.u.abp.AppSKey, &length);

                            uint32_t uplinks, downlinks;
                            result &= modlora_nvs_get_uint(E_LORA_NVS_ELE_UPLINK, &uplinks);
                            result &= modlora_nvs_get_uint(E_LORA_NVS_ELE_DWLINK, &downlinks);
                            result &= modlora_nvs_get_uint(E_LORA_NVS_ELE_ADR_ACKS, LoRaMacGetAdrAckCounter());

                            if (result) {
                                mibReq.Type = MIB_UPLINK_COUNTER;
                                mibReq.Param.UpLinkCounter = uplinks;
                                LoRaMacMibSetRequestConfirm( &mibReq );

                                mibReq.Type = MIB_DOWNLINK_COUNTER;
                                mibReq.Param.DownLinkCounter = downlinks;
                                LoRaMacMibSetRequestConfirm( &mibReq );

                                // write the MAC params directly from the NVRAM
                                length = sizeof(LoRaMacParams_t);
                                modlora_nvs_get_blob(E_LORA_NVS_ELE_MAC_PARAMS, (void *)LoRaMacGetMacParams(), &length);

                                // write the channel list directly from the NVRAM
                                ChannelParams_t *channels;
                                LoRaMacGetChannelList(&channels, &length);
                                modlora_nvs_get_blob(E_LORA_NVS_ELE_CHANNELS, channels, &length);

                                // write the channel mask directly from the NVRAM
                                uint16_t *channelmask;
                                if (LoRaMacGetChannelsMask(&channelmask, &length)) {
                                    modlora_nvs_get_blob(E_LORA_NVS_ELE_CHANNELMASK, channelmask, &length);
                                }

                                // write the channel mask remaining directly from the NVRAM
                                if (LoRaMacGetChannelsMaskRemaining(&channelmask, &length)) {
                                    modlora_nvs_get_blob(E_LORA_NVS_ELE_CHANNELMASK_REMAINING, channelmask, &length);
                                }

                                uint32_t srv_ack_req;
                                modlora_nvs_get_uint(E_LORA_NVS_ELE_ACK_REQ, (uint32_t *)&srv_ack_req);
                                bool *ack_req = LoRaMacGetSrvAckRequested();
                                if (srv_ack_req) {
                                    *ack_req = true;
                                } else {
                                    *ack_req = false;
                                }

                                uint32_t mac_cmd_next_tx;
                                modlora_nvs_get_uint(E_LORA_NVS_MAC_NXT_TX, (uint32_t *)&mac_cmd_next_tx);
                                bool *next_tx = LoRaMacGetMacCmdNextTx();
                                if (mac_cmd_next_tx) {
                                    *next_tx = true;
                                } else {
                                    *next_tx = false;
                                }

                                uint32_t mac_cmd_buffer_idx;
                                modlora_nvs_get_uint(E_LORA_NVS_MAC_CMD_BUF_IDX, (uint32_t *)&mac_cmd_buffer_idx);
                                uint8_t *buffer_idx = LoRaMacGetMacCmdBufferIndex();
                                *buffer_idx = mac_cmd_buffer_idx;

                                modlora_nvs_get_uint(E_LORA_NVS_MAC_CMD_BUF_RPT_IDX, (uint32_t *)&mac_cmd_buffer_idx);
                                buffer_idx = LoRaMacGetMacCmdBufferRepeatIndex();
                                *buffer_idx = mac_cmd_buffer_idx;

                                // write the buffered MAC commads directly from NVRAM
                                length = 128;
                                modlora_nvs_get_blob(E_LORA_NVS_ELE_MAC_BUF, (void *)LoRaMacGetMacCmdBuffer(), &length);

                                // write the buffered MAC commads to repeat directly from NVRAM
                                length = 128;
                                modlora_nvs_get_blob(E_LORA_NVS_ELE_MAC_RPT_BUF, (void *)LoRaMacGetMacCmdBufferRepeat(), &length);

                                lora_obj.activation = E_LORA_ACTIVATION_ABP;
                                lora_obj.state = E_LORA_STATE_JOIN;
                                // clear the joined flag until the nvram_save method is called again
                                modlora_nvs_set_uint(E_LORA_NVS_ELE_JOINED, (uint32_t)false);
                            } else {
                                lora_obj.state = E_LORA_STATE_IDLE;
                            }
                        } else {
                            lora_obj.state = E_LORA_STATE_IDLE;
                        }
                    } else {
                        // radio initialization
                        RadioEvents.TxDone = OnTxDone;
                        RadioEvents.RxDone = OnRxDone;
                        RadioEvents.TxTimeout = OnTxTimeout;
                        RadioEvents.RxTimeout = OnRxTimeout;
                        RadioEvents.RxError = OnRxError;
                        Radio.Init(&RadioEvents);

                        // radio configuration
                        lora_radio_setup(&task_cmd_data.info.init);
                        lora_obj.state = E_LORA_STATE_IDLE;
                    }
                    lora_obj.joined = false;
                    if (lora_obj.state == E_LORA_STATE_IDLE) {
                        xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    }
                    if (isReset) {
                        xEventGroupSetBits(LoRaEvents, LORA_STATUS_RESET_DONE);
                    }
                    break;
                case E_LORA_CMD_JOIN:
                    lora_obj.joined = false;
                    lora_obj.activation = task_cmd_data.info.join.activation;
                    if (lora_obj.activation == E_LORA_ACTIVATION_OTAA) {
                        memcpy((void *)lora_obj.u.otaa.DevEui, task_cmd_data.info.join.u.otaa.DevEui, sizeof(lora_obj.u.otaa.DevEui));
                        memcpy((void *)lora_obj.u.otaa.AppEui, task_cmd_data.info.join.u.otaa.AppEui, sizeof(lora_obj.u.otaa.AppEui));
                        memcpy((void *)lora_obj.u.otaa.AppKey, task_cmd_data.info.join.u.otaa.AppKey, sizeof(lora_obj.u.otaa.AppKey));
                        lora_obj.otaa_dr = task_cmd_data.info.join.otaa_dr;
                    } else {
                        lora_obj.net_id = DEF_LORAWAN_NETWORK_ID;
                        lora_obj.u.abp.DevAddr = task_cmd_data.info.join.u.abp.DevAddr;
                        memcpy((void *)lora_obj.u.abp.AppSKey, task_cmd_data.info.join.u.abp.AppSKey, sizeof(lora_obj.u.abp.AppSKey));
                        memcpy((void *)lora_obj.u.abp.NwkSKey, task_cmd_data.info.join.u.abp.NwkSKey, sizeof(lora_obj.u.abp.NwkSKey));
                    }
                    lora_obj.state = E_LORA_STATE_JOIN;
                    break;
                case E_LORA_CMD_TX:
                    // implement Listen-before-Talk LBT, only for LoRa RAW (not LoRaWAN)
                    if (lora_lbt_is_free()) {
                        // no activity detected on Lora, so send the pack now

                        // taking sigfox semaphore blocks ?!?!?
                        // maybe, in the end of TX sempahore has to be released sooner
//                        #if defined(FIPY) || defined(LOPY4)
//                            xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
//                        #endif
                        Radio.Send(task_cmd_data.info.tx.data, task_cmd_data.info.tx.len);
                        lora_obj.state = E_LORA_STATE_TX;
                    } else {
                        // activity detected on Lora, so put the TX command back on queue on Front
                        // to be executed on next Lora task
                        xQueueSendToFront(xCmdQueue, (void *)&task_cmd_data, (TickType_t)portMAX_DELAY);
                    }
                    break;
                case E_LORA_CMD_CONFIG_CHANNEL:
                    if (task_cmd_data.info.channel.add) {
                        ChannelParams_t channel =
                        { task_cmd_data.info.channel.frequency, 0, {((task_cmd_data.info.channel.dr_max << 4) | task_cmd_data.info.channel.dr_min)}, 0};
                        ChannelAddParams_t channelAdd = { &channel, task_cmd_data.info.channel.index };
                        RegionChannelManualAdd(lora_obj.region, &channelAdd);
                    } else {
                        ChannelRemoveParams_t channelRemove = { task_cmd_data.info.channel.index };
                        RegionChannelsManualRemove(lora_obj.region, &channelRemove);
                    }
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    break;
                case E_LORA_CMD_LORAWAN_TX: {
                        LoRaMacTxInfo_t txInfo;
                        EventBits_t status = 0;
                        bool empty_frame = false;
                        int8_t mac_datarate = 0;

                        // set the new data rate before checking if Tx is possible, but store the current one
                        if (!lora_obj.adr) {
                            mibReq.Type = MIB_CHANNELS_DATARATE;
                            LoRaMacMibGetRequestConfirm( &mibReq );
                            mac_datarate = mibReq.Param.ChannelsDatarate;
                            mibReq.Param.ChannelsDatarate = task_cmd_data.info.tx.dr;
                            LoRaMacMibSetRequestConfirm( &mibReq );
                        }

                        if (LoRaMacQueryTxPossible (task_cmd_data.info.tx.len, &txInfo) != LORAMAC_STATUS_OK) {
                            // send an empty frame in order to flush MAC commands
                            mcpsReq.Type = MCPS_UNCONFIRMED;
                            mcpsReq.Req.Unconfirmed.fBuffer = NULL;
                            mcpsReq.Req.Unconfirmed.fBufferSize = 0;
                            mcpsReq.Req.Unconfirmed.Datarate = task_cmd_data.info.tx.dr;
                            empty_frame = true;
                            status |= LORA_STATUS_MSG_SIZE;
                        } else {
                            if (task_cmd_data.info.tx.confirmed) {
                                mcpsReq.Type = MCPS_CONFIRMED;
                                mcpsReq.Req.Confirmed.fPort = task_cmd_data.info.tx.port;
                                mcpsReq.Req.Confirmed.fBuffer = task_cmd_data.info.tx.data;
                                mcpsReq.Req.Confirmed.fBufferSize = task_cmd_data.info.tx.len;
                                mcpsReq.Req.Confirmed.NbTrials = lora_obj.tx_retries + 1;
                                mcpsReq.Req.Confirmed.Datarate = task_cmd_data.info.tx.dr;
                            } else {
                                mcpsReq.Type = MCPS_UNCONFIRMED;
                                mcpsReq.Req.Unconfirmed.fPort = task_cmd_data.info.tx.port;
                                mcpsReq.Req.Unconfirmed.fBuffer = task_cmd_data.info.tx.data;
                                mcpsReq.Req.Unconfirmed.fBufferSize = task_cmd_data.info.tx.len;
                                mcpsReq.Req.Unconfirmed.Datarate = task_cmd_data.info.tx.dr;
                            }
                        }
                    #if defined(FIPY) || defined(LOPY4)
                        xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
                    #endif

                        // set back the original datarate
                        if (!lora_obj.adr) {
                            mibReq.Param.ChannelsDatarate = mac_datarate;
                            LoRaMacMibSetRequestConfirm( &mibReq );
                        }

                        if (LoRaMacMcpsRequest(&mcpsReq) != LORAMAC_STATUS_OK || empty_frame) {
                            // the command has failed, send the response now
                            lora_obj.state = E_LORA_STATE_IDLE;
                            status |= LORA_STATUS_ERROR;
                            xEventGroupSetBits(LoRaEvents, status);
                        #if defined(FIPY) || defined(LOPY4)
                            xSemaphoreGive(xLoRaSigfoxSem);
                        #endif
                        } else {
                            lora_obj.state = E_LORA_STATE_TX;
                        }
                    }
                    break;
                case E_LORA_CMD_SLEEP:
                    Radio.Sleep();
                    lora_obj.state = E_LORA_STATE_SLEEP;
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                #if defined(FIPY) || defined(LOPY4)
                    xSemaphoreGive(xLoRaSigfoxSem);
                #endif
                    break;
                case E_LORA_CMD_WAKE_UP:
                    // just enable the receiver again
                    Radio.Rx(LORA_RX_TIMEOUT);
                    lora_obj.state = E_LORA_STATE_RX;
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                #if defined(FIPY) || defined(LOPY4)
                    xSemaphoreGive(xLoRaSigfoxSem);
                #endif
                    break;
                default:
                    break;
                }
//            } else if (lora_obj.state == E_LORA_STATE_IDLE && lora_obj.stack_mode == E_LORA_STACK_MODE_LORA) {
//                Radio.Rx(LORA_RX_TIMEOUT);
//                lora_obj.state = E_LORA_STATE_RX;
            }
            break;
        case E_LORA_STATE_JOIN:
            TimerStop( &TxNextActReqTimer );
            if (!lora_obj.joined) {
                if (lora_obj.activation == E_LORA_ACTIVATION_OTAA) {
                #if defined(FIPY) || defined(LOPY4)
                    xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
                #endif
                    mibReq.Type = MIB_NETWORK_ACTIVATION;
                    mibReq.Param.NetworkActivation = ACTIVATION_TYPE_OTAA;
                    LoRaMacMibSetRequestConfirm( &mibReq );
                    
                    TimerStart( &TxNextActReqTimer );
                    mlmeReq.Type = MLME_JOIN;
                    mlmeReq.Req.Join.DevEui = (uint8_t *)lora_obj.u.otaa.DevEui;
                    mlmeReq.Req.Join.AppEui = (uint8_t *)lora_obj.u.otaa.AppEui;
                    mlmeReq.Req.Join.AppKey = (uint8_t *)lora_obj.u.otaa.AppKey;
                    mlmeReq.Req.Join.NbTrials = 1;
                    mlmeReq.Req.Join.DR = (uint8_t) lora_obj.otaa_dr;
                    LoRaMacMlmeRequest( &mlmeReq );
                } else {
                    mibReq.Type = MIB_NETWORK_ACTIVATION;
                    mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
                    LoRaMacMibSetRequestConfirm( &mibReq );
                    
                    mibReq.Type = MIB_NET_ID;
                    mibReq.Param.NetID = lora_obj.net_id;
                    LoRaMacMibSetRequestConfirm( &mibReq );

                    mibReq.Type = MIB_DEV_ADDR;
                    mibReq.Param.DevAddr = (uint32_t)lora_obj.u.abp.DevAddr;
                    LoRaMacMibSetRequestConfirm( &mibReq );

                    mibReq.Type = MIB_NWK_SKEY;
                    mibReq.Param.NwkSKey = (uint8_t *)lora_obj.u.abp.NwkSKey;
                    LoRaMacMibSetRequestConfirm( &mibReq );

                    mibReq.Type = MIB_APP_SKEY;
                    mibReq.Param.AppSKey = (uint8_t *)lora_obj.u.abp.AppSKey;
                    LoRaMacMibSetRequestConfirm( &mibReq );

                    mibReq.Type = MIB_NETWORK_JOINED;
                    mibReq.Param.IsNetworkJoined = true;
                    LoRaMacMibSetRequestConfirm( &mibReq );
                    lora_obj.joined = true;
                    lora_obj.ComplianceTest.State = 1;
                }
            }
            xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
            lora_obj.state = E_LORA_STATE_IDLE;
            break;
        case E_LORA_STATE_LINK_CHECK:
            mlmeReq.Type = MLME_LINK_CHECK;
            LoRaMacMlmeRequest(&mlmeReq);
            lora_obj.state = E_LORA_STATE_IDLE;
            break;
        case E_LORA_STATE_RX_DONE:
        case E_LORA_STATE_RX_TIMEOUT:
        case E_LORA_STATE_RX_ERROR:
            // we need to perform a mode transition in order to clear the TxRx FIFO
            Radio.Sleep();
            //lora_obj.state = E_LORA_STATE_IDLE;
            lora_obj.state = E_LORA_STATE_RX;
            Radio.Rx(LORA_RX_TIMEOUT);
            break;
        case E_LORA_STATE_TX:
            break;
        case E_LORA_STATE_TX_DONE:
            // we need to perform a mode transition in order to clear the TxRx FIFO
            Radio.Sleep();
            xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
            //lora_obj.state = E_LORA_STATE_IDLE;
            lora_obj.state = E_LORA_STATE_RX;
            Radio.Rx(LORA_RX_TIMEOUT);
        #if defined(FIPY) || defined(LOPY4)
            xSemaphoreGive(xLoRaSigfoxSem);
        #endif
            break;
        case E_LORA_STATE_TX_TIMEOUT:
            // we need to perform a mode transition in order to clear the TxRx FIFO
            Radio.Sleep();
            xEventGroupSetBits(LoRaEvents, LORA_STATUS_ERROR);
            //lora_obj.state = E_LORA_STATE_IDLE;
            lora_obj.state = E_LORA_STATE_RX;
            Radio.Rx(LORA_RX_TIMEOUT);
        #if defined(FIPY) || defined(LOPY4)
            xSemaphoreGive(xLoRaSigfoxSem);
        #endif
            break;
        default:
            break;
        }

        TimerLowPowerHandler();
    }
}

static void TASK_LoRa_Timer (void *pvParameters) {

    for(;;)
    {
        modlora_timerCallback cb;
        while (pdTRUE == xQueueReceive(xCbQueue, &cb, portMAX_DELAY))
        {
            if(cb != NULL)
            {
                cb();
            }
        }
    }
}

/*! lora_lbt_is_free checks if there is any Lora traffic on the current channel
 * note: if Modem is not in RX, returns true (free spectrum)
 * additionally rssi threshold and timeout_us can be added as function parameters
 * returns true if spectrum is free
 * returns false if spectrum is busy
 */

static bool lora_lbt_is_free(void)
{
    int16_t rssi_th = -85;
    int timeout_us = 1000; //[microsec]
    bool is_free = true;

    int rssi = -1000;
    int rssi_max = -1000;

    uint64_t now = mp_hal_ticks_us_non_blocking();

    do {
        if (RF_RX_RUNNING == Radio.GetStatus()) {
            rssi = Radio.Rssi(MODEM_LORA);
        }
        rssi_max = (rssi > rssi_max) ? rssi : rssi_max;

        if ( rssi_max > rssi_th ) {
            break;
        }
    } while (mp_hal_ticks_us_non_blocking() - now < timeout_us);

    is_free = (rssi_max < rssi_th);

    return is_free;
}

static void lora_callback_handler(void *arg) {
    lora_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {
        mp_call_function_1(self->handler, self->handler_arg);
    }
}

static IRAM_ATTR void OnTxDone (void) {
    lora_obj.events |= MODLORA_TX_EVENT;
    if (lora_obj.trigger & MODLORA_TX_EVENT) {
        mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
    }
    lora_obj.state = E_LORA_STATE_TX_DONE;
}

static IRAM_ATTR void OnRxDone (uint8_t *payload, uint32_t timestamp, uint16_t size, int16_t rssi, int8_t snr, uint8_t sf) {
    lora_obj.rx_timestamp = timestamp;
    lora_obj.rssi = rssi;
    lora_obj.snr = snr;
    lora_obj.sfrx = sf;
    if (size <= LORA_PAYLOAD_SIZE_MAX) {
        memcpy((void *)rx_data_isr.data, payload, size);
        rx_data_isr.len = size;
        xQueueSendFromISR(xRxQueue, (void *)&rx_data_isr, NULL);
    }

    lora_obj.events |= MODLORA_RX_EVENT;
    if (lora_obj.trigger & MODLORA_RX_EVENT) {
        mp_irq_queue_interrupt(lora_callback_handler, (void *)&lora_obj);
    }

    lora_obj.state = E_LORA_STATE_RX_DONE;
}

static IRAM_ATTR void OnTxTimeout (void) {
    lora_obj.state = E_LORA_STATE_TX_TIMEOUT;
}

static IRAM_ATTR void OnRxTimeout (void) {
    lora_obj.state = E_LORA_STATE_RX_TIMEOUT;
}

static IRAM_ATTR void OnRxError (void) {
    lora_obj.state = E_LORA_STATE_RX_ERROR;
}

static void lora_radio_setup (lora_init_cmd_data_t *init_data) {
    uint16_t symbol_to = 8;

    Radio.SetModem(MODEM_LORA);

    if (init_data->public) {
        Radio.Write(REG_LR_SYNCWORD, LORA_MAC_PUBLIC_SYNCWORD);
    } else {
        Radio.Write(REG_LR_SYNCWORD, LORA_MAC_PRIVATE_SYNCWORD);
    }

    Radio.SetChannel(init_data->frequency);

    Radio.SetTxConfig(MODEM_LORA, init_data->tx_power, 0, init_data->bandwidth,
                                  init_data->sf, init_data->coding_rate,
                                  init_data->preamble, LORA_FIX_LENGTH_PAYLOAD_OFF,
                                  true, 0, 0, init_data->txiq, LORA_TX_TIMEOUT_MAX);

    Radio.SetRxConfig(MODEM_LORA, init_data->bandwidth, init_data->sf,
                                  init_data->coding_rate, 0, init_data->preamble,
                                  symbol_to, LORA_FIX_LENGTH_PAYLOAD_OFF,
                                  0, true, 0, 0, init_data->rxiq, true);

    Radio.SetMaxPayloadLength(MODEM_LORA, LORA_PAYLOAD_SIZE_MAX);

    if (init_data->power_mode == E_LORA_MODE_ALWAYS_ON) {
        // start listening
        Radio.Rx(LORA_RX_TIMEOUT);
        lora_obj.state = E_LORA_STATE_RX;
    } else {
        Radio.Sleep();
        lora_obj.state = E_LORA_STATE_SLEEP;
    }
}

static void lora_validate_mode (uint32_t mode) {
    if (mode > E_LORA_STACK_MODE_LORAWAN) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid mode %d", mode));
    }
}

static void lora_validate_frequency (uint32_t frequency) {
    switch (lora_obj.region) {
        case LORAMAC_REGION_AS923:
            if (frequency < 915000000 || frequency > 928000000) {
                goto freq_error;
            }
            break;
        case LORAMAC_REGION_AU915:
            if (frequency < 915000000 || frequency > 928000000) {
                goto freq_error;
            }
            break;
        case LORAMAC_REGION_US915:
            if (frequency < 902000000 || frequency > 928000000) {
                goto freq_error;
            }
            break;
        case LORAMAC_REGION_US915_HYBRID:
            if (frequency < 902000000 || frequency > 928000000) {
                goto freq_error;
            }
            break;
        case LORAMAC_REGION_CN470:
        #if defined(LOPY4)
            if (frequency < 470000000 || frequency > 510000000) {
                goto freq_error;
            }
        #else
            goto freq_error;
        #endif
            break;
        case LORAMAC_REGION_IN865:
            if (frequency < 865000000 || frequency > 867000000) {
                goto freq_error;
            }
            break;
        case LORAMAC_REGION_EU433:
        #if defined(LOPY4)
            if (frequency < 433000000 || frequency > 435000000) { // LoRa 433 - 434
                goto freq_error;
            }
        #else
            goto freq_error;
        #endif
            break;
        case LORAMAC_REGION_EU868:
            if (frequency < 863000000 || frequency > 870000000) {
                goto freq_error;
            }
            break;
        default:
            break;
    }
    return;

freq_error:
    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "frequency %d out of range", frequency));
}

static void lora_validate_channel (uint32_t index) {
    switch (lora_obj.region) {
        case LORAMAC_REGION_AS923:
            if (index >= AS923_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_AU915:
            if (index >= AU915_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_US915:
            if (index >= US915_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_US915_HYBRID:
            if (index >= US915_HYBRID_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_EU868:
            if (index >= EU868_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_CN470:
            if (index >= CN470_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        case LORAMAC_REGION_IN865:
            if (index >= IN865_MAX_NB_CHANNELS) {
                goto channel_error;
            }
            break;
        default:
            break;
    }
    return;

channel_error:
    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "channel %d out of range", index));
}

static void lora_validate_power (uint8_t tx_power) {
    if (tx_power < TX_OUTPUT_POWER_MIN || tx_power > TX_OUTPUT_POWER_MAX) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Tx power %d out of range", tx_power));
    }
}

static bool lora_validate_data_rate (uint32_t data_rate) {

    switch (lora_obj.region) {
    case LORAMAC_REGION_AS923:
    case LORAMAC_REGION_EU868:
    case LORAMAC_REGION_AU915:
    case LORAMAC_REGION_EU433:
    case LORAMAC_REGION_CN470:
    case LORAMAC_REGION_IN865:
        if (data_rate > DR_6) {
            return false;
        }
        break;
    case LORAMAC_REGION_US915:
    case LORAMAC_REGION_US915_HYBRID:
        if (data_rate > DR_4) {
            return false;
        }
        break;
    default:
        break;
    }
    return true;
}

static void lora_validate_bandwidth (uint8_t bandwidth) {
    if (bandwidth > E_LORA_BW_500_KHZ) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "bandwidth %d not supported", bandwidth));
    }
}

static void lora_validate_sf (uint8_t sf) {
    if (sf < LORA_SPREADING_FACTOR_MIN || sf > LORA_SPREADING_FACTOR_MAX) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "spreading factor %d out of range", sf));
    }
}

static void lora_validate_coding_rate (uint8_t coding_rate) {
    if (coding_rate < E_LORA_CODING_4_5 || coding_rate > E_LORA_CODING_4_8) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "coding rate %d out of range", coding_rate));
    }
}

static void lora_validate_power_mode (uint8_t power_mode) {
    if (power_mode < E_LORA_MODE_ALWAYS_ON || power_mode > E_LORA_MODE_SLEEP) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid power mode %d", power_mode));
    }
}

static void lora_validate_device_class (DeviceClass_t device_class) {
    // CLASS_B is not implemented
    if (device_class != CLASS_A && device_class != CLASS_C) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid device_class %d", device_class));
    }
}

static void lora_validate_region (LoRaMacRegion_t region) {
    if (region != LORAMAC_REGION_AS923 && region != LORAMAC_REGION_AU915
        && region != LORAMAC_REGION_EU868 && region != LORAMAC_REGION_US915
        && region != LORAMAC_REGION_IN865
#if defined(LOPY4)
        && region != LORAMAC_REGION_EU433 && region != LORAMAC_REGION_CN470
#endif
        ) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid region %d", region));
    }
}


static void lora_set_config (lora_cmd_data_t *cmd_data) {
    lora_obj.stack_mode = cmd_data->info.init.stack_mode;
    lora_obj.bandwidth = cmd_data->info.init.bandwidth;
    lora_obj.coding_rate = cmd_data->info.init.coding_rate;
    lora_obj.frequency = cmd_data->info.init.frequency;
    lora_obj.preamble = cmd_data->info.init.preamble;
    lora_obj.rxiq = cmd_data->info.init.rxiq;
    lora_obj.txiq = cmd_data->info.init.txiq;
    lora_obj.sf = cmd_data->info.init.sf;
    lora_obj.tx_power = cmd_data->info.init.tx_power;
    lora_obj.pwr_mode = cmd_data->info.init.power_mode;
    lora_obj.adr = cmd_data->info.init.adr;
    lora_obj.public = cmd_data->info.init.public;
    lora_obj.tx_retries = cmd_data->info.init.tx_retries;
    lora_obj.device_class = cmd_data->info.init.device_class;
    lora_obj.region = cmd_data->info.init.region;
}

static void lora_get_config (lora_cmd_data_t *cmd_data) {
    cmd_data->info.init.stack_mode = lora_obj.stack_mode;
    cmd_data->info.init.bandwidth = lora_obj.bandwidth;
    cmd_data->info.init.coding_rate = lora_obj.coding_rate;
    cmd_data->info.init.frequency = lora_obj.frequency;
    cmd_data->info.init.preamble = lora_obj.preamble;
    cmd_data->info.init.rxiq = lora_obj.rxiq;
    cmd_data->info.init.txiq = lora_obj.txiq;
    cmd_data->info.init.sf = lora_obj.sf;
    cmd_data->info.init.tx_power = lora_obj.tx_power;
    cmd_data->info.init.power_mode = lora_obj.pwr_mode;
    cmd_data->info.init.public = lora_obj.public;
    cmd_data->info.init.adr = lora_obj.adr;
    cmd_data->info.init.tx_retries = lora_obj.tx_retries;
    cmd_data->info.init.device_class = lora_obj.device_class;
    cmd_data->info.init.region = lora_obj.region;
}

static void lora_send_cmd (lora_cmd_data_t *cmd_data) {
    xEventGroupClearBits(LoRaEvents, LORA_STATUS_COMPLETED | LORA_STATUS_ERROR | LORA_STATUS_MSG_SIZE);

    xQueueSend(xCmdQueue, (void *)cmd_data, (TickType_t)portMAX_DELAY);

    uint32_t result = xEventGroupWaitBits(LoRaEvents,
                                          LORA_STATUS_COMPLETED | LORA_STATUS_ERROR,
                                          pdTRUE,   // clear on exit
                                          pdFALSE,  // do not wait for all bits
                                          (TickType_t)portMAX_DELAY);

    if (result & LORA_STATUS_ERROR) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
}

static int32_t lora_send (const byte *buf, uint32_t len, uint32_t timeout_ms) {
    lora_cmd_data_t cmd_data;

/*
#if defined(FIPY) || defined(LOPY4)
    xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
    lora_get_config (&cmd_data);
    cmd_data.cmd = E_LORA_CMD_INIT;
    lora_send_cmd (&cmd_data);
    xSemaphoreGive(xLoRaSigfoxSem);
#endif
*/
    cmd_data.cmd = E_LORA_CMD_TX;
    memcpy (cmd_data.info.tx.data, buf, len);
    cmd_data.info.tx.len = len;

    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    xEventGroupClearBits(LoRaEvents, LORA_STATUS_COMPLETED | LORA_STATUS_ERROR | LORA_STATUS_MSG_SIZE);

    // just pass to the LoRa queue
    if (!xQueueSend(xCmdQueue, (void *)&cmd_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        //printf("Q full\n");
        return 0;
    }

    lora_obj.sftx = lora_obj.sf;

    if (timeout_ms != 0) {
        //printf("timeout_ms %d\n", timeout_ms);

        xEventGroupWaitBits(LoRaEvents,
                            LORA_STATUS_COMPLETED | LORA_STATUS_ERROR | LORA_STATUS_MSG_SIZE,
                            pdTRUE,   // clear on exit
                            pdFALSE,  // do not wait for all bits
                            (TickType_t)portMAX_DELAY);
    }

    // calculate the time on air
    lora_obj.tx_time_on_air = Radio.TimeOnAir(MODEM_LORA, len);
    lora_obj.tx_counter += 1;
    lora_obj.tx_frequency = lora_obj.frequency;

    // return the number of bytes sent
    return len;
}

static int32_t lora_recv (byte *buf, uint32_t len, int32_t timeout_ms, uint32_t *port) {
    lora_rx_data_t rx_data;

    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    // if there's a partial packet pending
    if (lora_partial_rx_packet.size > 0) {
        // adjust the len
        uint32_t available_len = lora_partial_rx_packet.size - lora_partial_rx_packet.index;
        if (available_len < len) {
            len = available_len;
        }

        // get the available data
        memcpy(buf, (void *)&lora_partial_rx_packet.data[lora_partial_rx_packet.index], len);
        if (port != NULL) {
            *port = rx_data.port;
        }

        // update the index and size values
        lora_partial_rx_packet.index += len;
        if (lora_partial_rx_packet.index == lora_partial_rx_packet.size) {
            // there's no more data left
            lora_partial_rx_packet.size = 0;
        }
        // return the number of bytes received
        return len;
    } else if (xQueueReceive(xRxQueue, &rx_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        // adjust the len
        if (rx_data.len < len) {
            len = rx_data.len;
        }

        // get the available data
        memcpy(buf, rx_data.data, len);
        if (port != NULL) {
            *port = rx_data.port;
        }

        // copy the remainder to the partial data buffer
        int32_t r_len = rx_data.len - len;
        if (r_len > 0) {
            memcpy((void *)lora_partial_rx_packet.data, &rx_data.data[len], r_len);
            lora_partial_rx_packet.size = r_len;
            lora_partial_rx_packet.index = 0;
            lora_partial_rx_packet.port = rx_data.port;
        }
        // return the number of bytes received
        return len;
    }
    // non-blocking sockects do not thrown timeout errors
    if (timeout_ms == 0) {
        return 0;
    }
    // there's no data available
    return -1;
}

static bool lora_rx_any (void) {
    lora_rx_data_t rx_data;
    if (lora_partial_rx_packet.size > 0) {
        return true;
    } else if (xQueuePeek(xRxQueue, &rx_data, (TickType_t)0)) {
        return true;
    }
    return false;
}

static bool lora_tx_space (void) {
    if (uxQueueSpacesAvailable(xCmdQueue) > 0) {
        return true;
    }
    return false;
}

/******************************************************************************/
// Micro Python bindings; LoRa class

/// \class LoRa - Semtech SX1272 radio driver
static mp_obj_t lora_init_helper(lora_obj_t *self, const mp_arg_val_t *args) {
    lora_cmd_data_t cmd_data;

    cmd_data.info.init.stack_mode = args[0].u_int;
    lora_validate_mode (cmd_data.info.init.stack_mode);

    // we need to know the region first
    if (args[14].u_obj == MP_OBJ_NULL) {
        cmd_data.info.init.region = config_get_lora_region();
        if (cmd_data.info.init.region == 0xff) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "no region specified and no default found in config block"));
        }
    } else {
        cmd_data.info.init.region = mp_obj_get_int(args[14].u_obj);
    }
    lora_validate_region(cmd_data.info.init.region);
    // we need to do it here in advance for the rest of the validation to work
    lora_obj.region = cmd_data.info.init.region;

    if (args[1].u_obj == MP_OBJ_NULL) {
        switch (cmd_data.info.init.region) {
        case LORAMAC_REGION_AS923:
            cmd_data.info.init.frequency = 923000000;
            break;
        case LORAMAC_REGION_AU915:
        case LORAMAC_REGION_US915:
        case LORAMAC_REGION_US915_HYBRID:
            cmd_data.info.init.frequency = 915000000;
            break;
        case LORAMAC_REGION_EU868:
            cmd_data.info.init.frequency = 868000000;
            break;
        case LORAMAC_REGION_EU433:
            cmd_data.info.init.frequency = 433175000;
            break;
        case LORAMAC_REGION_CN470:
            cmd_data.info.init.frequency = 470000000;
        case LORAMAC_REGION_IN865:
            cmd_data.info.init.frequency = 865000000;
        default:
            break;
        }
    } else {
        cmd_data.info.init.frequency = mp_obj_get_int(args[1].u_obj);
        lora_validate_frequency (cmd_data.info.init.frequency);
    }

    if (args[2].u_obj == MP_OBJ_NULL) {
        switch (cmd_data.info.init.region) {
        case LORAMAC_REGION_AS923:
        case LORAMAC_REGION_AU915:
        case LORAMAC_REGION_US915:
        case LORAMAC_REGION_IN865:
        case LORAMAC_REGION_US915_HYBRID:
            cmd_data.info.init.tx_power = 20;
            break;
        case LORAMAC_REGION_CN470:
        case LORAMAC_REGION_EU868:
            cmd_data.info.init.tx_power = 14;
            break;
        case LORAMAC_REGION_EU433:
            cmd_data.info.init.tx_power = 12;
            break;
        default:
            break;
        }
    } else {
        cmd_data.info.init.tx_power = mp_obj_get_int(args[2].u_obj);
        lora_validate_power (cmd_data.info.init.tx_power);
    }

    cmd_data.info.init.bandwidth = args[3].u_int;
    lora_validate_bandwidth (cmd_data.info.init.bandwidth);

    cmd_data.info.init.sf = args[4].u_int;
    lora_validate_sf(cmd_data.info.init.sf);

    cmd_data.info.init.preamble = args[5].u_int;

    cmd_data.info.init.coding_rate = args[6].u_int;
    lora_validate_coding_rate (cmd_data.info.init.coding_rate);

    cmd_data.info.init.power_mode = args[7].u_int;
    lora_validate_power_mode (cmd_data.info.init.power_mode);

    cmd_data.info.init.txiq = args[8].u_bool;
    cmd_data.info.init.rxiq = args[9].u_bool;

    cmd_data.info.init.adr = args[10].u_bool;
    cmd_data.info.init.public = args[11].u_bool;
    cmd_data.info.init.tx_retries = args[12].u_int;

    cmd_data.info.init.device_class = args[13].u_int;
    lora_validate_device_class(cmd_data.info.init.device_class);

    // send message to the lora task
    cmd_data.cmd = E_LORA_CMD_INIT;
    lora_send_cmd(&cmd_data);

    return mp_const_none;
}

STATIC const mp_arg_t lora_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_STACK_MODE_LORA} },
    { MP_QSTR_frequency,    MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
    { MP_QSTR_tx_power,     MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
    { MP_QSTR_bandwidth,    MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_BW_125_KHZ} },
    { MP_QSTR_sf,           MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = 7} },
    { MP_QSTR_preamble,     MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = 8} },
    { MP_QSTR_coding_rate,  MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_CODING_4_5} },
    { MP_QSTR_power_mode,   MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_MODE_ALWAYS_ON} },
    { MP_QSTR_tx_iq,        MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_rx_iq,        MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_adr,          MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_public,       MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true} },
    { MP_QSTR_tx_retries,   MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int = 2} },
    { MP_QSTR_device_class, MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int = CLASS_A} },
    { MP_QSTR_region,       MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t lora_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lora_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), lora_init_args, args);

    // setup the object
    lora_obj_t *self = (lora_obj_t *)&lora_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_lora;

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // run the constructor if the peripehral is not initialized or extra parameters are given
    if (n_kw > 0 || self->state == E_LORA_STATE_NOINIT) {
        // start the peripheral
        lora_init_helper(self, &args[1]);
        // register it as a network card
        mod_network_register_nic(self);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t lora_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lora_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &lora_init_args[1], args);
    return lora_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_init_obj, 1, lora_init);

STATIC mp_obj_t lora_join(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_activation,     MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_auth,           MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, },
        { MP_QSTR_dr,             MP_ARG_KW_ONLY  | MP_ARG_OBJ,                         {.u_obj = mp_const_none}},
        { MP_QSTR_timeout,        MP_ARG_KW_ONLY  | MP_ARG_OBJ,                         {.u_obj = mp_const_none} },
    };
    lora_cmd_data_t cmd_data;

    // check for the correct lora radio mode
    if (lora_obj.stack_mode != E_LORA_STACK_MODE_LORAWAN) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // get the activation type
    uint32_t activation = args[0].u_int;
    if (activation > E_LORA_ACTIVATION_ABP) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid activation type %d", activation));
    }

    // get the auth details
    mp_obj_t *auth;
    mp_buffer_info_t bufinfo_0, bufinfo_1, bufinfo_2;
    if (activation == E_LORA_ACTIVATION_OTAA) {
        uint32_t auth_len;
        mp_obj_get_array(args[1].u_obj, &auth_len, &auth);
        if (auth_len == 2) {
            mp_get_buffer_raise(auth[0], &bufinfo_1, MP_BUFFER_READ);
            mp_get_buffer_raise(auth[1], &bufinfo_2, MP_BUFFER_READ);
            config_get_lpwan_mac(cmd_data.info.join.u.otaa.DevEui);
        } else {
            mp_get_buffer_raise(auth[0], &bufinfo_0, MP_BUFFER_READ);
            memcpy(cmd_data.info.join.u.otaa.DevEui, bufinfo_0.buf, sizeof(cmd_data.info.join.u.otaa.DevEui));
            mp_get_buffer_raise(auth[1], &bufinfo_1, MP_BUFFER_READ);
            mp_get_buffer_raise(auth[2], &bufinfo_2, MP_BUFFER_READ);
        }
        memcpy(cmd_data.info.join.u.otaa.AppEui, bufinfo_1.buf, sizeof(cmd_data.info.join.u.otaa.AppEui));
        memcpy(cmd_data.info.join.u.otaa.AppKey, bufinfo_2.buf, sizeof(cmd_data.info.join.u.otaa.AppKey));
    } else {
        mp_obj_get_array_fixed_n(args[1].u_obj, 3, &auth);
        mp_get_buffer_raise(auth[1], &bufinfo_0, MP_BUFFER_READ);
        mp_get_buffer_raise(auth[2], &bufinfo_1, MP_BUFFER_READ);
        cmd_data.info.join.u.abp.DevAddr = mp_obj_int_get_truncated(auth[0]);
        memcpy(cmd_data.info.join.u.abp.NwkSKey, bufinfo_0.buf, sizeof(cmd_data.info.join.u.abp.NwkSKey));
        memcpy(cmd_data.info.join.u.abp.AppSKey, bufinfo_1.buf, sizeof(cmd_data.info.join.u.abp.AppSKey));
    }

    // need a way to indicate an invalid data rate so the default approach is used
    uint32_t dr = DR_0;
    switch (lora_obj.region) {
    case LORAMAC_REGION_AS923:
        dr = DR_2;
        break;
    case LORAMAC_REGION_AU915:
        dr = DR_6;
        break;
    case LORAMAC_REGION_US915:
    case LORAMAC_REGION_US915_HYBRID:
        dr = DR_4;
        break;
    case LORAMAC_REGION_CN470:
    case LORAMAC_REGION_EU868:
    case LORAMAC_REGION_EU433:
    case LORAMAC_REGION_IN865:
        dr = DR_5;
        break;
    default:
        break;
    }

    // get the data rate
    if (args[2].u_obj != mp_const_none) {
        dr = mp_obj_get_int(args[2].u_obj);
        switch (lora_obj.region) {
        case LORAMAC_REGION_AS923:
            if (dr != DR_2) {
                goto dr_error;
            }
            break;
        case LORAMAC_REGION_AU915:
            if (dr != DR_0 && dr != DR_6) {
                goto dr_error;
            }
            break;
        case LORAMAC_REGION_US915:
            if (dr != DR_0 && dr != DR_4) {
                goto dr_error;
            }
            break;
        case LORAMAC_REGION_US915_HYBRID:
            if (dr != DR_0 && dr != DR_4) {
                goto dr_error;
            }
            break;
        case LORAMAC_REGION_EU433:
        case LORAMAC_REGION_CN470:
        case LORAMAC_REGION_EU868:
            if (dr > DR_5) {
                goto dr_error;
            }
            break;
        default:
            break;
        }
    }
    cmd_data.info.join.otaa_dr = dr;

    // get the timeout
    int32_t timeout = INT32_MAX;
    if (args[3].u_obj != mp_const_none) {
        timeout = mp_obj_get_int(args[3].u_obj);
    }

    // send a join request message
    cmd_data.info.join.activation = activation;
    cmd_data.cmd = E_LORA_CMD_JOIN;
    lora_send_cmd(&cmd_data);

    if (timeout > 0) {
        while (!lora_obj.joined && timeout >= 0) {
            mp_hal_delay_ms(LORA_JOIN_WAIT_MS);
            timeout -= LORA_JOIN_WAIT_MS;
        }
        if (timeout <= 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TimeoutError, "timed out"));
        }
    }
    return mp_const_none;

dr_error:
    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid join data rate %d", dr));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_join_obj, 1, lora_join);

STATIC mp_obj_t lora_join_multicast_group (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mcAddress,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_int  = -1}         },
        { MP_QSTR_mcNwkKey,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mcAppKey,     MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    
    mp_buffer_info_t bufinfo_0, bufinfo_1;
    mp_get_buffer_raise(args[1].u_obj, &bufinfo_0, MP_BUFFER_READ);
    mp_get_buffer_raise(args[2].u_obj, &bufinfo_1, MP_BUFFER_READ);
    
    MulticastParams_t *channelParam = m_new_obj(MulticastParams_t);
    channelParam->Next = NULL;
    channelParam->DownLinkCounter = 0;
    channelParam->Address = args[0].u_int;
    memcpy(channelParam->NwkSKey, bufinfo_0.buf, sizeof(channelParam->NwkSKey));
    memcpy(channelParam->AppSKey, bufinfo_1.buf, sizeof(channelParam->AppSKey));
    
    if (LoRaMacMulticastChannelLink(channelParam) == LORAMAC_STATUS_OK) {
        return mp_const_true;
    }
            
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_join_multicast_group_obj, 0, lora_join_multicast_group);

STATIC mp_obj_t lora_leave_multicast_group (mp_obj_t self_in, mp_obj_t multicast_addr_obj) {
    uint32_t mcAddr = mp_obj_get_int(multicast_addr_obj);
    MulticastParams_t *channelParam = LoRaMacMulticastGetChannel(mcAddr);
    if (LoRaMacMulticastChannelUnlink(channelParam) == LORAMAC_STATUS_OK) {
        m_del_obj(MulticastParams_t, channelParam);
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lora_leave_multicast_group_obj, lora_leave_multicast_group);

STATIC mp_obj_t lora_compliance_test(mp_uint_t n_args, const mp_obj_t *args) {
    // get
    if (n_args == 1) {
        static const qstr lora_compliance_info_fields[] = {
            MP_QSTR_enabled, MP_QSTR_running, MP_QSTR_state, MP_QSTR_tx_confirmed,
            MP_QSTR_downlink_counter, MP_QSTR_link_check, MP_QSTR_demod_margin,
            MP_QSTR_nbr_gateways
        };

        mp_obj_t compliance_tuple[8];
        compliance_tuple[0] = mp_obj_new_bool(lora_obj.ComplianceTest.Enabled);
        compliance_tuple[1] = mp_obj_new_bool(lora_obj.ComplianceTest.Running);
        compliance_tuple[2] = mp_obj_new_int(lora_obj.ComplianceTest.State);
        compliance_tuple[3] = mp_obj_new_bool(lora_obj.ComplianceTest.IsTxConfirmed);
        compliance_tuple[4] = mp_obj_new_int(lora_obj.ComplianceTest.DownLinkCounter);
        compliance_tuple[5] = mp_obj_new_bool(lora_obj.ComplianceTest.LinkCheck);
        compliance_tuple[6] = mp_obj_new_int(lora_obj.ComplianceTest.DemodMargin);
        compliance_tuple[7] = mp_obj_new_int(lora_obj.ComplianceTest.NbGateways);

        return mp_obj_new_attrtuple(lora_compliance_info_fields, 8, compliance_tuple);
    } else {    // set
        if (mp_obj_is_true(args[1])) {  // enable or disable
            lora_obj.ComplianceTest.Enabled = true;
        } else {
            lora_obj.ComplianceTest.Enabled = false;
            lora_obj.ComplianceTest.IsTxConfirmed = false;
            lora_obj.ComplianceTest.DownLinkCounter = 0;
            lora_obj.ComplianceTest.Running = false;

            // set adr back to its original value
            MibRequestConfirm_t mibReq;
            mibReq.Type = MIB_ADR;
            mibReq.Param.AdrEnable = lora_obj.adr;
            LoRaMacMibSetRequestConfirm(&mibReq);
        }

        if (n_args > 2) {
            // state
            lora_obj.ComplianceTest.State = mp_obj_get_int(args[2]);

            if (n_args > 3) {
                // link check
                if (mp_obj_is_true(args[3])) {
                    lora_obj.ComplianceTest.LinkCheck = true;
                } else {
                    lora_obj.ComplianceTest.LinkCheck = false;
                }
            }
        }

        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_compliance_test_obj, 1, 4, lora_compliance_test);

STATIC mp_obj_t lora_tx_power (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->tx_power);
    } else {
        lora_cmd_data_t cmd_data;
        uint8_t power = mp_obj_get_int(args[1]);
        lora_validate_power(power);
        lora_get_config (&cmd_data);
        cmd_data.info.init.tx_power = power;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_tx_power_obj, 1, 2, lora_tx_power);

STATIC mp_obj_t lora_coding_rate (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->coding_rate);
    } else {
        lora_cmd_data_t cmd_data;
        uint8_t coding_rate = mp_obj_get_int(args[1]);
        lora_validate_coding_rate(coding_rate);
        lora_get_config (&cmd_data);
        cmd_data.info.init.coding_rate = coding_rate;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_coding_rate_obj, 1, 2, lora_coding_rate);

STATIC mp_obj_t lora_preamble (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->preamble);
    } else {
        lora_cmd_data_t cmd_data;
        uint8_t preamble = mp_obj_get_int(args[1]);
        lora_get_config (&cmd_data);
        cmd_data.info.init.preamble = preamble;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_preamble_obj, 1, 2, lora_preamble);

STATIC mp_obj_t lora_bandwidth (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->bandwidth);
    } else {
        lora_cmd_data_t cmd_data;
        uint8_t bandwidth = mp_obj_get_int(args[1]);
        lora_validate_bandwidth(bandwidth);
        lora_get_config (&cmd_data);
        cmd_data.info.init.bandwidth = bandwidth;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_bandwidth_obj, 1, 2, lora_bandwidth);

STATIC mp_obj_t lora_frequency (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->frequency);
    } else {
        lora_cmd_data_t cmd_data;
        uint32_t frequency = mp_obj_get_int(args[1]);
        lora_validate_frequency(frequency);
        lora_get_config (&cmd_data);
        cmd_data.info.init.frequency = frequency;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_frequency_obj, 1, 2, lora_frequency);

STATIC mp_obj_t lora_sf (mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    if (n_args == 1) {
        return mp_obj_new_int(self->sf);
    } else {
        lora_cmd_data_t cmd_data;
        uint8_t sf = mp_obj_get_int(args[1]);
        lora_validate_sf(sf);
        lora_get_config (&cmd_data);
        cmd_data.info.init.sf = sf;
        cmd_data.cmd = E_LORA_CMD_INIT;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_sf_obj, 1, 2, lora_sf);

STATIC mp_obj_t lora_power_mode(mp_uint_t n_args, const mp_obj_t *args) {
    lora_obj_t *self = args[0];
    lora_cmd_data_t cmd_data;

    if (n_args == 1) {
        return mp_obj_new_int(self->pwr_mode);
    } else {
        uint8_t pwr_mode = mp_obj_get_int(args[1]);
        lora_validate_power_mode(pwr_mode);
        if (pwr_mode == E_LORA_MODE_ALWAYS_ON) {
            cmd_data.cmd = E_LORA_CMD_WAKE_UP;
        } else {
            cmd_data.cmd = E_LORA_CMD_SLEEP;
        }
        self->pwr_mode = pwr_mode;
        lora_send_cmd (&cmd_data);
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lora_power_mode_obj, 1, 2, lora_power_mode);

STATIC mp_obj_t lora_stats(mp_obj_t self_in) {
    lora_obj_t *self = self_in;
    float snr;

    static const qstr lora_stats_info_fields[] = {
        MP_QSTR_rx_timestamp, MP_QSTR_rssi, MP_QSTR_snr, MP_QSTR_sfrx, MP_QSTR_sftx,
        MP_QSTR_tx_trials, MP_QSTR_tx_power, MP_QSTR_tx_time_on_air, MP_QSTR_tx_counter,
        MP_QSTR_tx_frequency
    };

    if (self->snr & 0x80)  { // the SNR sign bit is 1
        // invert and divide by 4
        snr = ((~self->snr + 1 ) & 0xFF) / 4;
        snr = -snr;
    } else {
        // divide by 4
        snr = (self->snr & 0xFF) / 4;
    }

    mp_obj_t stats_tuple[10];
    stats_tuple[0] = mp_obj_new_int_from_uint(self->rx_timestamp);
    stats_tuple[1] = mp_obj_new_int(self->rssi);
    stats_tuple[2] = mp_obj_new_float(snr);
    stats_tuple[3] = mp_obj_new_int(self->sfrx);
    stats_tuple[4] = mp_obj_new_int(self->sftx);
    stats_tuple[5] = mp_obj_new_int(self->tx_trials);
    stats_tuple[6] = mp_obj_new_int(self->tx_power);
    stats_tuple[7] = mp_obj_new_int(self->tx_time_on_air);
    stats_tuple[8] = mp_obj_new_int(self->tx_counter);
    stats_tuple[9] = mp_obj_new_int(self->tx_frequency);

    return mp_obj_new_attrtuple(lora_stats_info_fields, sizeof(stats_tuple) / sizeof(stats_tuple[0]), stats_tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_stats_obj, lora_stats);

STATIC mp_obj_t lora_has_joined(mp_obj_t self_in) {
    lora_obj_t *self = self_in;
    return self->joined ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_has_joined_obj, lora_has_joined);

STATIC mp_obj_t lora_add_channel (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_index,        MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_frequency,    MP_ARG_REQUIRED | MP_ARG_KW_ONLY  | MP_ARG_INT },
        { MP_QSTR_dr_min,       MP_ARG_REQUIRED | MP_ARG_KW_ONLY  | MP_ARG_INT },
        { MP_QSTR_dr_max,       MP_ARG_REQUIRED | MP_ARG_KW_ONLY  | MP_ARG_INT },
    };
    lora_cmd_data_t cmd_data;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint32_t index = args[0].u_int;
    lora_validate_channel(index);

    uint32_t frequency = args[1].u_int;
    uint32_t dr_min = args[2].u_int;
    uint32_t dr_max = args[3].u_int;
    if (dr_min > dr_max || !lora_validate_data_rate(dr_min) || !lora_validate_data_rate(dr_max)) {
        goto error;
    }

    cmd_data.info.channel.index = index;
    cmd_data.info.channel.frequency = frequency;
    cmd_data.info.channel.dr_min = dr_min;
    cmd_data.info.channel.dr_max = dr_max;
    cmd_data.info.channel.add = true;
    cmd_data.cmd = E_LORA_CMD_CONFIG_CHANNEL;
    lora_send_cmd (&cmd_data);

    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_add_channel_obj, 1, lora_add_channel);

STATIC mp_obj_t lora_remove_channel (mp_obj_t self_in, mp_obj_t idx) {
    lora_cmd_data_t cmd_data;

    uint32_t index = mp_obj_get_int(idx);
    lora_validate_channel(index);

    cmd_data.info.channel.index = index;
    cmd_data.info.channel.add = false;
    cmd_data.cmd = E_LORA_CMD_CONFIG_CHANNEL;
    lora_send_cmd (&cmd_data);

    // return the number of bytes written
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lora_remove_channel_obj, lora_remove_channel);

STATIC mp_obj_t lora_mac(mp_obj_t self_in) {
    uint8_t mac[8];
    config_get_lpwan_mac(mac);
    return mp_obj_new_bytes((const byte *)mac, sizeof(mac));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_mac_obj, lora_mac);

/// \method callback(trigger, handler, arg)
STATIC mp_obj_t lora_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    lora_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        self->trigger = mp_obj_get_int(args[0].u_obj);
        self->handler = args[1].u_obj;
        mp_irq_add(self, args[1].u_obj);
        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    } else {
        self->trigger = 0;
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_callback_obj, 1, lora_callback);

STATIC mp_obj_t lora_events(mp_obj_t self_in) {
    lora_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_events_obj, lora_events);

STATIC mp_obj_t lora_ischannel_free(mp_obj_t self_in, mp_obj_t rssi, mp_obj_t time_ms) {
    lora_obj_t *self = self_in;

    // probably we could listen for 2 symbols (bits) for the current Lora settings (freq, bw, sf)
    if (Radio.IsChannelFree(MODEM_LORA, self->frequency, mp_obj_get_int(rssi), mp_obj_get_int(time_ms))) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lora_ischannel_free_obj, lora_ischannel_free);

STATIC mp_obj_t lora_set_battery_level(mp_obj_t self_in, mp_obj_t battery) {
    BoardSetBatteryLevel(mp_obj_get_int(battery));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lora_set_battery_level_obj, lora_set_battery_level);

STATIC mp_obj_t lora_nvram_save (mp_obj_t self_in) {
    LoRaMacRegion_t region = 0xFF;
    modlora_nvs_get_uint(E_LORA_NVS_ELE_REGION, &region);
    // if the region doesn't match, erase the previous stored data
    if (region != lora_obj.region) {
        lora_nvram_erase(NULL);
    }
    LoRaMacNvsSave();
    modlora_nvs_set_uint(E_LORA_NVS_ELE_REGION, (uint32_t)lora_obj.region);
    modlora_nvs_set_uint(E_LORA_NVS_ELE_JOINED, (uint32_t)lora_obj.joined);
    if (ESP_OK != nvs_commit(modlora_nvs_handle)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_nvram_save_obj, lora_nvram_save);

STATIC mp_obj_t lora_nvram_restore (mp_obj_t self_in) {
    uint32_t joined = 0;
    LoRaMacRegion_t region;
    lora_cmd_data_t cmd_data;

    if (modlora_nvs_get_uint(E_LORA_NVS_ELE_JOINED, &joined)) {
        lora_obj.joined = joined;
        if (joined) {
            if (modlora_nvs_get_uint(E_LORA_NVS_ELE_REGION, &region)) {
                // only restore from NVRAM if the region matches
                if (region == lora_obj.region) {
                    lora_get_config (&cmd_data);
                    cmd_data.cmd = E_LORA_CMD_INIT;
                    lora_send_cmd (&cmd_data);
                } else {
                    // erase the previous NVRAM data
                    lora_nvram_erase(NULL);
                    lora_obj.joined = false;
                }
            }
        }
    } else {
        lora_obj.joined = false;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_nvram_restore_obj, lora_nvram_restore);

STATIC mp_obj_t lora_nvram_erase (mp_obj_t self_in) {
    if (ESP_OK != nvs_erase_all(modlora_nvs_handle)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    nvs_commit(modlora_nvs_handle);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_nvram_erase_obj, lora_nvram_erase);

// return time-on-air (milisec) for the current Lora settings, specifying pack_len
STATIC mp_obj_t lora_airtime (mp_obj_t self_in, mp_obj_t pack_len_obj) {
    int len = mp_obj_get_int(pack_len_obj);
    return mp_obj_new_int(Radio.TimeOnAir(MODEM_LORA, len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lora_airtime_obj, lora_airtime);

STATIC mp_obj_t lora_reset (mp_obj_t self_in) {

    lora_obj_t* self = (lora_obj_t*)self_in;
    lora_cmd_data_t cmd_data;

    MP_THREAD_GIL_EXIT();
    //Reset Command Queue
    while(!xQueueReset(xCmdQueue))
    {
        // Try again
        vTaskDelay (100 / portTICK_PERIOD_MS);
    }

    self->reset = true;

    lora_get_config (&cmd_data);
    cmd_data.cmd = E_LORA_CMD_INIT;
    lora_send_cmd (&cmd_data);

    xEventGroupWaitBits(LoRaEvents,
                                  LORA_STATUS_RESET_DONE,
                                  pdTRUE,   // clear on exit
                                  pdTRUE,  // do not wait for all bits
                                  (TickType_t)portMAX_DELAY);

    MP_THREAD_GIL_ENTER();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_reset_obj, lora_reset);


STATIC const mp_map_elem_t lora_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                  (mp_obj_t)&lora_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_join),                  (mp_obj_t)&lora_join_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_join_multicast_group),  (mp_obj_t)&lora_join_multicast_group_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_leave_multicast_group), (mp_obj_t)&lora_leave_multicast_group_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tx_power),              (mp_obj_t)&lora_tx_power_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bandwidth),             (mp_obj_t)&lora_bandwidth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_frequency),             (mp_obj_t)&lora_frequency_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_coding_rate),           (mp_obj_t)&lora_coding_rate_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_preamble),              (mp_obj_t)&lora_preamble_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sf),                    (mp_obj_t)&lora_sf_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_power_mode),            (mp_obj_t)&lora_power_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stats),                 (mp_obj_t)&lora_stats_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_has_joined),            (mp_obj_t)&lora_has_joined_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_channel),           (mp_obj_t)&lora_add_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remove_channel),        (mp_obj_t)&lora_remove_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                   (mp_obj_t)&lora_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_compliance_test),       (mp_obj_t)&lora_compliance_test_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),              (mp_obj_t)&lora_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),                (mp_obj_t)&lora_events_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ischannel_free),        (mp_obj_t)&lora_ischannel_free_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_battery_level),     (mp_obj_t)&lora_set_battery_level_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvram_save),            (mp_obj_t)&lora_nvram_save_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvram_restore),         (mp_obj_t)&lora_nvram_restore_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_nvram_erase),           (mp_obj_t)&lora_nvram_erase_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_airtime),               (mp_obj_t)&lora_airtime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),                 (mp_obj_t)&lora_reset_obj },

#ifdef LORA_OPENTHREAD_ENABLED
    { MP_OBJ_NEW_QSTR(MP_QSTR_Mesh),                (mp_obj_t)&lora_mesh_type },
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    // exceptions
    { MP_OBJ_NEW_QSTR(MP_QSTR_timeout),             (mp_obj_t)&mp_type_TimeoutError },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_LORA),                MP_OBJ_NEW_SMALL_INT(E_LORA_STACK_MODE_LORA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LORAWAN),             MP_OBJ_NEW_SMALL_INT(E_LORA_STACK_MODE_LORAWAN) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_OTAA),                MP_OBJ_NEW_SMALL_INT(E_LORA_ACTIVATION_OTAA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ABP),                 MP_OBJ_NEW_SMALL_INT(E_LORA_ACTIVATION_ABP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ALWAYS_ON),           MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_ALWAYS_ON) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_ONLY),             MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_TX_ONLY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SLEEP),               MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_SLEEP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_125KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_125_KHZ) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_250KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_250_KHZ) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_500KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_500_KHZ) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_5),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_5) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_6),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_6) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_7),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_7) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_8),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_8) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_RX_PACKET_EVENT),     MP_OBJ_NEW_SMALL_INT(MODLORA_RX_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_PACKET_EVENT),     MP_OBJ_NEW_SMALL_INT(MODLORA_TX_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_FAILED_EVENT),     MP_OBJ_NEW_SMALL_INT(MODLORA_TX_FAILED_EVENT) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_CLASS_A),             MP_OBJ_NEW_SMALL_INT(CLASS_A) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLASS_C),             MP_OBJ_NEW_SMALL_INT(CLASS_C) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_AS923),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_AS923) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AU915),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_AU915) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EU868),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_EU868) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_US915),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_US915) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CN470),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_CN470) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IN865),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_IN865) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EU433),               MP_OBJ_NEW_SMALL_INT(LORAMAC_REGION_EU433) },
};

STATIC MP_DEFINE_CONST_DICT(lora_locals_dict, lora_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_lora = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_LoRa,
        .make_new = lora_make_new,
        .locals_dict = (mp_obj_t)&lora_locals_dict,
     },

    .n_socket = lora_socket_socket,
    .n_close = lora_socket_close,
    .n_send = lora_socket_send,
    .n_sendto = lora_socket_sendto,
    .n_recv = lora_socket_recv,
    .n_recvfrom = lora_socket_recvfrom,
    .n_settimeout = lora_socket_settimeout,
    .n_setsockopt = lora_socket_setsockopt,
    .n_bind = lora_socket_bind,
    .n_ioctl = lora_socket_ioctl,
};

///******************************************************************************/
//// Micro Python bindings; LoRa socket

static int lora_socket_socket (mod_network_socket_obj_t *s, int *_errno) {
    if (lora_obj.state == E_LORA_STATE_NOINIT) {
        *_errno = MP_ENETDOWN;
        return -1;
    }
    s->sock_base.u.sd = 1;

#ifdef LORA_OPENTHREAD_ENABLED
    // if mesh is enabled, assume socket is for mesh, not for LoraWAN
    if (lora_mesh_ready()) {
        return mesh_socket_open(s, _errno);
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    uint32_t dr = DR_0;
    switch (lora_obj.region) {
    case LORAMAC_REGION_AS923:
    case LORAMAC_REGION_EU868:
    case LORAMAC_REGION_EU433:
    case LORAMAC_REGION_CN470:
        dr = DR_5;
        break;
    case LORAMAC_REGION_AU915:
    case LORAMAC_REGION_US915:
    case LORAMAC_REGION_US915_HYBRID:
        dr = DR_4;
        break;
    default:
        break;
    }
    LORAWAN_SOCKET_SET_DR(s->sock_base.u.sd, dr);

    // port number 2 is the default one
    LORAWAN_SOCKET_SET_PORT(s->sock_base.u.sd, 2);
    return 0;
}

static void lora_socket_close (mod_network_socket_obj_t *s) {
    s->sock_base.u.sd = -1;
#ifdef LORA_OPENTHREAD_ENABLED
    mesh_socket_close(s);
#endif  // #ifdef LORA_OPENTHREAD_ENABLED
}

static int lora_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    mp_int_t n_bytes = -1;

    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
            *_errno = MP_EOPNOTSUPP;
            return -1;
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    // is the radio able to transmit
    if (lora_obj.pwr_mode == E_LORA_MODE_SLEEP) {
        *_errno = MP_ENETDOWN;
    } else if (len > LORA_PAYLOAD_SIZE_MAX) {
        *_errno = MP_EMSGSIZE;
    } else if (len > 0) {
        if (lora_obj.stack_mode == E_LORA_STACK_MODE_LORA) {
            n_bytes = lora_send (buf, len, s->sock_base.timeout);
        } else {
            if (lora_obj.joined) {
                n_bytes = lorawan_send (buf, len, s->sock_base.timeout,
                                        LORAWAN_SOCKET_IS_CONFIRMED(s->sock_base.u.sd),
                                        LORAWAN_SOCKET_GET_DR(s->sock_base.u.sd),
                                        LORAWAN_SOCKET_GET_PORT(s->sock_base.u.sd));
            } else {
                *_errno = MP_ENETDOWN;
                return -1;
            }
        }
        if (n_bytes == 0) {
            *_errno = MP_EAGAIN;
            n_bytes = -1;
        } else if (n_bytes < 0) {
            *_errno = MP_EMSGSIZE;
        }
    } else {
        n_bytes = 0;
    }
    return n_bytes;
}

static int lora_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    int ret = lora_recv (buf, len, s->sock_base.timeout, NULL);
    if (ret < 0) {
        *_errno = MP_EAGAIN;
        return -1;
    }
    return ret;
}

static int lora_socket_recvfrom (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        return mesh_socket_recvfrom(s, buf, len, ip, port, _errno);
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    *port = 0;      // in case there's no data received
    int ret = lora_recv (buf, len, s->sock_base.timeout, (uint32_t *)port);
    if (ret < 0) {
        *_errno = MP_EAGAIN;
        return -1;
    }
    return ret;
}

static int lora_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    if (level != SOL_LORA) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }

    if (opt == SO_LORAWAN_CONFIRMED) {
        if (*(uint8_t *)optval) {
            LORAWAN_SOCKET_SET_CONFIRMED(s->sock_base.u.sd);
        } else {
            LORAWAN_SOCKET_CLR_CONFIRMED(s->sock_base.u.sd);
        }
    } else if (opt == SO_LORAWAN_DR) {
        if (!lora_validate_data_rate(*(uint32_t *)optval)) {
            *_errno = MP_EOPNOTSUPP;
            return -1;
        }
        LORAWAN_SOCKET_SET_DR(s->sock_base.u.sd, *(uint8_t *)optval);
    } else {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
    return 0;
}

static int lora_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    s->sock_base.timeout = timeout_ms;
    return 0;
}

static int lora_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        return mesh_socket_bind(s, ip, port, _errno);
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    if (port > 224) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
    LORAWAN_SOCKET_SET_PORT(s->sock_base.u.sd, port);
    return 0;
}

static int lora_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret = 0;

    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        if ((flags & MP_STREAM_POLL_RD) && lora_rx_any()) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && lora_tx_space()) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else {
        *_errno = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

static int lora_socket_sendto(struct _mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port, int *_errno) {
    LORA_CHECK_SOCKET(s);

#ifdef LORA_OPENTHREAD_ENABLED
    if (lora_mesh_ready()) {
        return mesh_socket_sendto(s, buf, len, ip, port, _errno);
    }
#endif  // #ifdef LORA_OPENTHREAD_ENABLED

    // not implemented for LoraWAN
    *_errno = MP_EOPNOTSUPP;
    return -1;
}
