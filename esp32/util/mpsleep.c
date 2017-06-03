/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>

#include "py/mpstate.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "rom/rtc.h"
#include "mpsleep.h"

/******************************************************************************
 DECLARE PRIVATE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE TYPES
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC mpsleep_reset_cause_t mpsleep_reset_cause = MPSLEEP_PWRON_RESET;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mpsleep_init0 (void) {
    // check the reset casue (if it's soft reset, leave it as it is)
    switch (rtc_get_reset_reason(0)) {
        case TG0WDT_SYS_RESET:
            mpsleep_reset_cause = MPSLEEP_WDT_RESET;
            break;
        case DEEPSLEEP_RESET:
            mpsleep_reset_cause = MPSLEEP_DEEPSLEEP_RESET;
            break;
        case RTCWDT_BROWN_OUT_RESET:
            mpsleep_reset_cause = MPSLEEP_BROWN_OUT_RESET;
            break;
        case TG1WDT_SYS_RESET:      // machine.reset()
            mpsleep_reset_cause = MPSLEEP_HARD_RESET;
            break;
        case POWERON_RESET:
        case RTCWDT_RTC_RESET:      // silicon bug after power on
        default:
            mpsleep_reset_cause = MPSLEEP_PWRON_RESET;
            break;
    }
}

void mpsleep_signal_soft_reset (void) {
    mpsleep_reset_cause = MPSLEEP_SOFT_RESET;
}

mpsleep_reset_cause_t mpsleep_get_reset_cause (void) {
    return mpsleep_reset_cause;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
