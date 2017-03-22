//*****************************************************************************
//! @file       cc112x_spi.c
//! @brief      Implementation file for basic and neccessary functions
//!          	to communicate with CC112X over SPI
//
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
#include "cc112x_spi.h"

#include "sigfox/modsigfox.h"
#include "lora/system/spi.h"
/******************************************************************************
 * FUNCTIONS
 */
/**************************************************************************//**
 * @brief       Read value(s) from config/status/extended radio register(s).
 *             \li If len  = 1: Reads a single register
 *             \li If len != 1: Reads len register values in burst mode
 *
 * @param      addr 	is address of the first register to read
 * @param      pData 	is pointer to data array where read bytes are saved
 * @param      len 		is the number of bytes to read
 *
 * @return     status byte ::rfStatus_t
 *****************************************************************************/
IRAM_ATTR rfStatus_t
cc112xSpiReadReg(uint16 addr, uint8 *pData, uint8 len)
{
    uint8 tempExt  = (uint8)(addr>>8);
    uint8 tempAddr = (uint8)(addr & 0x00FF);
    uint8 rc = 0;

    // Checking if this is a FIFO access -> returns chip not ready
    if((CC112X_SINGLE_TXFIFO<=tempAddr)&&(tempExt==0)) return STATUS_CHIP_RDYn_BM;

    // Decide what register space is accessed
    if(!tempExt)
    {

        rc = trx8BitRegAccess((RADIO_BURST_ACCESS|RADIO_READ_ACCESS),tempAddr,pData,len);
    }
    else if (tempExt == 0x2F)
    {
        rc = trx16BitRegAccess((RADIO_BURST_ACCESS|RADIO_READ_ACCESS),tempExt,tempAddr,pData,len);
    }
    return (rc);
}


/**************************************************************************//**
 * @brief       Write value(s) to config/status/extended radio register(s).
 *              \li If len  = 1: Writes a single register
 *              \li If len  > 1: Writes len register values in burst mode
 *
 * @param       addr 	is address of the first register to write
 * @param       pData 	is pointer to data array that holds bytes to be written
 * @param       len 	is the number of bytes to write
 *
 * @return      status byte ::rfStatus_t
 *****************************************************************************/
IRAM_ATTR rfStatus_t
cc112xSpiWriteReg(uint16 addr, uint8 *pData, uint8 len)
{

    uint8 tempExt  = (uint8)(addr>>8);
    uint8 tempAddr = (uint8)(addr & 0x00FF);
    uint8 rc = 0;

    // Checking if this is a FIFO access - returns chip not ready
    if((CC112X_SINGLE_TXFIFO<=tempAddr)&&(tempExt==0)) return STATUS_CHIP_RDYn_BM;

    // Decide what register space is accessed
    if(!tempExt)
    {
    rc = trx8BitRegAccess((RADIO_BURST_ACCESS|RADIO_WRITE_ACCESS),tempAddr,pData,len);
    }
    else if (tempExt == 0x2F)
    {
    rc = trx16BitRegAccess((RADIO_BURST_ACCESS|RADIO_WRITE_ACCESS),tempExt,tempAddr,pData,len);
    }
    return (rc);
}


/***************************************************************************//**
 * @brief       Write pData to radio transmit FIFO.
 *
 * @param       pData 	is pointer to data array that is written to TX FIFO
 * @param       len 	is the length of data array to be written
 *
 * @return      status byte ::rfStatus_t
 ******************************************************************************/
rfStatus_t
cc112xSpiWriteTxFifo(uint8 *pData, uint8 len)
{
    uint8 rc;
    rc = trx8BitRegAccess(0x00,CC112X_BURST_TXFIFO, pData, len);
    return (rc);
}


/***************************************************************************//**
 * @brief       Reads RX FIFO values to pData array
 *
 * @param       pData 	is pointer to data array where RX FIFO bytes are saved
 * @param       len 	is the number of bytes to read from the RX FIFO
 *
 * @return      status byte ::rfStatus_t
 ******************************************************************************/
rfStatus_t cc112xSpiReadRxFifo(uint8 * pData, uint8 len)
{
    uint8 rc;
    rc = trx8BitRegAccess(0x00,CC112X_BURST_RXFIFO, pData, len);
    return (rc);
}


/**************************************************************************//**
 * @brief   This function transmits a No Operation Strobe (SNOP) to get the
 *          status of the radio and the number of free bytes in the TX FIFO.
 *
 * @return  status byte ::rfStatus_t
 *
 * @note	Status byte format:
 *
 *          ---------------------------------------------------------------------------
 *          |          |            |                                                 |
 *          | CHIP_RDY | STATE[2:0] | FIFO_BYTES_AVAILABLE (free bytes in the TX FIFO |
 *          |          |            |                                                 |
 *          ---------------------------------------------------------------------------
 *****************************************************************************/
rfStatus_t
cc112xGetTxStatus(void)
{
    return(trxSpiCmdStrobe(CC112X_SNOP));
}


/**************************************************************************//**
 *  @brief    This function transmits a No Operation Strobe (SNOP) with the
 *            read bit set to get the status of the radio and the number of
 *            available bytes in the RXFIFO.
 *
 * @return  status byte ::rfStatus_t
 *
 * @note	Status byte format:
 *
 *          ---------------------------------------------------------------------------
 *          |          |            |                                                 |
 *          | CHIP_RDY | STATE[2:0] | FIFO_BYTES_AVAILABLE (free bytes in the TX FIFO |
 *          |          |            |                                                 |
 *          ---------------------------------------------------------------------------
 *****************************************************************************/
rfStatus_t
cc112xGetRxStatus(void)
{
    return(trxSpiCmdStrobe(CC112X_SNOP | RADIO_READ_ACCESS));
}

/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
