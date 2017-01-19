/*
 * Copyright (c) 2016, Pycom Limited.
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

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "bufhelper.h"
#include "mpexception.h"
#include "modlora.h"
#include "board.h"
#include "radio.h"
#include "modnetwork.h"
#include "pybioctl.h"
#include "modusocket.h"
#include "pycom_config.h"

#include "lora/mac/LoRaMac.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "lwip/sockets.h"       // for the socket error codes

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lora/mac/LoRaMacTest.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#if defined(USE_BAND_868)
#define RF_FREQUENCY_MIN                            863000000   // Hz
#define RF_FREQUENCY_CENTER                         868000000   // Hz
#define RF_FREQUENCY_MAX                            870000000   // Hz
#define TX_OUTPUT_POWER_MAX                         14          // dBm
#define TX_OUTPUT_POWER_MIN                         2           // dBm
#elif defined(USE_BAND_915)
#define RF_FREQUENCY_MIN                            902000000   // Hz
#define RF_FREQUENCY_CENTER                         915000000   // Hz
#define RF_FREQUENCY_MAX                            928000000   // Hz
#define TX_OUTPUT_POWER_MAX                         20          // dBm
#define TX_OUTPUT_POWER_MIN                         5           // dBm
#else
    #error "Please define a frequency band in the compiler options."
#endif

#define LORA_SYMBOL_TIMEOUT                         (5)         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  (true)
#define LORA_FIX_LENGTH_PAYLOAD_OFF                 (false)
#define LORA_TX_TIMEOUT_MAX                         (9500)      // 9.5 seconds
#define LORA_RX_TIMEOUT                             (0)         // No timeout

// [SF7..SF12]
#define LORA_SPREADING_FACTOR_MIN                   (7)
#define LORA_SPREADING_FACTOR_MAX                   (12)

#define LORA_CHECK_SOCKET(s)                        if (s->sock_base.sd < 0) {  \
                                                        *_errno = EBADF;        \
                                                        return -1;              \
                                                    }

#define OVER_THE_AIR_ACTIVATION_DUTYCYCLE           15000  // 15 [s] value in ms

#if defined( USE_BAND_868 )
#define LC4                                         { 867100000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC5                                         { 867300000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC6                                         { 867500000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC7                                         { 867700000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC8                                         { 867900000, { ( ( DR_5 << 4 ) | DR_0 ) }, 0 }
#define LC9                                         { 868800000, { ( ( DR_7 << 4 ) | DR_7 ) }, 2 }
#define LC10                                        { 868300000, { ( ( DR_6 << 4 ) | DR_6 ) }, 1 }
#endif

#define DEF_LORAWAN_NETWORK_ID                      0
#define DEF_LORAWAN_APP_PORT                        2

#define LORA_JOIN_WAIT_MS                           (50)

#define LORAWAN_SOCKET_GET_FD(sd)                   (sd & 0xFF)

#define LORAWAN_SOCKET_IS_CONFIRMED(sd)             ((sd & 0x80000000) == 0x80000000)
#define LORAWAN_SOCKET_SET_CONFIRMED(sd)            (sd |= 0x80000000)
#define LORAWAN_SOCKET_CLR_CONFIRMED(sd)            (sd &= ~0x80000000)

#define LORAWAN_SOCKET_SET_PORT(sd, port)           (sd &= 0xFFFF00FF); \
                                                    (sd |= (port << 8))

#define LORAWAN_SOCKET_GET_PORT(sd)                 ((sd >> 8) & 0xFF)


#define LORAWAN_SOCKET_SET_DR(sd, dr)               (sd &= 0xFF00FFFF); \
                                                    (sd |= (dr << 16))

#define LORAWAN_SOCKET_GET_DR(sd)                   ((sd >> 16) & 0xFF)

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
    E_LORA_STATE_SLEEP
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
  lora_stack_mode_t stack_mode;
  lora_state_t      state;
  uint32_t          frequency;
  uint32_t          rx_timeout;
  uint32_t          tx_timeout;
  uint32_t          dev_addr;
  int16_t           rssi;
  uint8_t           preamble;
  uint8_t           bandwidth;
  uint8_t           coding_rate;
  uint8_t           sf;
  uint8_t           tx_power;
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
  };
  bool              txiq;
  bool              rxiq;
  bool              adr;
  bool              public;
  bool              joined;
} lora_obj_t;

typedef struct {
    uint32_t    index;
    uint32_t    size;
    uint8_t     data[LORA_PAYLOAD_SIZE_MAX];
} lora_partial_rx_packet_t;


/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static EventGroupHandle_t LoRaEvents;

static RadioEvents_t RadioEvents;

static volatile lora_obj_t lora_obj;
static volatile lora_partial_rx_packet_t lora_partial_rx_packet;
static volatile lora_rx_data_t rx_data_isr;

static TimerEvent_t TxNextActReqTimer;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LoRa (void *pvParameters);
static void OnTxDone (void);
static void OnRxDone (uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout (void);
static void OnRxTimeout (void);
static void OnRxError (void);
static void lora_setup (lora_init_cmd_data_t *init_data);
static void lora_validate_mode (uint32_t mode);
static void lora_validate_frequency (uint32_t frequency);
static void lora_validate_power (uint8_t tx_power);
static void lora_validate_bandwidth (uint8_t bandwidth);
static void lora_validate_sf (uint8_t sf);
static void lora_validate_coding_rate (uint8_t coding_rate);
static void lora_get_config (lora_cmd_data_t *cmd_data);
static void lora_send_cmd (lora_cmd_data_t *cmd_data);
static int32_t lora_send (const byte *buf, uint32_t len, uint32_t timeout_ms);
static int32_t lora_recv (byte *buf, uint32_t len, int32_t timeout_ms);
static bool lora_rx_any (void);
static bool lora_tx_space (void);

static int lora_socket_socket (mod_network_socket_obj_t *s, int *_errno);
static void lora_socket_close (mod_network_socket_obj_t *s);
static int lora_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
static int lora_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
static int lora_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
static int lora_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);
static int lora_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
static int lora_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modlora_init0(void) {
    xCmdQueue = xQueueCreate(LORA_CMD_QUEUE_SIZE_MAX, sizeof(lora_cmd_data_t));
    xRxQueue = xQueueCreate(LORA_DATA_QUEUE_SIZE_MAX, sizeof(lora_rx_data_t));
    LoRaEvents = xEventGroupCreate();

    xTaskCreate(TASK_LoRa, "LoRa", LORA_STACK_SIZE / sizeof(StackType_t), NULL, LORA_TASK_PRIORITY, NULL);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
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

    // just pass to the LoRa queue
    if (!xQueueSend(xCmdQueue, (void *)&cmd_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        return 0;
    }

    if (timeout_ms != 0) {
        uint32_t result = xEventGroupWaitBits(LoRaEvents,
                                              LORA_STATUS_COMPLETED | LORA_STATUS_ERROR,
                                              pdTRUE,   // clear on exit
                                              pdFALSE,  // do not wait for all bits
                                              (TickType_t)portMAX_DELAY);
        if (result & LORA_STATUS_ERROR) {
            return -1;
        }
    }
    // return the number of bytes sent
    return len;
}

static IRAM_ATTR void McpsConfirm (McpsConfirm_t *McpsConfirm) {
    uint32_t status = LORA_STATUS_COMPLETED;
    if (McpsConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK) {
        switch (McpsConfirm->McpsRequest) {
            case MCPS_UNCONFIRMED:
                // Check Datarate
                // Check TxPower
                break;
            case MCPS_CONFIRMED:
                // Check Datarate
                // Check TxPower
                // Check AckReceived
                // Check NbTrials
                break;
            case MCPS_PROPRIETARY:
                break;
            default:
                break;
        }
    } else {
        status |= LORA_STATUS_ERROR;
    }
    lora_obj.state = E_LORA_STATE_IDLE;
    xEventGroupSetBits(LoRaEvents, status);
}

static void McpsIndication (McpsIndication_t *mcpsIndication) {
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

    lora_obj.rssi = mcpsIndication->Rssi;

    if (lora_obj.ComplianceTest.Running == true) {
        lora_obj.ComplianceTest.DownLinkCounter++;
    }

    // printf("MCPS indication!=%d :%d\n", mcpsIndication->BufferSize, mcpsIndication->Port);

    if (mcpsIndication->RxData) {
        switch (mcpsIndication->Port) {
        case 1:
        case 2:
            if (mcpsIndication->BufferSize <= LORA_PAYLOAD_SIZE_MAX) {
                memcpy((void *)rx_data_isr.data, mcpsIndication->Buffer, mcpsIndication->BufferSize);
                rx_data_isr.len = mcpsIndication->BufferSize;
                xQueueSendFromISR(xRxQueue, (void *)&rx_data_isr, NULL);
            }
            // printf("Data on port 2 received\n");
            break;
        case 224:
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

                    #if defined(USE_BAND_868)
                        // always disable duty cycle limitation during test mode
                        LoRaMacTestSetDutyCycleOn(false);
                    #endif
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
                    #if defined( USE_BAND_868 )
                        LoRaMacTestSetDutyCycleOn(true);
                    #endif
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
                        if (mcpsIndication->BufferSize <= LORA_PAYLOAD_SIZE_MAX) {
                            memcpy((void *)rx_data_isr.data, mcpsIndication->Buffer, mcpsIndication->BufferSize);
                            rx_data_isr.len = mcpsIndication->BufferSize;
                            xQueueSendFromISR(xRxQueue, (void *)&rx_data_isr, NULL);
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
                        lora_obj.ComplianceTest.State = 6;
                        lora_obj.state = E_LORA_STATE_JOIN;
                        // printf("Trigger join\n");
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        default:
            break;
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
}

static void OnTxNextActReqTimerEvent(void) {
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;

    TimerStop(&TxNextActReqTimer);

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm(&mibReq);

    if (status == LORAMAC_STATUS_OK) {
        if (mibReq.Param.IsNetworkJoined == true) {
            lora_obj.joined = true;
        } else {
            lora_obj.state = E_LORA_STATE_JOIN;
        }
    }
}

static void TASK_LoRa (void *pvParameters) {
    lora_cmd_data_t cmd_data;
    lora_obj.state = E_LORA_STATE_NOINIT;
    lora_obj.pwr_mode = E_LORA_MODE_ALWAYS_ON;

    LoRaMacPrimitives_t LoRaMacPrimitives;
    LoRaMacCallback_t LoRaMacCallbacks;
    MibRequestConfirm_t mibReq;
    MlmeReq_t mlmeReq;

    // target board initialisation
    BoardInitMcu();
    BoardInitPeriph();

    for ( ; ; ) {
        vTaskDelay (2 / portTICK_PERIOD_MS);

        switch (lora_obj.state) {
        case E_LORA_STATE_NOINIT:
        case E_LORA_STATE_IDLE:
        case E_LORA_STATE_RX:
        case E_LORA_STATE_SLEEP:
            // receive from the command queue and act accordingly
            if (xQueueReceive(xCmdQueue, &cmd_data, 0)) {
                switch (cmd_data.cmd) {
                case E_LORA_CMD_INIT:
                    lora_obj.stack_mode = cmd_data.info.init.stack_mode;
                    if (cmd_data.info.init.stack_mode == E_LORA_STACK_MODE_LORAWAN) {
                        LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
                        LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
                        LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
                        LoRaMacCallbacks.GetBatteryLevel = BoardGetBatteryLevel;
                        LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks);

                        TimerInit(&TxNextActReqTimer, OnTxNextActReqTimerEvent);

                        mibReq.Type = MIB_ADR;
                        mibReq.Param.AdrEnable = cmd_data.info.init.adr;
                        LoRaMacMibSetRequestConfirm(&mibReq);

                        mibReq.Type = MIB_PUBLIC_NETWORK;
                        mibReq.Param.EnablePublicNetwork = cmd_data.info.init.public;
                        LoRaMacMibSetRequestConfirm(&mibReq);

                        // copy the configuration (must be done before sending the response)
                        lora_obj.adr = cmd_data.info.init.adr;
                        lora_obj.public = cmd_data.info.init.public;
                        lora_obj.tx_retries = cmd_data.info.init.tx_retries;
                        lora_obj.frequency = RF_FREQUENCY_CENTER;
                        lora_obj.state = E_LORA_STATE_IDLE;
                    } else {
                        // radio initialization
                        RadioEvents.TxDone = OnTxDone;
                        RadioEvents.RxDone = OnRxDone;
                        RadioEvents.TxTimeout = OnTxTimeout;
                        RadioEvents.RxTimeout = OnRxTimeout;
                        RadioEvents.RxError = OnRxError;
                        Radio.Init(&RadioEvents);

                        lora_setup(&cmd_data.info.init);
                        // copy the configuration (must be done before sending the response)
                        lora_obj.bandwidth = cmd_data.info.init.bandwidth;
                        lora_obj.coding_rate = cmd_data.info.init.coding_rate;
                        lora_obj.frequency = cmd_data.info.init.frequency;
                        lora_obj.preamble = cmd_data.info.init.preamble;
                        lora_obj.rxiq = cmd_data.info.init.rxiq;
                        lora_obj.txiq = cmd_data.info.init.txiq;
                        lora_obj.sf = cmd_data.info.init.sf;
                        lora_obj.tx_power = cmd_data.info.init.tx_power;
                        lora_obj.pwr_mode = cmd_data.info.init.power_mode;
                        if (lora_obj.pwr_mode == E_LORA_MODE_ALWAYS_ON) {
                            // start listening
                            Radio.Rx(LORA_RX_TIMEOUT);
                            lora_obj.state = E_LORA_STATE_RX;
                        } else {
                            Radio.Sleep();
                            lora_obj.state = E_LORA_STATE_SLEEP;
                        }
                    }
                    lora_obj.joined = false;
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    break;
                case E_LORA_CMD_JOIN:
                    lora_obj.joined = false;
                    lora_obj.activation = cmd_data.info.join.activation;
                    if (lora_obj.activation == E_LORA_ACTIVATION_OTAA) {
                        memcpy((void *)lora_obj.otaa.DevEui, cmd_data.info.join.otaa.DevEui, sizeof(lora_obj.otaa.DevEui));
                        memcpy((void *)lora_obj.otaa.AppEui, cmd_data.info.join.otaa.AppEui, sizeof(lora_obj.otaa.AppEui));
                        memcpy((void *)lora_obj.otaa.AppKey, cmd_data.info.join.otaa.AppKey, sizeof(lora_obj.otaa.AppKey));
                    } else {
                        lora_obj.abp.DevAddr = cmd_data.info.join.abp.DevAddr;
                        memcpy((void *)lora_obj.abp.AppSKey, cmd_data.info.join.abp.AppSKey, sizeof(lora_obj.abp.AppSKey));
                        memcpy((void *)lora_obj.abp.NwkSKey, cmd_data.info.join.abp.NwkSKey, sizeof(lora_obj.abp.NwkSKey));
                    }
                    lora_obj.state = E_LORA_STATE_JOIN;
                    break;
                case E_LORA_CMD_TX:
                    Radio.Send(cmd_data.info.tx.data, cmd_data.info.tx.len);
                    lora_obj.state = E_LORA_STATE_TX;
                    break;
                case E_LORA_CMD_CONFIG_CHANNEL:
                    if (cmd_data.info.channel.add) {
                        ChannelParams_t channel =
                        { cmd_data.info.channel.frequency, {((cmd_data.info.channel.dr_max << 4) | cmd_data.info.channel.dr_min)}, 0};
                        LoRaMacChannelManualAdd(cmd_data.info.channel.index, channel);
                    } else {
                        LoRaMacChannelManualRemove(cmd_data.info.channel.index);
                    }
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    break;
                case E_LORA_CMD_LORAWAN_TX:
                    {
                        McpsReq_t mcpsReq;
                        LoRaMacTxInfo_t txInfo;
                        bool empty_frame = false;

                        if (LoRaMacQueryTxPossible (cmd_data.info.tx.len, &txInfo) != LORAMAC_STATUS_OK) {
                            // send an empty frame in order to flush MAC commands
                            mcpsReq.Type = MCPS_UNCONFIRMED;
                            mcpsReq.Req.Unconfirmed.fBuffer = NULL;
                            mcpsReq.Req.Unconfirmed.fBufferSize = 0;
                            mcpsReq.Req.Unconfirmed.Datarate = cmd_data.info.tx.dr;
                            empty_frame = true;
                        } else {
                            if (cmd_data.info.tx.confirmed) {
                                mcpsReq.Type = MCPS_CONFIRMED;
                                mcpsReq.Req.Confirmed.fPort = cmd_data.info.tx.port;
                                mcpsReq.Req.Confirmed.fBuffer = cmd_data.info.tx.data;
                                mcpsReq.Req.Confirmed.fBufferSize = cmd_data.info.tx.len;
                                mcpsReq.Req.Confirmed.NbTrials = lora_obj.tx_retries + 1;
                                mcpsReq.Req.Confirmed.Datarate = cmd_data.info.tx.dr;
                            } else {
                                mcpsReq.Type = MCPS_UNCONFIRMED;
                                mcpsReq.Req.Unconfirmed.fPort = cmd_data.info.tx.port;
                                mcpsReq.Req.Unconfirmed.fBuffer = cmd_data.info.tx.data;
                                mcpsReq.Req.Unconfirmed.fBufferSize = cmd_data.info.tx.len;
                                mcpsReq.Req.Unconfirmed.Datarate = cmd_data.info.tx.dr;
                            }
                        }

                        if (LoRaMacMcpsRequest(&mcpsReq) != LORAMAC_STATUS_OK || empty_frame) {
                            // the command has failed, send the response now
                            lora_obj.state = E_LORA_STATE_IDLE;
                            xEventGroupSetBits(LoRaEvents, LORA_STATUS_ERROR);
                        } else {
                            lora_obj.state = E_LORA_STATE_TX;
                        }
                    }
                    break;
                case E_LORA_CMD_SLEEP:
                    Radio.Sleep();
                    lora_obj.state = E_LORA_STATE_SLEEP;
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    break;
                case E_LORA_CMD_WAKE_UP:
                    // just enable the receiver again
                    Radio.Rx(LORA_RX_TIMEOUT);
                    lora_obj.state = E_LORA_STATE_RX;
                    xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
                    break;
                default:
                    break;
                }
            } else if (lora_obj.state == E_LORA_STATE_IDLE && lora_obj.stack_mode == E_LORA_STACK_MODE_LORA) {
                Radio.Rx(LORA_RX_TIMEOUT);
                lora_obj.state = E_LORA_STATE_RX;
            }
            break;
        case E_LORA_STATE_JOIN:
            TimerStop( &TxNextActReqTimer );
            if (lora_obj.activation == E_LORA_ACTIVATION_OTAA) {
                mlmeReq.Type = MLME_JOIN;
                mlmeReq.Req.Join.DevEui = (uint8_t *)lora_obj.otaa.DevEui;
                mlmeReq.Req.Join.AppEui = (uint8_t *)lora_obj.otaa.AppEui;
                mlmeReq.Req.Join.AppKey = (uint8_t *)lora_obj.otaa.AppKey;
                LoRaMacMlmeRequest(&mlmeReq);
                TimerSetValue( &TxNextActReqTimer, OVER_THE_AIR_ACTIVATION_DUTYCYCLE);
                TimerStart( &TxNextActReqTimer );
            } else {
                mibReq.Type = MIB_NET_ID;
                mibReq.Param.NetID = DEF_LORAWAN_NETWORK_ID;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_DEV_ADDR;
                mibReq.Param.DevAddr = (uint32_t)lora_obj.abp.DevAddr;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NWK_SKEY;
                mibReq.Param.NwkSKey = (uint8_t *)lora_obj.abp.NwkSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_APP_SKEY;
                mibReq.Param.AppSKey = (uint8_t *)lora_obj.abp.AppSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NETWORK_JOINED;
                mibReq.Param.IsNetworkJoined = true;
                LoRaMacMibSetRequestConfirm( &mibReq );
                lora_obj.joined = true;
                lora_obj.ComplianceTest.State = 1;
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
            lora_obj.state = E_LORA_STATE_IDLE;
            break;
        case E_LORA_STATE_TX:
            break;
        case E_LORA_STATE_TX_DONE:
            // we need to perform a mode transition in order to clear the TxRx FIFO
            Radio.Sleep();
            xEventGroupSetBits(LoRaEvents, LORA_STATUS_COMPLETED);
            lora_obj.state = E_LORA_STATE_IDLE;
            break;
        case E_LORA_STATE_TX_TIMEOUT:
            // we need to perform a mode transition in order to clear the TxRx FIFO
            Radio.Sleep();
            xEventGroupSetBits(LoRaEvents, LORA_STATUS_ERROR);
            lora_obj.state = E_LORA_STATE_IDLE;
            break;
        default:
            break;
        }
        TimerLowPowerHandler();
    }
}

static IRAM_ATTR void OnTxDone (void) {
    lora_obj.state = E_LORA_STATE_TX_DONE;
}

static IRAM_ATTR void OnRxDone (uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
    lora_obj.state = E_LORA_STATE_RX_DONE;
    lora_obj.rssi = rssi;
    if (size <= LORA_PAYLOAD_SIZE_MAX) {
        memcpy((void *)rx_data_isr.data, payload, size);
        rx_data_isr.len = size;
        xQueueSendFromISR(xRxQueue, (void *)&rx_data_isr, NULL);
    }
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

static void lora_setup (lora_init_cmd_data_t *init_data) {
    Radio.SetChannel(init_data->frequency);

    Radio.SetTxConfig(MODEM_LORA, init_data->tx_power, 0, init_data->bandwidth,
                                  init_data->sf, init_data->coding_rate,
                                  init_data->preamble, LORA_FIX_LENGTH_PAYLOAD_OFF,
                                  true, 0, 0, init_data->txiq, LORA_TX_TIMEOUT_MAX);

    Radio.SetRxConfig(MODEM_LORA, init_data->bandwidth, init_data->sf,
                                  init_data->coding_rate, 0, init_data->preamble,
                                  LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_OFF,
                                  0, true, 0, 0, init_data->rxiq, true);
}

static void lora_validate_mode (uint32_t mode) {
    if (mode > E_LORA_STACK_MODE_LORAWAN) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid mode %d", mode));
    }
}

static void lora_validate_frequency (uint32_t frequency) {
    if (frequency < RF_FREQUENCY_MIN || frequency > RF_FREQUENCY_MAX) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "frequency %d out of range", frequency));
    }
}

static void lora_validate_power (uint8_t tx_power) {
    if (tx_power < TX_OUTPUT_POWER_MIN || tx_power > TX_OUTPUT_POWER_MAX) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Tx power %d out of range", tx_power));
    }
}

static bool lora_validate_data_rate (uint32_t data_rate) {
#if defined(USE_BAND_868)
    if (data_rate > DR_7) {
        return false;
    }
#else
    if (data_rate > DR_4) {
        return false;
    }
#endif
    return true;
}

static void lora_validate_bandwidth (uint8_t bandwidth) {
#if defined(USE_BAND_868)
    if (bandwidth > E_LORA_BW_250_KHZ) {
#else
    if (bandwidth != E_LORA_BW_125_KHZ && bandwidth != E_LORA_BW_500_KHZ) {
#endif
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

static void lora_get_config (lora_cmd_data_t *cmd_data) {
    cmd_data->info.init.bandwidth = lora_obj.bandwidth;
    cmd_data->info.init.coding_rate = lora_obj.coding_rate;
    cmd_data->info.init.frequency = lora_obj.frequency;
    cmd_data->info.init.preamble = lora_obj.preamble;
    cmd_data->info.init.rxiq = lora_obj.rxiq;
    cmd_data->info.init.txiq = lora_obj.txiq;
    cmd_data->info.init.sf = lora_obj.sf;
    cmd_data->info.init.tx_power = lora_obj.tx_power;
    cmd_data->info.init.power_mode = lora_obj.pwr_mode;
}

static void lora_send_cmd (lora_cmd_data_t *cmd_data) {
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

    cmd_data.cmd = E_LORA_CMD_TX;
    memcpy (cmd_data.info.tx.data, buf, len);
    cmd_data.info.tx.len = len;

    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    // just pass to the LoRa queue
    if (!xQueueSend(xCmdQueue, (void *)&cmd_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        return 0;
    }

    if (timeout_ms != 0) {
        xEventGroupWaitBits(LoRaEvents,
                            LORA_STATUS_COMPLETED | LORA_STATUS_ERROR,
                            pdTRUE,   // clear on exit
                            pdFALSE,  // do not wait for all bits
                            (TickType_t)portMAX_DELAY);
    }
    // return the number of bytes sent
    return len;
}

static int32_t lora_recv (byte *buf, uint32_t len, int32_t timeout_ms) {
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

        // copy the remainder to the partial data buffer
        int32_t r_len = rx_data.len - len;
        if (r_len > 0) {
            memcpy((void *)lora_partial_rx_packet.data, &rx_data.data[len], r_len);
            lora_partial_rx_packet.size = r_len;
            lora_partial_rx_packet.index = 0;
        }
        // return the number of bytes received
        return len;
    }
    // non-blocking sockects do not thrown timeout error
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

    cmd_data.info.init.frequency = args[1].u_int;
    lora_validate_frequency (cmd_data.info.init.frequency);

    cmd_data.info.init.tx_power = args[2].u_int;
    lora_validate_power (cmd_data.info.init.tx_power);

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
    cmd_data.info.init.tx_retries = args[11].u_int;

    // send message to the lora task
    cmd_data.cmd = E_LORA_CMD_INIT;
    lora_send_cmd(&cmd_data);

    return mp_const_none;
}

STATIC const mp_arg_t lora_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_STACK_MODE_LORA} },
    { MP_QSTR_frequency,    MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = RF_FREQUENCY_CENTER} },
    { MP_QSTR_tx_power,     MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = TX_OUTPUT_POWER_MAX} },
    { MP_QSTR_bandwidth,    MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_BW_125_KHZ} },
    { MP_QSTR_sf,           MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = LORA_SPREADING_FACTOR_MIN} },
    { MP_QSTR_preamble,     MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = 8} },
    { MP_QSTR_coding_rate,  MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_CODING_4_5} },
    { MP_QSTR_power_mode,   MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_LORA_MODE_ALWAYS_ON} },
    { MP_QSTR_tx_iq,        MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_rx_iq,        MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_adr,          MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false} },
    { MP_QSTR_public,       MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true} },
    { MP_QSTR_tx_retries,   MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int = 2} },
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

    // give it to the sleep module
    //pyb_sleep_set_wlan_obj(self); // FIXME

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
        { MP_QSTR_timeout,        MP_ARG_KW_ONLY  | MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
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
            config_get_lora_mac(cmd_data.info.join.otaa.DevEui);
        } else {
            mp_get_buffer_raise(auth[0], &bufinfo_0, MP_BUFFER_READ);
            memcpy(cmd_data.info.join.otaa.DevEui, bufinfo_0.buf, sizeof(cmd_data.info.join.otaa.DevEui));
            mp_get_buffer_raise(auth[1], &bufinfo_1, MP_BUFFER_READ);
            mp_get_buffer_raise(auth[2], &bufinfo_2, MP_BUFFER_READ);
        }
        memcpy(cmd_data.info.join.otaa.AppEui, bufinfo_1.buf, sizeof(cmd_data.info.join.otaa.AppEui));
        memcpy(cmd_data.info.join.otaa.AppKey, bufinfo_2.buf, sizeof(cmd_data.info.join.otaa.AppKey));
    } else {
        mp_obj_get_array_fixed_n(args[1].u_obj, 3, &auth);
        mp_get_buffer_raise(auth[1], &bufinfo_0, MP_BUFFER_READ);
        mp_get_buffer_raise(auth[2], &bufinfo_1, MP_BUFFER_READ);
        cmd_data.info.join.abp.DevAddr = mp_obj_int_get_truncated(auth[0]);
        memcpy(cmd_data.info.join.abp.NwkSKey, bufinfo_0.buf, sizeof(cmd_data.info.join.abp.NwkSKey));
        memcpy(cmd_data.info.join.abp.AppSKey, bufinfo_1.buf, sizeof(cmd_data.info.join.abp.AppSKey));
    }

    // get the timeout
    int32_t timeout = INT32_MAX;
    if (args[2].u_obj != mp_const_none) {
        timeout = mp_obj_get_int(args[2].u_obj);
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
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lora_join_obj, 1, lora_join);

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

STATIC mp_obj_t lora_rssi(mp_obj_t self_in) {
    lora_obj_t *self = self_in;
    return mp_obj_new_int(self->rssi);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_rssi_obj, lora_rssi);

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
    if (index >= LORA_MAX_NB_CHANNELS) {
        goto error;
    }
    uint32_t frequency = args[1].u_int;
    lora_validate_frequency(frequency);

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
    if (index >= LORA_MAX_NB_CHANNELS) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }

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
    config_get_lora_mac(mac);
    return mp_obj_new_bytes((const byte *)mac, sizeof(mac));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lora_mac_obj, lora_mac);

STATIC const mp_map_elem_t lora_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&lora_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_join),                (mp_obj_t)&lora_join_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_tx_power),            (mp_obj_t)&lora_tx_power_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bandwidth),           (mp_obj_t)&lora_bandwidth_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_frequency),           (mp_obj_t)&lora_frequency_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_coding_rate),         (mp_obj_t)&lora_coding_rate_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_preamble),            (mp_obj_t)&lora_preamble_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sf),                  (mp_obj_t)&lora_sf_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_power_mode),          (mp_obj_t)&lora_power_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rssi),                (mp_obj_t)&lora_rssi_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_has_joined),          (mp_obj_t)&lora_has_joined_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_channel),         (mp_obj_t)&lora_add_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remove_channel),      (mp_obj_t)&lora_remove_channel_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&lora_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_compliance_test),     (mp_obj_t)&lora_compliance_test_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_LORA),                MP_OBJ_NEW_SMALL_INT(E_LORA_STACK_MODE_LORA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LORAWAN),             MP_OBJ_NEW_SMALL_INT(E_LORA_STACK_MODE_LORAWAN) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_OTAA),                MP_OBJ_NEW_SMALL_INT(E_LORA_ACTIVATION_OTAA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ABP),                 MP_OBJ_NEW_SMALL_INT(E_LORA_ACTIVATION_ABP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ALWAYS_ON),           MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_ALWAYS_ON) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TX_ONLY),             MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_TX_ONLY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SLEEP),               MP_OBJ_NEW_SMALL_INT(E_LORA_MODE_SLEEP) },

#if defined(USE_BAND_868) || defined(USE_BAND_915)
    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_125KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_125_KHZ) },
#endif
#if defined(USE_BAND_868)
    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_250KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_250_KHZ) },
#endif
#if defined(USE_BAND_915)
    { MP_OBJ_NEW_QSTR(MP_QSTR_BW_500KHZ),           MP_OBJ_NEW_SMALL_INT(E_LORA_BW_500_KHZ) },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_5),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_5) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_6),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_6) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_7),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_7) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CODING_4_8),          MP_OBJ_NEW_SMALL_INT(E_LORA_CODING_4_8) },
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
    .n_recv = lora_socket_recv,
    .n_settimeout = lora_socket_settimeout,
    .n_setsockopt = lora_socket_setsockopt,
    .n_bind = lora_socket_bind,
    .n_ioctl = lora_socket_ioctl,
};

///******************************************************************************/
//// Micro Python bindings; LoRa socket

