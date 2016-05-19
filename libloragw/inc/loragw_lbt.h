/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Functions used to handle the Listen Before Talk feature

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Michael Coracin
*/

#ifndef _LORAGW_LBT_H
#define _LORAGW_LBT_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_LBT_SUCCESS 0
#define LGW_LBT_ERROR -1

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

int lbt_setconf(struct lgw_conf_lbt_s * conf);

int lbt_setup(uint32_t rf_freq, uint8_t rssi_target, uint16_t lbt_scan_time_us, uint8_t lbt_nb_channel);

int lbt_start(void);

int lbt_is_channel_free(struct lgw_pkt_tx_s * pkt_data, bool * tx_allowed);

#endif
/* --- EOF ------------------------------------------------------------------ */
