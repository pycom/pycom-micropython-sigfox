//*****************************************************************************
//! @file       manufacturer_api.h
//! @brief
//!
//****************************************************************************/

#ifndef MANUFACTURER_API_H
#define MANUFACTURER_API_H

#define BYTES_SIZE_200                  200
#define NUMBER_BLOCKS_200BYTES          1
#define START_DYNAMIC_MEMORY            0

#define START_MEMORY_BLOCK_200BYTES     START_DYNAMIC_MEMORY
#define END_MEMORY_BLOCK_200BYTES       START_DYNAMIC_MEMORY + (NUMBER_BLOCKS_200BYTES * BYTES_SIZE_200)
#define SFX_DYNAMIC_MEMORY              END_MEMORY_BLOCK_200BYTES

typedef struct {
    sfx_u8 * memory_ptr;
    sfx_u8  allocated;
} MemoryBlock;

extern sfx_u8 usePublicKey;
extern MemoryBlock Table_200bytes;
extern sfx_u8 DynamicMemoryTable[SFX_DYNAMIC_MEMORY];

/********************************
 * \enum e_SystemState
 * \brief transmitter status
 *******************************/
typedef enum{
   IdleState = 0,   /*!< Idle state */
   TxStart,         /*!< Start the carrier */
   TxWaiting,       /*!< Continue Transmit since all bits have not been sent */
   TxProcessing,    /*!< Send a bit */
   TxEnd            /*!< Transmission is finished */
} e_SystemState;

sfx_error_t MANUF_API_nvs_open(void);

#endif
