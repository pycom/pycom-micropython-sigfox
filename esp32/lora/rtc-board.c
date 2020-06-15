/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2020, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 *
 * This file contains code under the following copyright and licensing notices.
 * The code has been changed but otherwise retained.
 */

/*!
 * \file      rtc-board.c
 *
 * \brief     Target board RTC timer and low power modes management
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Marten Lootsma(TWTG) on behalf of Microchip/Atmel (c)2017
 */

#include "lora/system/timer.h"
#include "lora/system/systime.h"
#include "gpio.h"
#include "board.h"

#include "rtc-board.h"

#include "py/mphal.h"


/*!
 * Hardware Time base in ms
 */
#define HW_TIMER_TIME_BASE                              1 // ms

/*!
 * Hardware Timer tick counter
 */
DRAM_ATTR volatile TimerTime_t RTCTimerTickCounter = 1;

/*!
 * Value trigging the IRQ
 */
DRAM_ATTR volatile TimerTime_t RTCTimeoutCntValue = 0;


#define MIN_ALARM_DELAY                             1 // in ticks

/*!
 * \brief Indicates if the RTC is already Initialized or not
 */
static bool RtcInitialized = false;

typedef enum AlarmStates_e
{
    ALARM_STOPPED = 0,
    ALARM_RUNNING = !ALARM_STOPPED
} AlarmStates_t;

/*!
 * RTC timer context 
 */
typedef struct
{
    uint32_t Time;  // Reference time
    uint32_t Delay; // Reference Timeout duration
    uint32_t AlarmState;
}RtcTimerContext_t;

/*!
 * Keep the value of the RTC timer when the RTC alarm is set
 * Set with the \ref RtcSetTimerContext function
 * Value is kept as a Reference to calculate alarm
 */
static RtcTimerContext_t RtcTimerContext;

/*!
 * Used to store the Seconds and SubSeconds.
 * 
 * WARNING: Temporary fix fix. Should use MCU NVM internal
 *          registers
 */
uint32_t RtcBkupRegisters[] = { 0, 0 };

/*!
 * \brief Callback for the hw_timer when alarm expired
 */
static void RtcAlarmIrq( void );

/*!
 * \brief Callback for the hw_timer when counter overflows
 */
static IRAM_ATTR void TimerTickCallback (void) {

    RTCTimerTickCounter++;
    if (RTCTimeoutCntValue > 0 && RTCTimerTickCounter == RTCTimeoutCntValue) {
        RtcAlarmIrq();
    }
}

void RtcInit( void )
{
    if( RtcInitialized == false )
    {
        // RTC timer
        CRITICAL_SECTION_BEGIN( );
        HAL_set_tick_cb(TimerTickCallback);
        CRITICAL_SECTION_END( );

        RtcTimerContext.AlarmState = ALARM_STOPPED;
        RtcSetTimerContext( );
        RtcInitialized = true;
    }
}

IRAM_ATTR uint32_t RtcSetTimerContext( void )
{
    RtcTimerContext.Time = ( uint32_t )RTCTimerTickCounter;
    return ( uint32_t )RtcTimerContext.Time;
}

IRAM_ATTR uint32_t RtcGetTimerContext( void )
{
    return RtcTimerContext.Time;
}

IRAM_ATTR uint32_t RtcGetMinimumTimeout( void )
{
    return( MIN_ALARM_DELAY );
}

IRAM_ATTR uint32_t RtcMs2Tick( TimerTime_t milliseconds )
{
    return ( uint32_t )( milliseconds );
}

TimerTime_t RtcTick2Ms( uint32_t tick )
{
    return ( TimerTime_t ) ( tick );
}

void RtcDelayMs( TimerTime_t milliseconds )
{
    vTaskDelay (milliseconds / portTICK_PERIOD_MS);
}

IRAM_ATTR void RtcSetAlarm( uint32_t timeout )
{
    RtcStartAlarm( timeout );
}

void RtcStopAlarm( void )
{
    RtcTimerContext.AlarmState = ALARM_STOPPED;
}

IRAM_ATTR void RtcStartAlarm( uint32_t timeout )
{
    CRITICAL_SECTION_BEGIN( );

    RtcStopAlarm( );

    RtcTimerContext.Delay = timeout;
    RtcTimerContext.Time = RTCTimerTickCounter;

    if(timeout <= MIN_ALARM_DELAY) {
        RTCTimeoutCntValue = RTCTimerTickCounter + MIN_ALARM_DELAY;
    } else {
        RTCTimeoutCntValue = RTCTimerTickCounter + timeout;
    }

    RtcTimerContext.AlarmState = ALARM_RUNNING;

    CRITICAL_SECTION_END( );
}

uint32_t RtcGetTimerValue( void )
{
    return ( uint32_t )RTCTimeoutCntValue;
}

IRAM_ATTR uint32_t RtcGetTimerElapsedTime( void )
{
    return ( uint32_t)( RTCTimerTickCounter - RtcTimerContext.Time );
}

uint32_t RtcGetCalendarTime( uint16_t *milliseconds )
{
    uint32_t calendarValue = mp_hal_ticks_ms();

    uint32_t seconds = ( uint32_t )calendarValue / 1000;

    *milliseconds =  ( uint32_t ) ( calendarValue - seconds * 1000);

    return seconds;
}

void RtcBkupWrite( uint32_t data0, uint32_t data1 )
{
    CRITICAL_SECTION_BEGIN( );
    RtcBkupRegisters[0] = data0;
    RtcBkupRegisters[1] = data1;
    CRITICAL_SECTION_END( );
}

void RtcBkupRead( uint32_t* data0, uint32_t* data1 )
{
    CRITICAL_SECTION_BEGIN( );
    *data0 = RtcBkupRegisters[0];
    *data1 = RtcBkupRegisters[1];
    CRITICAL_SECTION_END( );
}

void RtcProcess( void )
{

}

TimerTime_t RtcTempCompensation( TimerTime_t period, float temperature )
{
    return period;
}

static IRAM_ATTR void RtcAlarmIrq( void )
{
    RtcTimerContext.AlarmState = ALARM_STOPPED;

    // NOTE: The handler should take less then 1 ms otherwise the clock shifts
    TimerIrqHandler( );
}

static void RtcOverflowIrq( void )
{
    // RtcTimerContext.Time += ( uint64_t )( 1 << 32 );
}
