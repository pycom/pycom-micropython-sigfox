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

int SendCmd_linux(CmdSettings_t CmdSettings, int fd);
int ReceiveAns_linux(AnsSettings_t *Ansbuffer, int fd);

#endif

/* --- EOF ------------------------------------------------------------------ */
