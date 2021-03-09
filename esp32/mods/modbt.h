/*
 * Copyright (c) 2021, Pycom Limited.
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

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
extern void modbt_init0(void);
extern mp_obj_t bt_deinit(mp_obj_t self_in);
extern void bt_resume(bool reconnect);
void modbt_deinit(bool allow_reconnect);
#endif  // MODBT_H_
