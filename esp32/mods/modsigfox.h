/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


#ifndef MODSIGFOX_H_
#define MODSIGFOX_H_

#include "lora/system/spi.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define SIGFOX_STATUS_COMPLETED                       (0x01)
#define SIGFOX_STATUS_ERR                             (0x02)

#define SIGFOX_TX_PAYLOAD_SIZE_MAX                    (12)
#define SIGFOX_RX_PAYLOAD_SIZE_MAX                    (8)
#define FSK_TX_PAYLOAD_SIZE_MAX                       (64)

#define SIGFOX_CMD_QUEUE_SIZE_MAX                     (2)
#define SIGFOX_DATA_QUEUE_SIZE_MAX                    (3)
#define SIGFOX_STACK_SIZE                             (4096)
#define SIGFOX_TASK_PRIORITY                          (6)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    Gpio_t        Reset;
    Gpio_t        DIO;
} sigfox_settings_t;

typedef enum {
    E_SIGFOX_CMD_INIT = 0,
    E_SIGFOX_CMD_TX,
    E_SIGFOX_CMD_TEST,
} sigfox_cmd_t;

typedef struct {
    uint32_t   frequency;
    uint8_t    mode;
    uint8_t    rcz;
} sigfox_init_cmd_data_t;

typedef struct {
    uint8_t     data[FSK_TX_PAYLOAD_SIZE_MAX + 4];
    uint8_t     len;
    uint8_t     tx_repeat;
    bool        receive;
    bool        oob;
} sigfox_tx_cmd_t;

typedef struct {
    uint32_t     mode;
    uint32_t     config;
} sigfox_test_cmd_t;

typedef union {
    sigfox_init_cmd_data_t        init;
    sigfox_tx_cmd_t               tx;
    sigfox_test_cmd_t             test;
} sigfox_cmd_info_u_t;

typedef struct {
    sigfox_cmd_t                  cmd;
    sigfox_cmd_info_u_t           info;
} sigfox_cmd_data_t;

///////////////////////////////////////////

typedef struct {
    uint8_t data[FSK_TX_PAYLOAD_SIZE_MAX + 4];
    uint8_t len;
} sigfox_rx_data_t;

typedef union {
    sigfox_rx_data_t      rx;
} sigfox_rx_info_u_t;

typedef struct {
    sigfox_rx_info_u_t    info;
} sigfox_rx_info_t;

///////////////////////////////////////////

typedef union {
    sigfox_cmd_data_t   cmd_u;
    sigfox_rx_info_t    rxd_u;
} sigfox_cmd_rx_data_t;

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
#define RADIO_RESET                                 GPIO18

#define RADIO_MOSI                                  GPIO27
#define RADIO_MISO                                  GPIO19
#define RADIO_SCLK                                  GPIO5
#define RADIO_NSS                                   GPIO17

#define RADIO_DIO                                   GPIO23

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
extern Spi_t sigfox_spi;
extern sigfox_settings_t sigfox_settings;

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modsigfox_init0 (void);

#endif  // MODSIGFOX_H_
