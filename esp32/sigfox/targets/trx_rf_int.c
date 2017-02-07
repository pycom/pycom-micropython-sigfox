//*****************************************************************************
//! @file trx_rf_int.c
//!
//! @brief  Implementation file for radio interrupt interface
//!          functions on Port 1, pin 7. The ISR is defined elsewhere
//!          and connected to the interrupt vector real time.
//
//  Copyright (C) 2015 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************/

/**************************************************************************//**
 * @addtogroup Interrupts
 * @{
 ******************************************************************************/

/******************************************************************************
 * INCLUDES
 */
#include "hal_spi_rf_trxeb.h"
#include "mods/modsigfox.h"
#include "hal_types.h"
#include "hal_defs.h"
#include "trx_rf_int.h"
#include "gpio.h"

/******************************************************************************
* CONSTANTS
*/

// Interrupt port and pin
#define TRXEM_INT_PORT 2
#define TRXEM_INT_PIN  3
#define TRXEM_INT_PORT_IN 0 //P2IN

/******************************************************************************
 * FUNCTIONS
 */
/***************************************************************************//**
 * @brief       Connects an ISR function to PORT1 interrupt vector and
 *              configures the interrupt to be a high-low transition.
 *
 * @param       pF is function pointer to ISR
 ******************************************************************************/
void
trxIsrConnect(ISR_FUNC_PTR pF)
{
    GpioInit( &sigfox_settings.DIO, RADIO_DIO, PIN_INPUT, PIN_PUSH_PULL, PIN_PULL_UP, 0 );
    GpioSetInterrupt( &sigfox_settings.DIO, IRQ_FALLING_EDGE, IRQ_HIGH_PRIORITY, pF );
}


/***************************************************************************//**
 * @brief       Clears sync interrupt flag
 ******************************************************************************/
void
trxClearIntFlag(void)
{

}


/***************************************************************************//**
 * @brief       Enables sync interrupt
 ******************************************************************************/
void
trxEnableInt(void)
{

}


/***************************************************************************//**
 * @brief       Disables sync interrupt
 ******************************************************************************/
void
trxDisableInt(void)
{
  GpioRemoveInterrupt( &sigfox_settings.DIO);
}


/**************************************************************************//**
 * @brief       Reads the value of the sync pin.
 *
 * @return      status of the interrupt pin
 ******************************************************************************/
uint8
trxSampleSyncPin(void)
{
  return (uint8) GpioRead( &sigfox_settings.DIO);
}

/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