static int lora_socket_socket (mod_network_socket_obj_t *s, int *_errno) {
    if (lora_obj.state == E_LORA_STATE_NOINIT) {
        *_errno = ENETDOWN;
        return -1;
    }
    s->sock_base.sd = 1;
#if defined(USE_BAND_868)
    LORAWAN_SOCKET_SET_DR(s->sock_base.sd, DR_5);
#else
    LORAWAN_SOCKET_SET_DR(s->sock_base.sd, DR_4);
#endif
    // port #2 is the defualt one
    LORAWAN_SOCKET_SET_PORT(s->sock_base.sd, 2);
    return 0;
}

static void lora_socket_close (mod_network_socket_obj_t *s) {
    // this is to prevent the finalizer to close a socket that failed during creation
    if (s->sock_base.sd > 0) {
        s->sock_base.sd = -1;
    }
}

static int lora_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    mp_int_t n_bytes = -1;

    LORA_CHECK_SOCKET(s);

    // is the radio able to transmit
    if (lora_obj.pwr_mode == E_LORA_MODE_SLEEP) {
        *_errno = ENETDOWN;
    } else if (len > LORA_PAYLOAD_SIZE_MAX) {
        *_errno = EMSGSIZE;
    } else if (len > 0) {
        if (lora_obj.stack_mode == E_LORA_STACK_MODE_LORA) {
            n_bytes = lora_send (buf, len, s->sock_base.timeout);
        } else if (lora_obj.joined) {
            n_bytes = lorawan_send (buf, len, s->sock_base.timeout,
                                    LORAWAN_SOCKET_IS_CONFIRMED(s->sock_base.sd),
                                    LORAWAN_SOCKET_GET_DR(s->sock_base.sd),
                                    LORAWAN_SOCKET_GET_PORT(s->sock_base.sd));
        }
        if (n_bytes == 0) {
            *_errno = EAGAIN;
            n_bytes = -1;
        } else if (n_bytes < 0) {
            *_errno = ENETUNREACH;
        }
    }
    return n_bytes;
}

