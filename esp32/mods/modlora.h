/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODLORA_H_
#define MODLORA_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define LORA_PAYLOAD_SIZE_MAX                                   (255)
#define LORA_CMD_QUEUE_SIZE_MAX                                 (2)
#define LORA_DATA_QUEUE_SIZE_MAX                                (2)
#define LORA_STACK_SIZE                                         (3072)
#define LORA_TASK_PRIORITY                                      (6)

#define LORA_STATUS_COMPLETED                                   (0x01)
#define LORA_STATUS_ERROR                                       (0x02)

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

typedef struct {
    uint32_t    frequency;
    uint8_t     stack_mode;
    uint8_t     preamble;
    uint8_t     bandwidth;
    uint8_t     coding_rate;
    uint8_t     sf;
    uint8_t     tx_power;
    uint8_t     power_mode;
    uint8_t     tx_retries;
    bool        txiq;
    bool        rxiq;
    bool        adr;
    bool        public;
} lora_init_cmd_data_t;

typedef struct {
    uint8_t activation;
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
    };
} lora_join_cmd_data_t;

typedef struct {
    uint8_t     data[LORA_PAYLOAD_SIZE_MAX];
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
    uint8_t data[LORA_PAYLOAD_SIZE_MAX];
    uint8_t len;
} lora_rx_data_t;

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modlora_init0(void);

#endif  // MODLORA_H_
