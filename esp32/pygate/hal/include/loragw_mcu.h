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
 Wrapper to call MCU's HAL functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/

#ifndef _LORAGW_MCU_H
#define _LORAGW_MCU_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types*/

#include "config.h"     /* library configuration options (dynamically generated) */

#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_MCU_SUCCESS 0
#define LGW_MCU_ERROR   -1

#define STM32FWVERSION 0x010a0006 /* increment LSB for new version */

#define MCU_DELAY_COM_INIT 1000
#define MCU_DELAY_RESET 200

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Command to configure the board of the LoRa concentrator through MCU
@param conf board configuration structure to be sent to MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_board_setconf(struct lgw_conf_board_s* conf);

/**
@brief Command to configure an RF chain through MCU
@param rfchain index of the RF chain to configure
@param conf RF chain configuration structure to be sent to MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_rxrf_setconf(uint8_t rfchain, struct lgw_conf_rxrf_s* conf);

/**
@brief Command to configure an IF chain through MCU
@param ifchain index of the IF chain to configure
@param conf IF chain configuration structure to be sent to MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_rxif_setconf(uint8_t ifchain, struct lgw_conf_rxif_s* conf);

/**
@brief Command to configure the Tx gain LUT through MCU
@param conf TX LUT gain table configuration structure to be sent to MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_txgain_setconf(struct lgw_tx_gain_lut_s *conf);

/**
@brief Command to receive packet from the concentrator through MCU
@param max_pkt maximum number of received packets
@param pkt_data array of packets received
@return number of received packets
*/
int lgw_mcu_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data);

/**
@brief Command to send a packet to the concentrator through MCU
@param pkt_data packet data to be sent to MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_send(struct lgw_pkt_tx_s* pkt_data);

/**
@brief Command to get the value of the internal counter of the concentrator through MCU
@param data pointer to byte array that will be read from the concentrator
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_get_trigcnt(uint32_t *data);

/**
@brief Command to store radio calibration parameters to the concentrator through MCU
@param idx_start start index in the MCU calibration offset table where to store the given offsets
@param idx_nb the number of calibration offsets to be stored in the MCU table
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_commit_radio_calibration(uint8_t idx_start, uint8_t idx_nb);

/**
@brief Command to reset the MCU
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_reset(void);

/**
@brief Command to get the MCU's unique ID
@return status of operation (LGW_MCU_SUCCESS/LGW_MCU_ERROR)
*/
int lgw_mcu_get_unique_id(uint8_t *uid);

#endif

/* --- EOF ------------------------------------------------------------------ */
