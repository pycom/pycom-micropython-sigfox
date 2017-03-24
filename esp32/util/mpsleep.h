/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPSLEEP_H_
#define MPSLEEP_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

typedef enum {
    MPSLEEP_PWRON_RESET = 0,
    MPSLEEP_HARD_RESET,
    MPSLEEP_WDT_RESET,
    MPSLEEP_HIB_RESET,
    MPSLEEP_SOFT_RESET
} mpsleep_reset_cause_t;

/******************************************************************************
 DECLARE FUNCTIONS
 ******************************************************************************/
void mpsleep_init0 (void);
void mpsleep_signal_soft_reset (void);
mpsleep_reset_cause_t mpsleep_get_reset_cause (void);

#endif /* MPSLEEP_H_ */
