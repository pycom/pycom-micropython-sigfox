/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "otplat_alarm.h"
#include <stdint.h>
#include <stdbool.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"

#include "esp32_mphal.h"
#include "random.h"
#include "../lib/lora/system/timer.h"
#include <openthread/platform/alarm-milli.h>
#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/thread_ftd.h>

static bool is_running = false;
static TimerEvent_t otPlatAlarm;
static otInstance *otPtr = NULL;

/**
 * Alarm callback
 *
 */
void alarmCallback(void) {
    if (is_running) {
        is_running = false;
#if OPENTHREAD_ENABLE_DIAG
        if (otPlatDiagModeGet())
        {
            otPlatDiagAlarmFired(otPtr);
        }
        else
#endif
        otPlatAlarmMilliFired(otPtr);
    }
}

/**
 * openthread alarm initialisation
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 */
void otPlatAlarmInit(otInstance *aInstance) {
    if (otPtr == NULL) {
        otPtr = aInstance;
        TimerInit(&otPlatAlarm, alarmCallback);
        TimerStart(&otPlatAlarm);
    }
}

/**
 * Get the current time.
 *
 * @returns The current time in milliseconds.
 */
uint32_t otPlatAlarmMilliGetNow(void) {
    //return mp_hal_ticks_ms();
    return TimerGetCurrentTime();
}

/**
 * Set the alarm to fire at @p aDt milliseconds after @p aT0.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 * @param[in] aT0        The reference time.
 * @param[in] aDt        The time delay in milliseconds from @p aT0.
 */
void otPlatAlarmMilliStartAt(otInstance *aInstance, uint32_t aT0, uint32_t aDt) {
    (void) aInstance;

    uint32_t now = TimerGetCurrentTime();

    if (aT0 + aDt < now) {
        // set alarm ASAP (100msec) if params are "in the past"
        TimerSetValue(&otPlatAlarm, 100);
    } else {
        TimerSetValue(&otPlatAlarm, (aT0 + aDt) - now);
    }
    TimerStart(&otPlatAlarm);

    is_running = true;
    return;
}

/**
 * Stop the alarm.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 */
void otPlatAlarmMilliStop(otInstance *aInstance) {

    (void) aInstance;
    TimerStop(&otPlatAlarm);
    is_running = false;
}

/**
 * Get a 32-bit random value.
 *
 * This function may be implemented using a pseudo-random number generator.
 *
 * @returns A 32-bit random value.
 *
 */
uint32_t otPlatRandomGet(void) {
    return rng_get();
}

/**
 * Print an IPv6 address
 *
 * @param[in] addr  The OpenThread IPv6
 */
void printSingleIpv6(otIp6Address *addr) {
    printf("%x:%x:%x:%x:%x:%x:%x:%x\n", HostSwap16(addr->mFields.m16[0]),
            HostSwap16(addr->mFields.m16[1]), HostSwap16(addr->mFields.m16[2]),
            HostSwap16(addr->mFields.m16[3]), HostSwap16(addr->mFields.m16[4]),
            HostSwap16(addr->mFields.m16[5]), HostSwap16(addr->mFields.m16[6]),
            HostSwap16(addr->mFields.m16[7]));
}

