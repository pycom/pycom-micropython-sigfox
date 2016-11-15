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

/*!
 * RTC Time base in us
 */
#define RTC_ALARM_TIME_BASE_US                             30.51

/*!
 * \brief Initializes the RTC timer
 *
 * \remark The timer is based on the RTC
 */
void RtcInit( void ) {
    // nothing to do here
}

/*!
 * \brief Stop the RTC Timer
 */
void RtcStopTimer( void ) {

}

/*!
 * \brief Return the minimum timeout the RTC is able to handle (in us)
 *
 * \retval minimum value for a timeout
 */
uint32_t RtcGetMinimumTimeout( void ) {
    return (RTC_ALARM_TIME_BASE_US * 10);
}

/*!
 * \brief Start the RTC timer
 *
 * \remark The timer is based on the RTC Alarm running at 32.768KHz
 *
 * \param[IN] timeout       Duration of the Timer
 */
void RtcSetTimeout( uint32_t timeout ) {
    (void)timeout;
}

/*!
 * \brief Get the RTC timer value
 *
 * \retval RTC Timer value
 */
TimerTime_t RtcGetTimerValue( void ) {
    return 0;
}

/*!
 * \brief Get the RTC timer elapsed time since the last Alarm was set
 *
 * \retval RTC Elapsed time since the last alarm
 */
uint32_t RtcGetTimerElapsedTime( void ) {
    return 1;
}

/*!
 * \brief This function block the MCU from going into Low Power mode
 *
 * \param [IN] Status enable or disable
 */
void BlockLowPowerDuringTask ( bool Status ) {
    (void)Status;
}

/*!
 * \brief Sets the MCU in low power STOP mode
 */
void RtcEnterLowPowerStopMode( void ) {

}

/*!
 * \brief Restore the MCU to its normal operation mode
 */
void RtcRecoverMcuStatus( void ) {

}

/*!
 * \brief Perfoms a standard blocking delay in the code execution
 *
 * \param [IN] delay Delay value in ms
 */
void RtcDelayMs ( uint32_t delay ) {
    (void)delay;
}

