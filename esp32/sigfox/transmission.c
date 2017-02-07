//*****************************************************************************
//! @file       transmission.c
//! @brief      Module managing the detection of the bit (0) into the frame and calling
//!        		the low level modulation function
//!
//****************************************************************************/


/**************************************************************************//**
* @addtogroup Modulation
* @{
******************************************************************************/


/******************************************************************************
 * INCLUDES
 */
#include "sigfox_types.h"
#include "stdlib.h"
#include "stdbool.h"
#include "radio.h"
#include "targets/hal_spi_rf_trxeb.h"
#include "esp_attr.h"

/******************************************************************************
 * DEFINE
 */


/******************************************************************************
 * LOCAL VARIABLES
 */
static unsigned int bit_index_in_frame;
static unsigned char *frame;
static unsigned char u8_FrameSize;


/******************************************************************************
 * STATIC FUNCTIONS
 */
static unsigned char Read_Bit(unsigned char * frame);


/*!****************************************************************************
 * \fn void TX_init(sfx_u8 *message, sfx_u8 size)
 * \brief Initialize local paramters
 * \param message is the pointer to the frame to send
 * \param size is the frame size in bytes
 ******************************************************************************/
void TxInit(sfx_u8 *message, sfx_u8 size)
{
    bit_index_in_frame = 0;
    frame = message;
    u8_FrameSize = size;
}


/***************************************************************************//**
 *  @brief 		Function that reads the frame, and calls the modulation function
 *
 *  @param 		frame 			is the pointer to the frame to send
 *	@param 		u8_FrameSize 	is the frame size in bytes
 *
 *  @return 	\li \b 0 if TX frame in progress
 *  @return		\li \b 1 if TX frame complete
 ******************************************************************************/
IRAM_ATTR unsigned char
TxProcess(void)
{
    unsigned char TxStatus;

    TxStatus = 0;

    if (bit_index_in_frame < (u8_FrameSize)*8)
    {
        //
    	// If a 0 is read in the frame, call the modulation function
        //
    	if(0 ==  Read_Bit(frame))
        {
        	RADIO_modulate();
        }

        //
    	// TX frame in progress
        //
    	TxStatus = 0;
        bit_index_in_frame++;
    }
    else
    {
    	//
    	// End of the frame
        //
    	TxStatus = 1;
    }
    return TxStatus;
}


/***************************************************************************//**
 *  @brief 		Reads a bit in buffer to send to RF TX.
 *
 *  @param 		frame 		is the pointer to the TX frame.
 *
 *  @return 	\li \b 0 if modulation needs to be produced
 *  @return		\li \b 1 if not
 ******************************************************************************/
IRAM_ATTR static unsigned char
Read_Bit(unsigned char * frame)
{
    register sfx_u8 index_bit;
    register sfx_u8 index_byte;
    sfx_u8 bit_to_send;

    index_bit = bit_index_in_frame % 8;
    index_byte = bit_index_in_frame / 8;
    bit_to_send = ((frame[index_byte] >> (7 - index_bit)) & 0x01);
    return bit_to_send;
}


/**************************************************************************//**
* Close the Doxygen group.
* @}
******************************************************************************/

