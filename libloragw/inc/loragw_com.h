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

#define CMD_HEADER_TX_SIZE 4 /* Cmd + LenMsb + Len + Address */
#define CMD_HEADER_RX_SIZE 3 /* Cmd + LenMsb + Len */

#define CMD_DATA_TX_SIZE ATOMICTX
#define CMD_DATA_RX_SIZE ATOMICRX

#define OK 1
#define KO 0
#define ACK_KO 0

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */
/**
@brief structure for host to mcu commands
@param Cmd char  for cmd id
@param length  : length (16 bits) of the full msg,  length = LenMsb<<8 + Len
@param Address : address parameter is used in case of read/wrire registers cmd
@param Value   : raw data to transfer
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
/*  n           |GOTODFU                                */
/********************************************************/

typedef struct {
    char Cmd;
    uint8_t LenMsb;
    uint8_t Len;
    uint8_t Address;
    uint8_t Value[CMD_DATA_TX_SIZE];
} CmdSettings_t;

/**
@brief cmd structure response from stm32 to host
@param Cmd char  for cmd id
@param length  : length (8 bits) of the full msg
@param Rxbuf   : raw data data to transfer
*/
typedef struct {
    char Cmd;
    uint8_t LenMsb;
    uint8_t Len;
    uint8_t Rxbuf[CMD_DATA_RX_SIZE];
} AnsSettings_t;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator USB setup
@param com_target_ptr pointer on a generic pointer to USB target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_open(void **com_target_ptr);

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
@brief LoRa usb cmd to send a cmd to the stm32
@param CmdSettings_t : cmd structure
param com_target : USB target
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int SendCmd(CmdSettings_t CmdSettings, int fd);

/**
@brief LoRa usb cmd to receive a cmd response from the stm32
@param CmdSettings_t : cmd structure
@param fd : USB target
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int ReceiveAns(AnsSettings_t *Ansbuffer, int fd);

#endif

/* --- EOF ------------------------------------------------------------------ */
