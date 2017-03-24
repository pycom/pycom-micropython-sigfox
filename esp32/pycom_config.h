/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef PYCOM_CONFIG_H_
#define PYCOM_CONFIG_H_

#include "py/mpconfig.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    uint8_t lpwan_mac[8];

    uint8_t sigfox_id[4];
    uint8_t sigfox_pac[8];
    uint8_t sigfox_private_key[16];
    uint8_t sigfox_public_key[16];
    uint8_t dummy[12];

} pycom_config_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void config_init0 (void);

bool config_set_lpwan_mac (const uint8_t *mac);

void config_get_lpwan_mac (uint8_t *mac);

bool config_set_sigfox_id (uint8_t *id);

void config_get_sigfox_id (uint8_t *id);

bool config_set_sigfox_pac (uint8_t *pac);

void config_get_sigfox_pac (uint8_t *pac);

bool config_set_sigfox_public_key (uint8_t *public_key);

void config_get_sigfox_public_key (uint8_t *public_key);

bool config_set_sigfox_private_key (uint8_t *private_key);

void config_get_sigfox_private_key (uint8_t *private_key);

#endif /* PYCOM_CONFIG_H_ */
