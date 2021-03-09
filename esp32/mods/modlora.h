/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODLORA_H_
#define MODLORA_H_

#include "board.h"
#include "lora/mac/LoRaMac.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define LORA_PAYLOAD_SIZE_MAX                                   (255)
#define LORA_CMD_QUEUE_SIZE_MAX                                 (7)
#define LORA_DATA_QUEUE_SIZE_MAX                                (7)
#define LORA_CB_QUEUE_SIZE_MAX                                  (7)
#define LORA_STACK_SIZE                                         (4096)
#define LORA_TIMER_STACK_SIZE                                   (3072)
#define LORA_TASK_PRIORITY                                      (6)
#define LORA_TIMER_TASK_PRIORITY                                (8)

#define LORA_STATUS_COMPLETED                                   (0x01)
#define LORA_STATUS_ERROR                                       (0x02)
#define LORA_STATUS_MSG_SIZE                                    (0x04)
#define LORA_STATUS_RESET_DONE                                  (0x08)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    E_LORA_CMD_INIT = 0,
    E_LORA_CMD_JOIN,
    E_LORA_CMD_TX,
    E_LORA_CMD_CONFIG_CHANNEL,
    E_LORA_CMD_LORAWAN_TX,
    E_LORA_CMD_SLEEP,
    E_LORA_CMD_WAKE_UP,
} lora_cmd_t;

typedef enum {
    E_LORA_NVS_ELE_JOINED = 0,
    E_LORA_NVS_ELE_UPLINK,
    E_LORA_NVS_ELE_DWLINK,
    E_LORA_NVS_ELE_DEVADDR,
    E_LORA_NVS_ELE_NWSKEY,
    E_LORA_NVS_ELE_APPSKEY,
    E_LORA_NVS_ELE_NET_ID,
    E_LORA_NVS_ELE_ADR_ACKS,
    E_LORA_NVS_ELE_MAC_PARAMS,
    E_LORA_NVS_ELE_CHANNELS,
    E_LORA_NVS_ELE_ACK_REQ,
    E_LORA_NVS_MAC_NXT_TX,
    E_LORA_NVS_MAC_CMD_BUF_IDX,
    E_LORA_NVS_MAC_CMD_BUF_RPT_IDX,
    E_LORA_NVS_ELE_MAC_BUF,
    E_LORA_NVS_ELE_MAC_RPT_BUF,
    E_LORA_NVS_ELE_REGION,
    E_LORA_NVS_ELE_CHANNELMASK,
    E_LORA_NVS_ELE_CHANNELMASK_REMAINING,
    E_LORA_NVS_NUM_KEYS
} e_lora_nvs_key_t;

typedef struct {
    uint32_t        frequency;
    LoRaMacRegion_t region;
    DeviceClass_t   device_class;
    uint8_t         stack_mode;
    uint8_t         preamble;
    uint8_t         bandwidth;
    uint8_t         coding_rate;
    uint8_t         sf;
    uint8_t         tx_power;
    uint8_t         power_mode;
    uint8_t         tx_retries;
    bool            txiq;
    bool            rxiq;
    bool            adr;
    bool            public;
} lora_init_cmd_data_t;

typedef struct {
    uint8_t activation;
    uint8_t otaa_dr;
    union {
        struct {
            // For over the air activation
            uint8_t           DevEui[8];
            uint8_t           AppEui[8];
            uint8_t           AppKey[16];
        } otaa;

        struct {
            // For personalization activation
            uint32_t          DevAddr;
            uint8_t           NwkSKey[16];
            uint8_t           AppSKey[16];
        } abp;
    } u;
} lora_join_cmd_data_t;

typedef struct {
    uint8_t     data[LORA_PAYLOAD_SIZE_MAX + 1];
    uint8_t     len;
    uint8_t     port;
    uint8_t     dr;
    bool        confirmed;
} lora_tx_cmd_data_t;

typedef struct {
    uint32_t    frequency;
    uint8_t     index;
    uint8_t     dr_min;
    uint8_t     dr_max;
    bool        add;
} lora_config_channel_cmd_data_t;

typedef union {
    lora_init_cmd_data_t                init;
    lora_join_cmd_data_t                join;
    lora_tx_cmd_data_t                  tx;
    lora_config_channel_cmd_data_t      channel;
} lora_cmd_info_u_t;

typedef struct {
    lora_cmd_t                  cmd;
    lora_cmd_info_u_t           info;
} lora_cmd_data_t;

///////////////////////////////////////////

typedef struct {
    uint8_t data[LORA_PAYLOAD_SIZE_MAX + 1];
    uint8_t len;
    uint8_t port;
} lora_rx_data_t;

typedef void ( *modlora_timerCallback )( void );
/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
extern DRAM_ATTR TaskHandle_t xLoRaTimerTaskHndl;
/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modlora_init0(void);
extern bool modlora_nvs_set_uint(uint32_t key_idx, uint32_t value);
extern bool modlora_nvs_set_blob(uint32_t key_idx, const void *value, uint32_t length);
extern bool modlora_nvs_get_uint(uint32_t key_idx, uint32_t *value);
extern bool modlora_nvs_get_blob(uint32_t key_idx, void *value, uint32_t *length);
extern void modlora_sleep_module(void);
extern bool modlora_is_module_sleep(void);
IRAM_ATTR extern void modlora_set_timer_callback(modlora_timerCallback cb);

extern int lora_ot_recv(uint8_t *buf, int8_t *rssi);
extern void lora_ot_send(const uint8_t *buf, uint16_t len);

#endif  // MODLORA_H_
