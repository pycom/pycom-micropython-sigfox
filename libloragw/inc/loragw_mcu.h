/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
 Wrapper to call MCU's HAL functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAGW_MCU_H
#define _LORAGW_MCU_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types*/

#include "config.h"     /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define STM32FWVERSION 0x010a0000

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Command to configure the board of the LoRa concentrator through MCU
@param com_target generic pointer to USB target
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_board_setconf(uint8_t *data, uint16_t size);

/**
@brief Command to configure an RF chain through MCU
@param com_target generic pointer to COM target
@param rfchain index of the RF chain to configure
@param data pointer to byte array that contains the configuration to be applied
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_rxrf_setconf(uint8_t rfchain, uint8_t *data, uint16_t size);

/**
@brief Command to configure an IF chain through MCU
@param com_target generic pointer to COM target
@param ifchain index of the IF chain to configure
@param data pointer to byte array that contains the configuration to be applied
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_rxif_setconf(uint8_t ifchain, uint8_t *data, uint16_t size);

/**
@brief Command to configure the Tx gain LUT through MCU
@param com_target generic pointer to COM target
@param data pointer to byte array that contains the configuration to be applied
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_txgain_setconf(uint8_t *data, uint16_t size);

/**
@brief Command to receive packet from the concentrator through MCU
@param com_target generic pointer to COM target
@param max_packet maximum number of received packets
@param data pointer to byte array that will receive packets
@return number of received packets
*/
int lgw_mcu_receive(uint8_t max_packet, uint8_t *data);

/**
@brief Command to send a packet to the concentrator through MCU
@param com_target generic pointer to COM target
@param data pointer to byte array that contains the packet to be sent
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_send(uint8_t *data, uint16_t size);

/**
@brief Command to get the value of the internal counter of the concentrator through MCU
@param com_target generic pointer to COM target
@param data pointer to byte array that will be read from the concentrator
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_get_trigcnt(uint32_t *data);

/**
@brief Command to store radio calibration parameters to the concentrator through MCU
@param com_target generic pointer to COM target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_commit_radio_calibration(void);

/**
@brief Command to reset the MCU
@param com_target generic pointer to COM target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_reset(void);

/**
@brief Command to switch the MCU to DFU mode
@param com_target generic pointer to COM target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_set_dfu_mode(void);

/**
@brief Command to get the MCU's unique ID
@param com_target generic pointer to USB target
@return status of register operation (LGW_COM_SUCCESS/LGW_COM_ERROR)
*/
int lgw_mcu_get_unique_id(uint8_t *uid);

#endif

/* --- EOF ------------------------------------------------------------------ */
