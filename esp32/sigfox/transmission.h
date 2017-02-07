//*****************************************************************************
//! @file       transmission.h
//! @brief      Module managing the detection of the bit (0) into the frame and calling
//!        		the low level modulation function
//!
//****************************************************************************/


#ifndef TRANSMISSION_H
#define	TRANSMISSION_H

#include "sigfox_types.h"

/********************************
 * \enum e_mode
 * \brief TX mode
 *******************************/
typedef enum{
    E_CONTINUOUS=0,		/*!< Countinuois wave mode */
    E_MODULATED,		/*!< Modulated signal */
    E_ONLYCARRIER		/*!< Carrier only */
}e_mode;

/********************************
 * \enum e_TxStatus
 * \brief TX status
 *******************************/
typedef enum{
  E_TX_IN_PROGRESS = 0,	/*!< TX in progress */
  E_TX_END = 1			/*!< TX complete */
}e_TxStatus;

void TxInit(sfx_u8 *message, sfx_u8 size);
unsigned char TxProcess(void);


#endif	/* TRANSMISSION_H */
