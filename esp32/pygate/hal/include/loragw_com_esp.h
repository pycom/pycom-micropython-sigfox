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
@brief LoRa concentrator COM setup (configure I/O and peripherals)
@param com_target_ptr pointer on a generic pointer to COM target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_open_linux(void **com_target_ptr, const char *com_path);

/**
@brief LoRa concentrator COM close
@param com_target generic pointer to COM target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/

int lgw_com_close_linux(void *com_target);

/**
@brief LoRa concentrator COM single-byte write
@param com_target generic pointer to COM target
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_w_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data);

/**
@brief LoRa concentrator COM single-byte read
@param com_target generic pointer to COM target
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_r_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa concentrator COM burst (multiple-byte) write
@param com_target generic pointer to COM target
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_wb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa concentrator COM burst (multiple-byte) read
@param com_target generic pointer to COM target
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_rb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief Send command to LoRa concentrator through COM interface
@param cmd command to be sent to the concentrator
@param handle COM bridge handle
@return status of operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_send_cmd_linux(lgw_com_cmd_t cmd, lgw_handle_t handle);

/**
@brief Receive answer to the previously sent command from the LoRa concentrator through COM interface
@param ans answer received from to the concentrator
@param handle COM bridge handle
@return status of operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_com_receive_ans_linux(lgw_com_ans_t *ans, lgw_handle_t handle);

#endif

/* --- EOF ------------------------------------------------------------------ */
