/**************************************************************************//**
    @file       hal_int.h

    @brief      HAL interrupt control header file.
******************************************************************************/
#ifndef HAL_INT_H
#define HAL_INT_H

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 * INCLUDES
 */
#include "hal_defs.h"
#include "hal_types.h"


/******************************************************************************
 * MACROS
 */

#if (defined __ICC430__) || defined(__MSP430__)

#include "intrinsics.h"

// Use the macros below to reduce function call overhead for common
// global interrupt control functions

#include "intrinsics.h"

#if (defined __ICC430__)
#define HAL_INT_ON(x)      st( __enable_interrupt(); )
#define HAL_INT_OFF(x)     st( __disable_interrupt(); )
#define HAL_INT_LOCK(x)    st( (x) = __get_interrupt_state(); \
                               __disable_interrupt(); )
#define HAL_INT_UNLOCK(x)  st( __set_interrupt_state(x); )
#endif

#if (defined __MSP430__)
#define HAL_INT_ON(x)      st( _enable_interrupts(); )
#define HAL_INT_OFF(x)     st( _disable_interrupts(); )
#define HAL_INT_LOCK(x)    st( (x) = _get_SR_register(); \
                               _disable_interrupts(); )
#define HAL_INT_UNLOCK(x)  st( _enable_interrupts(); /*_bis_SR_register(x);*/ )
#endif

#elif defined __ICC8051__

#define HAL_INT_ON(x)      st( EA = 1; )
#define HAL_INT_OFF(x)     st( EA = 0; )
#define HAL_INT_LOCK(x)    st( (x) = EA; EA = 0; )
#define HAL_INT_UNLOCK(x)  st( EA = (x); )

typedef unsigned short istate_t;

#elif defined DESKTOP

#define HAL_INT_ON()
#define HAL_INT_OFF()
#define HAL_INT_LOCK(x)    st ((x)= 1; )
#define HAL_INT_UNLOCK(x)

#elif defined __KEIL__

#define HAL_INT_ON(x)      st( EA = 1; )
#define HAL_INT_OFF(x)     st( EA = 0; )
#define HAL_INT_LOCK(x)    st( (x) = EA; EA = 0; )
#define HAL_INT_UNLOCK(x)  st( EA = (x); )



#else
//#error "Unsupported compiler"
#endif


/******************************************************************************
 * GLOBAL FUNCTIONS
 */

void   halIntOn(void);
void   halIntOff(void);
uint16 halIntLock(void);
void   halIntUnlock(uint16 key);


#ifdef  __cplusplus
}
#endif


/******************************************************************************
  Copyright 2011 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
******************************************************************************/

#endif
