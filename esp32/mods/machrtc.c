/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdio.h>
#include <string.h>

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "timeutils.h"
#include "esp_system.h"

extern uint64_t system_get_rtc_time(void);

typedef struct _mach_rtc_obj_t {
    mp_obj_base_t base;
} mach_rtc_obj_t;

// RTC overflow checking
static uint64_t rtc_last_ticks;
static int64_t delta = 0;

void rtc_init0(void) {
    // system_get_rtc_time() is always 0 after reset/deepsleep
    rtc_last_ticks = system_get_rtc_time();
}

void mach_rtc_set_us_since_2000(uint64_t nowus) {
    // // Save RTC ticks for overflow detection.
    // rtc_last_ticks = system_get_rtc_time();
    // delta = nowus - (((uint64_t)rtc_last_ticks) >> 12);
}

uint64_t mach_rtc_get_us_since_2000(void) {
    uint64_t rtc_ticks;

    // ESP-SDK system_get_rtc_time() only returns uint32 and therefore
    // overflow about every 7:45h.  Thus, we have to check for
    // overflow and handle it.
    rtc_ticks = system_get_rtc_time();
    if (rtc_ticks < rtc_last_ticks) {
        // adjust delta because of RTC overflow.
        delta += UINT64_MAX;
    }
    rtc_last_ticks = rtc_ticks;

    return (uint64_t)(rtc_ticks / 150000);
};
