/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


#ifndef MODSIGFOX_H_
#define MODSIGFOX_H_

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"

#include "lora/system/spi.h"
#include "modnetwork.h"

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
#define SIGFOX_STACK_SIZE                             (3584)
#define SIGFOX_TASK_PRIORITY                          (6)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    E_SIGFOX_STATE_NOINIT = 0,
    E_SIGFOX_STATE_IDLE,
    E_SIGFOX_STATE_RX,
    E_SIGFOX_STATE_TX,
    E_SIGFOX_STATE_TEST
} sigfox_state_t;

typedef enum {
    E_SIGFOX_RCZ1 = 0,
    E_SIGFOX_RCZ2,
    E_SIGFOX_RCZ3,
    E_SIGFOX_RCZ4,
} sigfox_rcz_t;

typedef enum {
    E_SIGFOX_MODE_SIGFOX = 0,
    E_SIGFOX_MODE_FSK
} sigfox_mode_t;

typedef struct {
  mp_obj_base_t     base;
  sigfox_mode_t     mode;
  sigfox_state_t    state;
  uint32_t          frequency;
} sigfox_obj_t;

typedef struct {
    uint32_t    index;
    uint32_t    size;
    uint8_t     data[FSK_TX_PAYLOAD_SIZE_MAX + 4];
} sigfox_partial_rx_packet_t;

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

#if defined (SIPY) // For FIPY this is set as part of the LORA stack
#define RADIO_RESET                                 GPIO18

#define RADIO_MOSI                                  GPIO27
#define RADIO_MISO                                  GPIO19
#define RADIO_SCLK                                  GPIO5
#define RADIO_NSS                                   GPIO17

#define RADIO_DIO                                   GPIO23
#endif
/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
extern Spi_t sigfox_spi;
extern sigfox_settings_t sigfox_settings;
extern sigfox_obj_t sigfox_obj;

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modsigfox_init0 (void);
extern void sigfox_update_id (void);
extern void sigfox_update_pac (void);
extern void sigfox_update_private_key (void);
extern void sigfox_update_public_key (void);

extern mp_obj_t sigfox_init_helper(sigfox_obj_t *self, const mp_arg_val_t *args);
extern mp_obj_t sigfox_mac(mp_obj_t self_in);
extern mp_obj_t sigfox_id(mp_obj_t self_in);
extern mp_obj_t sigfox_pac(mp_obj_t self_in);
extern mp_obj_t sigfox_test_mode(mp_obj_t self_in, mp_obj_t mode, mp_obj_t config);
extern mp_obj_t sigfox_cw(mp_obj_t self_in, mp_obj_t frequency, mp_obj_t start);
extern mp_obj_t sigfox_frequencies(mp_obj_t self_in);
extern mp_obj_t sigfox_config(mp_uint_t n_args, const mp_obj_t *args);
extern mp_obj_t sigfox_public_key(mp_uint_t n_args, const mp_obj_t *args);
extern mp_obj_t sigfox_rssi(mp_obj_t self_in);
extern mp_obj_t sigfox_rssi_offset(mp_uint_t n_args, const mp_obj_t *args);
extern mp_obj_t sigfox_freq_offset(mp_uint_t n_args, const mp_obj_t *args);
extern mp_obj_t sigfox_version(mp_obj_t self_in);
extern mp_obj_t sigfox_info(mp_obj_t self_in);
extern mp_obj_t sigfox_reset(mp_obj_t self_in);

extern int sigfox_socket_socket (mod_network_socket_obj_t *s, int *_errno);
extern void sigfox_socket_close (mod_network_socket_obj_t *s);
extern int sigfox_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
extern int sigfox_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
extern int sigfox_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
extern int sigfox_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
extern int sigfox_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);

#endif  // MODSIGFOX_H_
