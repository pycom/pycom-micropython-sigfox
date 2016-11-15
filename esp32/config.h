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

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    uint8_t lora_mac[8];
    uint8_t dummy[56];

} pycom_config_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void config_init0 (void);

bool config_set_lora_mac (const uint8_t *mac);

void config_get_lora_mac (uint8_t *mac);

#endif /* PYCOM_CONFIG_H_ */
