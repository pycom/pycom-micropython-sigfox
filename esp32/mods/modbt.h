/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODBT_H_
#define MODBT_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MOD_BT_MAX_SCAN_RESULTS             8
#define MOD_BT_MAX_ADVERTISEMENT_LEN        31

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modlora_init0(void);

#endif  // MODBT_H_
