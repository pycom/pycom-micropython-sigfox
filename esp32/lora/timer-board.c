/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 *
 * This file contains code under the following copyright and licensing notices.
 * The code has been changed but otherwise retained.
 */

/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Bleeper STM32L151RD microcontroller pins definition

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#include "board.h"
#include "py/mphal.h"
#include "py/mpconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"
#include "esp_attr.h"

#include "math.h"

/*!
 * Hardware Time base in ms
 */
#define HW_TIMER_TIME_BASE                              1 // ms

/*!
 * Hardware Timer tick counter
 */
volatile TimerTime_t TimerTickCounter = 1;

/*!
 * Saved value of the Tick counter at the start of the next event
 */
static TimerTime_t TimerTickCounterContext = 0;

/*!
 * Value trigging the IRQ
 */
volatile TimerTime_t TimeoutCntValue = 0;

/*!
 * Timer enabled/disabled flag
 */
volatile bool TimerEnabled = false;


static IRAM_ATTR void TimerCallback (void) {
    if (TimerEnabled) {
        TimerTickCounter++;
        if (TimerTickCounter == TimeoutCntValue) {
            TimerIrqHandler();
        }
    }
}

void TimerHwInit( void ) {
    __disable_irq();
    HAL_set_tick_cb(TimerCallback);
    __enable_irq();
}

void TimerHwDeInit( void ) {
    // nothing to do here
    __disable_irq();
    HAL_set_tick_cb(NULL);
    __enable_irq();
}

IRAM_ATTR uint32_t TimerHwGetMinimumTimeout( void ) {
    return (ceil( 2 * HW_TIMER_TIME_BASE));
}

IRAM_ATTR void TimerHwStart (uint32_t val) {

    TimerTickCounterContext = TimerHwGetTimerValue();

    if (val <= HW_TIMER_TIME_BASE + 1) {
        TimeoutCntValue = TimerTickCounterContext + 1;
    } else {
        TimeoutCntValue = TimerTickCounterContext + ((val - 1) / HW_TIMER_TIME_BASE);
    }

    TimerEnabled = true;
}

void IRAM_ATTR TimerHwStop( void ) {
    TimerEnabled = false;
}

void IRAM_ATTR TimerHwDelayMs( uint32_t delay ) {
    vTaskDelay (delay / portTICK_PERIOD_MS);
}

IRAM_ATTR TimerTime_t TimerHwGetTimerValue (void) {
    TimerTime_t val;

    __disable_irq( );
    val = TimerTickCounter;
    __enable_irq( );

    return (val);
}

IRAM_ATTR TimerTime_t TimerHwGetTime( void ) {
    return TimerHwGetTimerValue() * HW_TIMER_TIME_BASE;
}

IRAM_ATTR TimerTime_t TimerHwGetElapsedTime (void) {
     return (((TimerHwGetTimerValue() - TimerTickCounterContext) + 1) * HW_TIMER_TIME_BASE);
}

IRAM_ATTR void TimerHwEnterLowPowerStopMode( void ) {
}

