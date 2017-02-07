//*****************************************************************************
//! @file       hal_spi_rf_trxeb.h
//! @brief     	Common header file for spi access to the different tranceiver
//!             radios. Supports CC1101/CC112X radios.
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


#ifndef HAL_SPI_RF_TRXEB_H
#define HAL_SPI_RF_TRXEB_H

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * INCLUDES
 */
#include "hal_types.h"
#include "hal_defs.h"

/******************************************************************************
 * CONSTANTS
 */

#define TRXEM_PORT_SEL       0 //P3SEL
#define TRXEM_PORT_OUT       0 //P3OUT
#define TRXEM_PORT_DIR       0 //P3DIR
#define TRXEM_PORT_IN        0 //P3IN


#define TRXEM_SPI_MOSI_PIN   0 //BIT0
#define TRXEM_SPI_MISO_PIN   0 //BIT1
#define TRXEM_SPI_SCLK_PIN   0 //BIT2

#define TRXEM_CS_N_PORT_SEL	 0 //P2SEL
#define TRXEM_CS_N_PORT_DIR	 0 //P2DIR
#define TRXEM_CS_N_PORT_OUT	 0 //P2OUT
#define TRXEM_SPI_SC_N_PIN   0 //BIT2

#define RF_RESET_N_PORT_SEL  0 //P2SEL
#define RF_RESET_N_PORT_DIR	 0 //P2DIR
#define RF_RESET_N_PORT_OUT	 0 //P2OUT
#define RF_RESET_N_PIN	     0 //BIT6

#define    RF_LNA_EN_PxOUT         0 //P1OUT
#define    RF_LNA_EN_PxDIR         0 //P1DIR
#define    RF_LNA_EN_PIN           0 //BIT6

#define    RF_PA_EN_PxOUT          0 //P2OUT
#define    RF_PA_EN_PxDIR          0 //P2DIR
#define    RF_PA_EN_PIN            0 //BIT7


#define RADIO_BURST_ACCESS   0x40
#define RADIO_SINGLE_ACCESS  0x00
#define RADIO_READ_ACCESS    0x80
#define RADIO_WRITE_ACCESS   0x00


/* Bit fields in the chip status byte */
#define STATUS_CHIP_RDYn_BM             0x80
#define STATUS_STATE_BM                 0x70
#define STATUS_FIFO_BYTES_AVAILABLE_BM  0x0F


/******************************************************************************
 * MACROS
 */

/* Macros for Tranceivers(TRX) */
#if defined(__MSP430F5529__) || defined(__MSP430F5438A__)

#define TRXEM_SPI_BEGIN()				st( TRXEM_CS_N_PORT_OUT &= ~TRXEM_SPI_SC_N_PIN; NOP(); )
#define TRXEM_SPI_TX(x)                st( UCB0IFG &= ~UCRXIFG; UCB0TXBUF= (x); )
#define TRXEM_SPI_WAIT_DONE()          st( while(!(UCB0IFG & UCRXIFG)); )
#define TRXEM_SPI_WAIT_TX_DONE()       st( while(!(UCB0IFG & UCTXIFG)); )
#define TRXEM_SPI_RX()                 UCB0RXBUF
#define TRXEM_SPI_WAIT_MISO_LOW(x)     st( uint8 count = 200; \
                                           while(TRXEM_PORT_IN & TRXEM_SPI_MISO_PIN) \
                                           { \
                                              __delay_cycles(5000); \
                                              count--; \
                                              if (count == 0) break; \
                                           } \
                                           if(count>0) (x) = 1; \
                                           else (x) = 0; )

#define TRXEM_SPI_END()                st( NOP(); TRXEM_CS_N_PORT_OUT |= TRXEM_SPI_SC_N_PIN; )
#endif
/******************************************************************************
 * TYPEDEFS
 */

typedef struct
{
  uint16  addr;
  uint8   data;
}registerSetting_t;

typedef uint8 rfStatus_t;



/******************************************************************************
 * PROTOTYPES
 */

void trxRfSpiInterfaceInit(uint8 prescalerValue);///////////////////////////////////////////////////////
rfStatus_t trx8BitRegAccess(uint8 accessType, uint8 addrByte, uint8 *pData, uint16 len);
rfStatus_t trxSpiCmdStrobe(uint8 cmd);

/* CC112X specific prototype function */
rfStatus_t trx16BitRegAccess(uint8 accessType, uint8 extAddr, uint8 regAddr, uint8 *pData, uint8 len);

#ifdef  __cplusplus
}
#endif
#endif //HAL_SPI_RF_TRXEB_H