static int lora_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    LORA_CHECK_SOCKET(s);
    int ret = lora_recv (buf, len, s->sock_base.timeout);
    if (ret < 0) {
        *_errno = EAGAIN;
        return -1;
    }
    return ret;
}

static int lora_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    LORA_CHECK_SOCKET(s);
    if (level != SOL_LORA) {
        *_errno = EOPNOTSUPP;
        return -1;
    }

    if (opt == SO_LORAWAN_CONFIRMED) {
        if (*(uint8_t *)optval) {
            LORAWAN_SOCKET_SET_CONFIRMED(s->sock_base.sd);
        } else {
            LORAWAN_SOCKET_CLR_CONFIRMED(s->sock_base.sd);
        }
    } else if (opt == SO_LORAWAN_DR) {
        if (!lora_validate_data_rate(*(uint32_t *)optval)) {
            *_errno = EOPNOTSUPP;
            return -1;
        }
        LORAWAN_SOCKET_SET_DR(s->sock_base.sd, *(uint8_t *)optval);
    } else {
        *_errno = EOPNOTSUPP;
        return -1;
    }
    return 0;
}

static int lora_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    LORA_CHECK_SOCKET(s);
    s->sock_base.timeout = timeout_ms;
    return 0;
}

static int lora_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    LORA_CHECK_SOCKET(s);
    if (port > 224) {
        *_errno = EOPNOTSUPP;
        return -1;
    }
    LORAWAN_SOCKET_SET_PORT(s->sock_base.sd, port);
    return 0;
}

static int lora_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret = 0;

    LORA_CHECK_SOCKET(s);
    if (request == MP_IOCTL_POLL) {
        mp_uint_t flags = arg;
        if ((flags & MP_IOCTL_POLL_RD) && lora_rx_any()) {
            ret |= MP_IOCTL_POLL_RD;
        }
        if ((flags & MP_IOCTL_POLL_WR) && lora_tx_space()) {
            ret |= MP_IOCTL_POLL_WR;
        }
    } else {
        *_errno = EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}
