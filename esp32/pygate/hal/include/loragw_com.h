/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
  A communication bridge layer to abstract linux/windows OS or others.
  The current project support only linux os

License: Revised BSD License, see LICENSE.TXT file include in the project

*/

#ifndef _LORAGW_COM_H
#define _LORAGW_COM_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types*/

#include "config.h"     /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_COM_SUCCESS     0
#define LGW_COM_ERROR       -1
#define LGW_BURST_CHUNK     1024
#define LGW_COM_MUX_MODE0   0x0     /* No FPGA */
#define LGW_COM_MUX_TARGET_SX1301   0x0

#define ATOMICTX 600
#define ATOMICRX 900

#define CMD_HEADER_TX_SIZE 4 /* id + len_msb + len_lsb + address */
#define CMD_HEADER_RX_SIZE 4 /* id + len_msb + len_lsb + status */

#define CMD_DATA_TX_SIZE ATOMICTX
#define CMD_DATA_RX_SIZE (1024 + 16 * 44) /* MAX_FIFO + 16 * METADATA_SIZE_ALIGNED */

#define ACK_OK 1
#define ACK_KO 0

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/**
@struct lgw_com_cmd_t
@brief structure for host to mcu commands
*/
/********************************************************/
/*   cmd name   |      description                      */
/*------------------------------------------------------*/
/*  r           |Read register                          */
/*  s           |Read long burst First packet           */
/*  t           |Read long burst Middle packet          */
/*  u           |Read long burst End packet             */
/*  p           |Read atomic burst packet               */
/*  w           |Write register                         */
/*  x           |Write long burst First packet          */
/*  y           |Write long burst Middle packet         */
/*  z           |Write long burst End packet            */
/*  a           |Write atomic burst packet              */
/*------------------------------------------------------*/
/*  b           |lgw_receive cmd                        */
/*  c           |lgw_rxrf_setconf cmd                   */
/*  d           |int lgw_rxif_setconf_cmd               */
/*  f           |int lgw_send cmd                       */
/*  h           |lgw_txgain_setconf                     */
/*  q           |lgw_trigger                            */
/*  i           |lgw_board_setconf                      */
/*  j           |lgw_calibration_snapshot               */
/*  l           |lgw_check_fw_version                   */
/*  m           |Reset STM32                            */
/*  n           |Go to bootloader                       */
/********************************************************/

typedef struct {
    char id;                            /*!> command ID */
    uint8_t len_msb;                    /*!> command length MSB */
    uint8_t len_lsb;                    /*!> command length LSB */
    uint8_t address;                    /*!> register address for register read/write commands */
    uint8_t cmd_data[CMD_DATA_TX_SIZE]; /*!> raw data to be transfered */
} lgw_com_cmd_t;

/**
@struct lgw_com_ans_t
@brief structure for mcu to host command answers
*/
typedef struct {
    char id;                            /*!> command ID */
    uint8_t len_msb;                    /*!> command length MSB */
    uint8_t len_lsb;                    /*!> command length LSB */
    uint8_t status;                     /*!> command acknoledge */
    uint8_t ans_data[CMD_DATA_RX_SIZE]; /*!> raw answer data */
} lgw_com_ans_t;

/**
@brief Generic file handle for communication bridge
*/
#ifdef _WIN32
    typedef HANDLE lgw_handle_t;
    #define LGW_GET_HANDLE(x) ((lgw_handle_t *)x)
#else
    typedef int lgw_handle_t;
    #define LGW_GET_HANDLE(x) (*(lgw_handle_t *)x)
#endif


/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator USB setup
@param com_target_ptr pointer on a generic pointer to USB target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_open(void **com_target_ptr, const char *com_path);

/**
@brief LoRa concentrator USB close
@param com_target generic pointer to USB target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_close(void *com_target);

/**
@brief LoRa usb bridge to spi sx1308 concentrator single-byte write
@param com_target generic pointer to USB target
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_w(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data);

/**
@brief  LoRa usb bridge to spi sx1308 concentrator single-byte read
@param com_target generic pointer to USB target
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_r(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa usb bridge to spi sx1308 concentrator  burst (multiple-byte) write
@param com_target generic pointer to USB target
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_wb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa usb bridge to spi sx1308 concentrator burst (multiple-byte) read
@param com_target generic pointer to USB target
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_rb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief Send command to concentrator through MCU, and wait for answer
@param com_target generic pointer to USB target
@param cmd command to be sent to the concentrator
@param ans answer received from the concentrator
@return status of operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_send_command(void *com_target, lgw_com_cmd_t cmd, lgw_com_ans_t *ans);

#endif

/* --- EOF ------------------------------------------------------------------ */
