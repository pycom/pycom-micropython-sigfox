//*****************************************************************************
//! @file       timer.c
//! @brief      Timer Management.
//!				Two timers are used in this project.
//!       \li \e Timer1 is used to generate bit rate ( fcc : 600bps / etsi : 100bps )
//!       \li \e Timer0 is used to ensure SigFox Downlink protocol timings are under control
//!              \li \c 20 s Waiting time after the 1st INITIATE_DOWNLINK Uplink Frame
//!              \li \c 25 s Reception windows to get the Downling frame
//!
//****************************************************************************/

#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include "py/mpconfig.h"

/******************************************************************************
 * FUNCTION PROTOTYPES
 */
void TIMER_bitrate_init(void);
void TIMER_bitrate_start(void);
void TIMER_bitrate_stop(void);
void TIMER_get_rssi_start(void);
void TIMER_get_rssi_stop(void);
void TIMER_get_rssi_init(sfx_u8 time_in_milliseconds);
void TIMER_bitrate_create (void);
void TIMER_downlinnk_timer_create (void);
void TIMER_carrier_sense_timer_create (void);
#if !defined(FIPY) && !defined(LOPY4)
void TIMER_RxTx_done_timer_create (void);
void TIMER_RxTx_done_start (void);
void TIMER_RxTx_done_stop (void);
#endif
void TIMER_downlink_timing_init(sfx_u16 time_in_seconds);
void TIMER_downlink_timing_stop(void);
void TIMER_carrier_sense_init(sfx_u16 time_in_milliseconds);
void TIMER_carrier_sense_stop(void);
void __delay_cycles(volatile int x);

extern bool TIMER_downlink_timeout;
extern bool TIMER_carrier_sense_timeout;
extern bool TIMER_rssi_end;

#endif // TIMER_H


