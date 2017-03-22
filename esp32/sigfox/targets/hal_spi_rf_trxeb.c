//*****************************************************************************
//! @file       hal_spi_rf_trxeb.c
//! @brief     	Implementation file for common spi access with the CCxxxx
//!             tranceiver radios using trxeb or MSP430F5529 Launchpad.
//!
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
 * @addtogroup RF_SPI
 * @{
 ******************************************************************************/

/******************************************************************************
 * INCLUDES
 */

#include "hal_types.h"
#include "hal_defs.h"
#include "hal_spi_rf_trxeb.h"

#include "sigfox/modsigfox.h"
#include "lora/system/spi.h"

/******************************************************************************
 * LOCAL FUNCTIONS
 */
static void trxReadWriteBurstSingle(uint8 addr,uint8 *pData,uint16 len) ;


/******************************************************************************
 * FUNCTIONS
 */
/**************************************************************************//**
 * @brief       Function to initialize SPI. CC1101/CC112x are currently
 *              supported. The supported prescalerValue must be set so that
 *              SMCLK/prescalerValue does not violate radio SPI constraints.
 *
 * @param       prescalerValue 		is SMCLK/prescalerValue. Gives SCLK frequency
 ******************************************************************************/
void
trxRfSpiInterfaceInit(uint8 prescalerValue)
{

}


/***************************************************************************//**
 * @brief       This function performs a read or write from/to a 8bit register
 *              address space. The function handles burst and single read/write
 *              as specfied in addrByte. Function assumes that chip is ready.
 *
 * @param       accessType 	Specifies if this is a read or write and if it's
 *                          a single or burst access. Bitmask made up of
 *              	\li	RADIO_BURST_ACCESS
 *					\li	RADIO_SINGLE_ACCESS
 *                  \li RADIO_WRITE_ACCESS
 *					\li	RADIO_READ_ACCESS
 * @param       addrByte 	is the address byte of the register
 * @param       pData 		is pointer to the data array
 * @param       len 		is Length of array to be read(TX)/written(RX)
 *
 * @return      status byte ::rfStatus_t
 ******************************************************************************/
IRAM_ATTR rfStatus_t
trx8BitRegAccess(uint8 accessType, uint8 addrByte, uint8 *pData, uint16 len)
{
  uint8 readValue;

  GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 17);
  readValue = SpiInOut((uint32_t) sigfox_spi.Spi, accessType|addrByte );
  trxReadWriteBurstSingle(accessType|addrByte,pData,len);
  GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 17);

  // return the status byte value
  return readValue;
}


/**************************************************************************//**
 * @brief       This function performs a read or write in the extended adress
 *              space of CC112X.
 *
 * @param       accessType	Specifies if this is a read or write and if it's
 *                          a single or burst access. Bitmask made up of
 *              	\li	RADIO_BURST_ACCESS
 *					\li	RADIO_SINGLE_ACCESS
 *                  \li RADIO_WRITE_ACCESS
 *					\li	RADIO_READ_ACCESS.
 * @param       extAddr 	is the Extended register space address = 0x2F.
 * @param       regAddr 	is the Register address in the extended address space.
 * @param       pData 		is pointer to the data array
 * @param       len 		is Length of array to be read(TX)/written(RX)
 *
 * @return      status byte ::rfStatus_t
 ******************************************************************************/
IRAM_ATTR rfStatus_t
trx16BitRegAccess(uint8 accessType, uint8 extAddr, uint8 regAddr, uint8 *pData, uint8 len)
{
  uint8 readValue;
  GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 17);
  readValue = SpiInOut((uint32_t) sigfox_spi.Spi, accessType | extAddr);
  SpiInOut((uint32_t) sigfox_spi.Spi, regAddr );
  trxReadWriteBurstSingle(accessType|extAddr,pData,len);
  GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 17);

  // return the status byte value
  return(readValue);
}


/***************************************************************************//**
 * @brief       Send command strobe to the radio. Returns status byte read
 *              during transfer of command strobe. Validation of provided
 *              is not done. Function assumes chip is ready.
 *
 * @param       cmd 	is the command strobe
 *
 * @return      status byte ::rfStatus_t
 ******************************************************************************/
IRAM_ATTR rfStatus_t
trxSpiCmdStrobe(uint8 cmd)
{
    uint8 rc = 0;
    GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 17);
    rc = SpiInOut((uint32_t) sigfox_spi.Spi, cmd);
    GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 17);
    return(rc);
}


/***************************************************************************//**
 * @brief       When the address byte is sent to the SPI slave, the next byte
 *              communicated is the data to be written or read. The address
 *              byte that holds information about read/write -and single/
 *              burst-access is provided to this function.
 *              Depending on these two bits this function will write len bytes to
 *              the radio in burst mode or read len bytes from the radio in burst
 *              mode if the burst bit is set. If the burst bit is not set, only
 *              one data byte is communicated.
 *
 * @note		: This function is used in the following way:
 *
 *              TRXEM_SPI_BEGIN();
 *              while(TRXEM_PORT_IN & TRXEM_SPI_MISO_PIN);
 *              ...[Depending on type of register access]
 *              trxReadWriteBurstSingle(uint8 addr,uint8 *pData,uint16 len);
 *              TRXEM_SPI_END();
 ******************************************************************************/
static IRAM_ATTR void
trxReadWriteBurstSingle(uint8 addr,uint8 *pData,uint16 len)
{
    uint16 i;

    if(addr&RADIO_READ_ACCESS)
    {
      if(addr&RADIO_BURST_ACCESS)
      {
        for (i = 0; i < len; i++)
        {
            // Store pData from last pData RX
            *pData = SpiInOut((uint32_t) sigfox_spi.Spi, 0 );
            pData++;
        }
      }
      else
      {
        *pData = SpiInOut((uint32_t) sigfox_spi.Spi, 0 );
      }
    }
    else
    {
      if(addr&RADIO_BURST_ACCESS)
      {
        // Communicate len number of bytes: if TX - the procedure doesn't overwrite pData
        for (i = 0; i < len; i++)
        {
          SpiInOut((uint32_t) sigfox_spi.Spi, *pData );
          pData++;
        }
      }
      else
      {
        SpiInOut((uint32_t) sigfox_spi.Spi, *pData );
      }
    }
    return;
}

/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
