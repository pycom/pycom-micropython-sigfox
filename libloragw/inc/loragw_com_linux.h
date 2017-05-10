/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
this file contains the USB cmd to configure and communicate with
the Sx1308 LoRA concentrator.
An USB CDC drivers is required to establish the connection with the picogateway board.

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _LORAGW_com_linux_H
#define _LORAGW_com_linux_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types*/

#include "config.h"     /* library configuration options _linux(dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator SPI setup _linux(configure I/O and peripherals)
@param com_target_ptr pointer on a generic pointer to SPI target _linux(implementation dependant)
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_open_linux(void **com_target_ptr);

/**
@brief LoRa concentrator SPI close
@param com_target generic pointer to SPI target _linux(implementation dependant)
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_close_linux(void *com_target);

/**
@brief LoRa concentrator SPI single-byte write
@param com_target generic pointer to SPI target _linux(implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_w_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data);

/**
@brief LoRa concentrator SPI single-byte read
@param com_target generic pointer to SPI target _linux(implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_r_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa concentrator SPI burst _linux(multiple-byte) write
@param com_target generic pointer to SPI target _linux(implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte_linux(s)
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_wb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa concentrator SPI burst _linux(multiple-byte) read
@param com_target generic pointer to SPI target _linux(implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte_linux(s)
@return status of register operation _linux(LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_rb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

int lgw_receive_cmd_linux(void *com_target, uint8_t max_packet, uint8_t *data);
int lgw_rxrf_setconfcmd_linux(void *com_target, uint8_t rfchain, uint8_t *data, uint16_t size);
int lgw_rxif_setconfcmd_linux(void *com_target, uint8_t ifchain, uint8_t *data, uint16_t size);
int lgw_txgain_setconfcmd_linux(void *com_target, uint8_t *data, uint16_t size);
int lgw_sendconfcmd_linux(void *com_target, uint8_t *data, uint16_t size);
int lgw_trigger_linux(void *com_target, uint8_t address, uint32_t *data);
int lgw_boardconfcmd_linux(void * com_target, uint8_t *data, uint16_t size);
int lgw_calibration_snapshot_linux(void * com_target);
int lgw_resetSTM32_linux(void * com_target);
int lgw_GOTODFU_linux(void * com_target);
int lgw_GetUniqueId_linux(void * com_target, uint8_t * uid);

#endif

/* --- EOF ------------------------------------------------------------------ */
