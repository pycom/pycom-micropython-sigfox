/*!****************************************************************************
 * \file manufacturer_api.c
 * \brief Manage The SIGFOX library API
 * \author SigFox Test and Validation team
 * \version 0.1
 ******************************************************************************
 ******************************************************************************
 * \section License
 * <b>(C) Copyright 2015 SIGFOX, http://www.sigfox.com</b>
 ******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: SIGFOX has no
 * obligation to support this Software. SIGFOX is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * SIGFOX will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 */

/******* INCLUDES *************************************************************/
#include <stdint.h>
#include "radio_sx127x.h"
#include "ti_aes_128.h"
#include "transmission.h"
#include "timer.h"
#include "targets/cc112x_spi.h"
#include "targets/hal_spi_rf_trxeb.h"
#include "sigfox_api.h"
#include "stdlib.h"
#include "stdio.h"
#include "manuf_api.h"
#include "manufacturer_api.h"
#include "pycom_config.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/*!****************************************************************************
 * @addtogroup ManufacturerAPI
 * @{
 ******************************************************************************/

/******** DEFINE **************************************************************/
#define RX_FIFO_ERROR                           (0x11)
#define NVM_NUM_ELEMENTS                        (5)
#define NVS_NAMESPACE                           "SFX_NVM"

/******* GLOBAL VARIABLES *****************************************************/
extern sfx_u8 uplink_spectrum_access;
extern unsigned char encrypted_sigfox_data[];

sfx_u8 usePublicKey = SFX_FALSE;
e_SystemState volatile SysState; /*!< */

/******* LOCAL VARIABLES ******************************************************/
/* TBD_UPLINK : Non Volatile Memory
 * [0]PN9
 * [1]SeqNb
 * [2]FCC info
 */

static nvs_handle sfx_nvs_handle;
static const char *nvs_data_key[NVM_NUM_ELEMENTS] = {"SFX_PN", "SFX_SEQ", "SFX_FH", "SFX_RSSI", "SFX_FREQ"};

static int RSSI = 0;
static sfx_u8 rf_serial[2] = {0x00, 0x00};

static int manuf_sfx_ramp_up_down_time = 0;

MemoryBlock Table_200bytes;
sfx_u8 DynamicMemoryTable[SFX_DYNAMIC_MEMORY];

/*!****************************************************************************
 * \fn sfx_error_t sfx_error_t MANUF_API_rf_init(sfx_rf_mode_t rf_mode)
 * \brief Init and configure Radio link in RX/TX
 *
 * \param[in] sfx_rf_mode_t rf_mode         Init Radio link in Tx or RX
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_INIT:              Init Radio link error
 *****************************************************************************/
sfx_error_t MANUF_API_rf_init(sfx_rf_mode_t rf_mode)
{
    if (( rf_mode == SFX_RF_MODE_TX ) || ( rf_mode == SFX_RF_MODE_RX ) || ( rf_mode == SFX_RF_MODE_CS200K_RX ) || ( rf_mode == SFX_RF_MODE_CS300K_RX ) )
    {
        /* Initialize the radio in TX or RX mode */
        RADIO_init_chip(rf_mode);
    }
    else
    {
        return SFX_ERR_MANUF_INIT;
    }
    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_rf_stop(void)
 * \brief Close Radio link
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_RF_STOP:           Close Radio link error
 ******************************************************************************/
sfx_error_t MANUF_API_rf_stop(void)
{
    RADIO_close_chip();
    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_change_frequency(sfx_u32 frequency)
 * \brief Change synthesizer carrier frequency
 *
 * \param[in] sfx_u32 frequency             Frequency in Hz to program in the radio chipset
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_CH_FREQ:           Change frequency error
 ******************************************************************************/
sfx_error_t MANUF_API_change_frequency(sfx_u32 frequency)
{
    int16_t freq_offset;
    if (SFX_ERR_MANUF_NONE != MANUF_API_get_nv_mem(SFX_NVMEM_FREQ, (sfx_u16 *)&freq_offset)) {
        freq_offset = 0;
    }
    RADIO_change_frequency(frequency + freq_offset);
    return SFX_ERR_MANUF_NONE;
}


/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_get_voltage_temperature(sfx_u16 *voltage_idle, sfx_u16 *voltage_tx, sfx_u16 *temperature)
 * \brief Get voltage and temperature for Out of band frames
 * Value must respect the units bellow for backend compatibility
 *
 * \param[out] sfx_u16 *voltage_idle        Device's voltage in Idle state (mV)
 * \param[out] sfx_u16 *voltage_tx          Device's voltage in Tx state (mV)
 * \param[out] sfx_u16 *temperature         Device's temperature in 1/10 of degrees celcius
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_VOLT_TEMP:         Get voltage/temperature error
 ******************************************************************************/
sfx_error_t MANUF_API_get_voltage_temperature(sfx_u16 *voltage_idle,
                                              sfx_u16 *voltage_tx,
                                              sfx_u16 *temperature)
{

    *voltage_idle = 3300;   /* Store the proper information from your device */
    *voltage_tx = 3300;     /* Store the proper information from your device */
    *temperature = 0;       /* Store the proper information from your device */

    return  SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_aes_128_cbc_encrypt(sfx_u8 *encrypted_data, sfx_u8 *data_to_encrypt, sfx_u8 data_len);
 * \brief Encrypt a complete buffer with Secret or Test key.
 * These keys must be stored in a secure place.
 * Can be hardcoded or soft coded (iv vector contains '0')
 *
 * \param[out] sfx_u8 *encrypted_data       Result of AES Encryption
 * \param[in] sfx_u8 *data_to_encrypt       Input data to Encrypt
 * \param[in] sfx_u8 data_len               Input data length
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_AES:               AES Encryption error
 ******************************************************************************/
sfx_error_t MANUF_API_aes_128_cbc_encrypt(sfx_u8 *encrypted_data,
                                          sfx_u8 *data_to_encrypt,
                                          sfx_u8 data_len)
{
    sfx_u8 i, j, blocks;
    sfx_u8 cbc[16] = {0x00};

    blocks = data_len / 16;
    for(i = 0; i < blocks; i++)
    {
        for(j = 0; j < 16; j++)
        {
            cbc[j] ^= data_to_encrypt[j+i*16];
        }
        if (usePublicKey)
        {
            uint8_t public_key[16];
            config_get_sigfox_public_key(public_key);
            aes_enc_dec(cbc, public_key, 0);
        }
        else
        {
            uint8_t private_key[16];
            config_get_sigfox_private_key(private_key);
            aes_enc_dec(cbc, private_key, 0);
        }
        for(j = 0; j < 16; j++)
        {
            encrypted_data[j+(i*16)] = cbc[j];
        }
    }
    return SFX_ERR_MANUF_NONE;
}

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_get_rf_serial(sfx_u8 ** ptr)
 * \brief This function retrieves the RF Serial Number of the RF chipset
 *
 * \param[out] sfx_u8** returned_pointer    pointer to RF serial buffer
 *
 * \retval SFX_ERR_MANUF_NONE:             No error
 * \retval SFX_ERR_MANUF_GET_RF_SERIAL:    RF Serial error
 *******************************************************************/
sfx_error_t MANUF_API_get_rf_serial(sfx_u8 ** returned_pointer)
{
    sfx_u8 i;
    sfx_bool valid=SFX_FALSE;
    sfx_error_t err = SFX_ERR_MANUF_NONE;
    /* Repeat measurements till we have 0x48 - if we don't succeed, quit */
    for (i=0; i < 50 && valid == SFX_FALSE ; i++)
    {
         cc112xSpiReadReg(CC112X_PARTNUMBER, &rf_serial[0], 1);
         if ( rf_serial[0]== 0x58 /* CC1125 */ )
         {
            valid = SFX_TRUE;
         }
    }

    if ( valid == SFX_FALSE)
    {
        err = SFX_ERR_MANUF_GET_RF_SERIAL;
    }
    else
    {
        cc112xSpiReadReg(CC112X_PARTVERSION, &rf_serial[1], 1);
        *returned_pointer = (sfx_u8 *)rf_serial;
    }

    return err;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_malloc(sfx_u16 size, sfx_u8 **returned_pointer)
 * \brief Allocate memory for library usage (Memory usage = size (Bytes))
 *
 * \param[in] sfx_u16 size                  size of buffer to allocate in Bytes
 * \param[out] sfx_u8** returned_pointer    pointer to buffer (can be static)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_MALLOC:            Malloc error
 ******************************************************************************/
sfx_error_t MANUF_API_malloc(sfx_u16 size, sfx_u8 **returned_pointer)
{
    MemoryBlock *mem_blk;

    /* Default initialization of the returned pointer */
    *returned_pointer = (sfx_u8 *)SFX_NULL;

    /* Check that desired allocation size is among allowed allocation */
    if (size <= BYTES_SIZE_200 )
    {
        /* There is only one allocation */
        mem_blk = &Table_200bytes;
        if ( mem_blk->allocated == SFX_FALSE)
        {
            /* The memory block is free, we can allocate it */
            *returned_pointer = mem_blk->memory_ptr ;
            mem_blk->allocated = SFX_TRUE;
        }
    }
    else
    {
        /* No block available */
        return SFX_ERR_MANUF_MALLOC;
    }

    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_free(sfx_u8 *p)
 * \brief Free memory allocated to library
 *
 * \param[in] sfx_u8 *p                     pointer to buffer
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_FREE:              Free error
 ******************************************************************************/
sfx_error_t MANUF_API_free(sfx_u8 *p)
{
    /* Default initialization of the returned status */
    sfx_error_t status = SFX_ERR_MANUF_FREE;

    /* search at which memory area the pointer is assigned
       to be able to free the memory */
    if( ( p >= Table_200bytes.memory_ptr) && ( p <= (Table_200bytes.memory_ptr + BYTES_SIZE_200 ) ))
    {
        /* The pointer is in the Memory table */
        if ( Table_200bytes.allocated == SFX_TRUE )
        {
            /* The memory block has been allocated then we can free it */
            Table_200bytes.allocated = SFX_FALSE;
            status = SFX_ERR_MANUF_NONE;
        }
    }

    return status;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_delay(sfx_delay_t delay_type)
 * \brief Inter stream delay, called between each MANUF_API_rf_send
 * - SFX_DLY_INTER_FRAME_TX  : 0 to 2s in Uplink ETSI
 * - SFX_DLY_INTER_FRAME_TRX : 500 ms in Uplink/Downlink FCC & Downlink ETSI
 * - SFX_DLY_OOB_ACK :         1.4s to 4s for Downlink OOB
 * - SFX_DLY_SENSI :           4s for sensitivity test mode
 *
 * \param[in] sfx_delay_t delay_type        Type of delay to call
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_DLY:               Delay error
 ******************************************************************************/
sfx_error_t MANUF_API_delay(sfx_delay_t delay_type)
{
    /* Local variables */
    sfx_error_t err = SFX_ERR_MANUF_NONE;

    /* Switch/case */
    switch(delay_type)
    {
    case SFX_DLY_INTER_FRAME_TRX :
        /* Delay  is 500ms  in FCC and ETSI
         * In ARIB : minimum delay is 50 ms */
        /* 500 ms delay is the SIGFOX specification : this delay has to take into account the
         * ramp-up and ramp-down of the frame. Please tune these values to match SIGFOX specifications */
        if( uplink_spectrum_access == SFX_LBT )
        {
            vTaskDelay((50 + 1) / portTICK_PERIOD_MS);
        }
        else
        {
            if( uplink_spectrum_access == SFX_FH ) {
                manuf_sfx_ramp_up_down_time = 1;
            } else {
                manuf_sfx_ramp_up_down_time = 25;
            }
            vTaskDelay(((500 + 1) - manuf_sfx_ramp_up_down_time) / portTICK_PERIOD_MS);
        }
        break;

    case SFX_DLY_INTER_FRAME_TX :
        /* Start delay 0 seconds to 2 seconds in FCC and ETSI
         * In ARIB : minimum delay is 50 ms */
        if( uplink_spectrum_access == SFX_LBT )
        {
            vTaskDelay((50 + 1) / portTICK_PERIOD_MS);
        }
        else
        {
            vTaskDelay((1000 + 1) / portTICK_PERIOD_MS);
        }
        break;

    case SFX_DLY_OOB_ACK :
        /* Start delay between 1.4 seconds to 4 seconds in FCC and ETSI */
        vTaskDelay((2000 + 1) / portTICK_PERIOD_MS);
        break;

    case SFX_DLY_SENSI :
        /* Start delay of 4 seconds */
        vTaskDelay((4000 + 1) / portTICK_PERIOD_MS);
        break;

    case SFX_DLY_CS_SLEEP :
        vTaskDelay((500 + 1) / portTICK_PERIOD_MS);
        break;

    default :
        err = SFX_ERR_MANUF_DLY;
        break;
    }

    return err;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_set_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 value)
 * \brief Set value to an index in non volatile memory. It is strongly
 *        recommanded to use NV memory like EEPROM since this function
 *        is called 2 times per SIGFOX_API_send_xxx and 3 times in
 *        downlink (4 Bytes to reserve in memory)
 *
 * \param[in] sfx_nvmem_t nvmem_datatype    Type of data to write
 * \param[in] sfx_u16 value                 Value to write in memory
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_SETNVMEM:          Write nvmem error
 ******************************************************************************/
sfx_error_t MANUF_API_set_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 value)
{
    if (ESP_OK == nvs_set_u16(sfx_nvs_handle, nvs_data_key[nvmem_datatype], value)) {
        if (ESP_OK == nvs_commit(sfx_nvs_handle)) {
            return SFX_ERR_MANUF_NONE;
        }
    }
    return SFX_ERR_MANUF_SETNVMEM;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_get_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 *return_value)
 * \brief Get value from an index in non volatile memory
 *
 * \param[in] sfx_nvmem_t nvmem_datatype    Type of data to read
 * \param[out] sfx_u16 *return_value        Read value from memory
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_GETNVMEM:          Read nvmem error
 ******************************************************************************/
sfx_error_t MANUF_API_get_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 *return_value)
{
    if (ESP_OK == nvs_get_u16(sfx_nvs_handle, nvs_data_key[nvmem_datatype], return_value)) {
        return SFX_ERR_MANUF_NONE;
    }
    return SFX_ERR_MANUF_GETNVMEM;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_rf_send(sfx_u8 *stream, sfx_u8 size)
 * \brief BPSK Modulation of data stream
 *        (from synchro bit field to CRC)
 *
 * \param[in] sfx_u8 *stream                Complete stream to modulate
 * \param[in] sfx_u8 size                   Length of stream
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_SEND:              Send data stream error
 ******************************************************************************/
sfx_error_t MANUF_API_rf_send(sfx_u8 *stream, sfx_u8 size)
{
    sfx_u8 End_Transmission = SFX_FALSE;
    SysState = TxStart;

    /* Init Bitrate Timer */
    TIMER_bitrate_init();

    /* Loop till end of transmission
     * Symbols are sent when the interrupt is asserted
     */
    while (End_Transmission == SFX_FALSE)
    {
        /* Send frame management */
        switch(SysState)
        {
        case TxStart:
            TxInit(stream, size);

            /* Start the signal */
            RADIO_start_rf_carrier();

            /* Start the bitrate timer */
            TIMER_bitrate_start();

            SysState = TxWaiting;
            break;

        case TxWaiting:
            /* Continue processing - there are still bits to transmit */
            break;

        case TxEnd:
            /* End of the transmission */
            /* Disable the timer interrupt */
            TIMER_bitrate_stop();
            /* Stop the radio */
            RADIO_stop_rf_carrier();
            End_Transmission = SFX_TRUE;
            break;

        default:
            End_Transmission = SFX_TRUE;
            break;
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    return SFX_ERR_MANUF_NONE;
}



/******************************************************************************/
/****************************** DOWNLINK MODE API *****************************/
/******************************************************************************/

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_timer_stop(void)
 * \brief Stop the timer (started with MANUF_API_timer_start)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_STOP:        Stop timer error
 ******************************************************************************/
sfx_error_t MANUF_API_timer_stop(void)
{
    // Reset the timeout value
    TIMER_downlink_timeout = SFX_FALSE;

    // Stop the timeout timer
    TIMER_downlink_timing_stop();

    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_timer_stop_carrier_sense(void)
 * \brief Stop the timer (started with MANUF_API_timer_start_carrier_sense)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_STOP_CS:     Stop timer error
 ******************************************************************************/
sfx_error_t MANUF_API_timer_stop_carrier_sense(void) {
    /* Stop the timeout timer */
    TIMER_carrier_sense_stop();

    return SFX_ERR_MANUF_NONE;
}


/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_timer_wait_for_end(void)
 * \brief Blocking function to wait for interrupt indicating timer
 *        elapsed. This function is only used for the 20 seconds wait
 *        in downlink.
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_END:         Wait end of timer error
 ******************************************************************************/
sfx_error_t MANUF_API_timer_wait_for_end(void)
{
    /* Need to wait for the time set in MANUF_API_timer_start */
    while (TIMER_downlink_timeout == SFX_FALSE) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    /* Reset the timeout value */
    TIMER_downlink_timeout = SFX_FALSE;

    /* Stop the interrupt */
    TIMER_downlink_timing_stop();

    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_get_rssi(sfx_s8 *rssi)
 * \brief Get RSSI level for out of band frame in downlink
 *        Returned RSSI = Chipset RSSI + 100 (backend compatibility)
 *        Example : Chipset RSSI = -126 dBm, Returned RSSI = -26.
 *
 * \param[out] sfx_s8 *rssi                 Returned signed RSSI
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_GET_RSSI:          Read RSSI error
 ******************************************************************************/
sfx_error_t MANUF_API_get_rssi(sfx_s8 *rssi)
{
    int16_t rssi_offset;
    if (SFX_ERR_MANUF_NONE != MANUF_API_get_nv_mem(SFX_NVMEM_RSSI, (sfx_u16 *)&rssi_offset)) {
        rssi_offset = 0;
    }
    *rssi = (sfx_s8)((RSSI - 102) + 100 + rssi_offset);

    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_timer_start(sfx_u16 time_duration_in_s)
 * \brief Start timer for :
 *         - 20 seconds wait in downlink
 *         - 25 seconds listening in downlink
 *         - 6 seconds listening in sensitivity test mode
 *         - 0 to 255 seconds listening in GFSK test mode (config argument)
 *
 * \param[in] sfx_u16 time_duration_in_s    Timer value in seconds
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_START:       Start timer error
 ******************************************************************************/
sfx_error_t MANUF_API_timer_start(sfx_u16 time_duration_in_s)
{
    /* Initialize the number of interrupt to wait for */
    TIMER_downlink_timing_init( time_duration_in_s );

    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_wait_frame(sfx_u8 *frame, sfx_rx_state_t *rx_state)
 * \brief Get all GFSK frames received in Rx buffer, structure of
 *        frame is : Synchro bit + Synchro frame + 15 Bytes. This function must
 *        be blocking state since data is received or timer of 25 s has elapsed.
 *
 * - If received buffer, function returns SFX_ERR_MANUF_NONE then the
 *   library will try to decode frame. If the frame is not correct, the
 *   library will recall MANUF_API_wait_frame.
 *
 * - If 25 seconds timer has elapsed, function returns
 *   SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT then library will stop receive
 *   frame phase.
 *
 * \param[out] sfx_s8 *frame                  Receive buffer
 *
 * \retval SFX_ERR_MANUF_NONE:                No error
 * \retval SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT:  Wait frame error
 ******************************************************************************/
sfx_error_t MANUF_API_wait_frame(sfx_u8 *frame)
{
    sfx_error_t status;
    sfx_u8 rxlastindex;
    sfx_u8 marcStatus;
    sfx_u8 End_Reception = SFX_FALSE;
    sfx_u8 iter;
    sfx_u8 rx_last;

    /* Set radio in RX */
    trxSpiCmdStrobe(CC112X_SRX);

    TIMER_RxTx_done_start();

    /* Loop till end of Reception */
    while( ( End_Reception == SFX_FALSE ) && ( TIMER_downlink_timeout == SFX_FALSE ))
    {
        /* Reception frame management */
        switch(packetSemaphore)
        {
        case ISR_IDLE:
            /* Wait */
            break;

        case ISR_ACTION_REQUIRED:
            /* Reinitialize the packetSemaphore */
            packetSemaphore = ISR_IDLE;

            /* A SigFox frame has been received */
            /* Read number of bytes in rx fifo */
            /* IMPORTANT : using the register CC112X_NUM_RXBYTES gives wrong values concerning the packet length
             * DO NOT USE THIS REGISTER, use RXLAST instead which give the last index in the RX FIFO
             * and flush the FIFO after reading it   */

            rxlastindex = 0;

            // Read 10 times to get around a bug
            for (iter=0; iter<10; iter++)
            {
                cc112xSpiReadReg(CC112X_RXLAST, &rx_last, 1);
                rxlastindex |= rx_last;
            }

            /* Check that we have bytes in fifo */
            if(rxlastindex != 0)
            {
                /* Read marcstate to check for RX FIFO error */
                cc112xSpiReadReg(CC112X_MARCSTATE, &marcStatus, 1);

                /* Mask out marcstate bits and check if we have a RX FIFO error */
                if((marcStatus & 0x1F) == RX_FIFO_ERROR)
                {
                    /* Flush RX Fifo */
                    trxSpiCmdStrobe(CC112X_SFRX);

                    /* We go back to reception */
                }
                else
                {
                    /* Read n bytes from rx fifo */
                    cc112xSpiReadRxFifo(frame, rxlastindex+1);
                    cc112xSpiReadReg(CC112X_MARCSTATE, &marcStatus, 1);

                    /* Once read, Flush RX Fifo */
                    trxSpiCmdStrobe(CC112X_SFRX);

                    /* Check CRC ok (CRC_OK: bit7 in second status byte)
                     * This assumes status bytes are appended in RX_FIFO
                     * (PKT_CFG1.APPEND_STATUS = 1.)
                     * If CRC is disabled the CRC_OK field will read 1
                     */
                    if(frame[rxlastindex] & 0x80)
                    {
                        /* isolate the RSSI value */
                        RSSI = frame[15];
                    }

                    status = SFX_ERR_MANUF_NONE;

                    /* End of Reception, as a frame has been received.
                       The frame will be analysed by the Sigfox library to check it was for the device.
                       In case the received frame is not for the device, the SigFox library will call
                       the SIGFOX_API_wait_frame() function again */
                    End_Reception = SFX_TRUE;

                    /* Set radio back in RX - as when a packet has been received, the radio is back to IDLE */
                    trxSpiCmdStrobe(CC112X_SRX);
                }
            }
            break;

        default:
            break;
        }/* End of switch */

        vTaskDelay(2 / portTICK_PERIOD_MS);

    } /* End of while */

    if( TIMER_downlink_timeout == SFX_TRUE )
    {
        /* Stop the timeout */
        TIMER_RxTx_done_stop();
        MANUF_API_timer_stop();

        status = SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT;
    }

    return status;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_wait_for_clear_channel ( sfx_u8 cs_min,
 *                                                    sfx_u8 cs_threshold);
 * \brief This function is used in ARIB standard for the Listen Before Talk
 *        feature. It listens the 200kHz Sigfox band during a sliding window set
 *        in the MANUF_API_timer_start_carrier_sense().
 *        If the channel is clear during the minimum carrier sense
 *        value ( cs_min ), under the limit of the cs_threshold,
 *        the functions returns with SFX_ERR_MANUF_NONE (transmission
 *        allowed). Otherwise it continues to listen to the channel till the expiration of the
 *        carrier sense maximum window and then returns SFX_ERR_MANUF_WAIT_CS_TIMEOUT.
 *
 * \param[out] sfx_u8 cs_min                  Minimum Carrier Sense time in ms.
 * \param[out] sfx_u8 cs_threshold            Power threshold limit to declare the channel clear.
 *                                            Value is a positive one but should be used as negative.
 *                                            i.e : cs_threshold value 80 means threshold of -80dBm
 *
 * \retval SFX_ERR_MANUF_NONE:                No error
 * \retval SFX_ERR_MANUF_WAIT_CS_TIMEOUT:     Wait Carrier Sense Timeout
 ******************************************************************************/
sfx_error_t MANUF_API_wait_for_clear_channel(sfx_u8 cs_min, sfx_u8 cs_threshold)
{
    /* TO BE IMPLEMENTED FOR ARIB STANDARD only */
    sfx_u8 writeByte;

    sfx_error_t status = SFX_ERR_MANUF_NONE;

    /* Set the power threshold */
    /* Power is set in the register as a Two's complement value */
    writeByte = 256 - cs_threshold;

    cc112xSpiWriteReg(CC112X_AGC_CS_THR, &writeByte, 1);

    /* Set radio in RX */
    /* Configure the radio and activate RX mode */
    trxSpiCmdStrobe(CC112X_SRX);

    /* Set the timer with the proper config to trigger read of RSSI */
    TIMER_get_rssi_init(cs_min);
    TIMER_get_rssi_start();

    /* Loop till end of Carrier Sense Timeout or Carrier Sense OK */
    while( ( TIMER_rssi_end == SFX_FALSE ) && ( TIMER_carrier_sense_timeout == SFX_FALSE ))
    {
        /* All the treatments are done in the interrupt context */
        vTaskDelay(2 / portTICK_PERIOD_MS);
    } /* End of while */

    /* Stop the RSSI timer whatever is the status */
    TIMER_get_rssi_stop();

    /* Flush RX Fifo - */
    trxSpiCmdStrobe(CC112X_SFRX);

    if( TIMER_carrier_sense_timeout == SFX_TRUE )
    {
        status = SFX_ERR_MANUF_WAIT_CS_TIMEOUT;

        /* Reset the timeout */
        TIMER_carrier_sense_timeout = SFX_FALSE;
    }
    else
    {
        /* Carrier Sense OK - reset the carrier_sense_flag */
        TIMER_rssi_end = SFX_FALSE;

        /* Stop the timer   */
        MANUF_API_timer_stop_carrier_sense();
    }

    return status;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_timer_start_carrier_sense(sfx_u16 time_duration_in_ms)
 * \brief Start timer for :
 *          - carrier sense maximum window ( used in ARIB standard )
 *
 * \param[in] sfx_u16 time_duration_in_ms    Timer value in milliseconds
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_START_CS:    Start CS timer error
 ******************************************************************************/
sfx_error_t MANUF_API_timer_start_carrier_sense(sfx_u16 time_duration_in_ms)
{
    TIMER_carrier_sense_init(time_duration_in_ms);
    return SFX_ERR_MANUF_NONE;
}

/*!****************************************************************************
 * \fn sfx_error_t MANUF_API_report_test_result(sfx_bool status)
 * \brief To report the result of Rx test for each valid message
 *        received/validated by library. Manufacturer api to show the result
 *        of RX test mode : can be uplink radio frame or uart print or
 *        gpio output.
 *
 * \param[in] sfx_bool status               Is SFX_TRUE when result ok else SFX_FALSE
 *                                          See SIGFOX_API_test_mode summary
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TEST_REPORT:       Report test result error
 ******************************************************************************/
sfx_error_t MANUF_API_report_test_result(sfx_bool status)
{
    int8_t rssi;
    MANUF_API_get_rssi(&rssi);
    printf("Test result = %d, RSSI = %d\n", status ? 1 : 0, (int16_t)(rssi - 100));
    return SFX_ERR_MANUF_NONE;
}

sfx_error_t MANUF_API_nvs_open(void)
{
    uint16_t data;
    bool commit = false;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &sfx_nvs_handle) != ESP_OK) {
        return (SFX_ERR_NONE + 1);
    } else {
        for (int i = 0; i < NVM_NUM_ELEMENTS; i++) {
            if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u16(sfx_nvs_handle, nvs_data_key[i], &data)) {
                // initialize the value to 0
                nvs_set_u16(sfx_nvs_handle, nvs_data_key[i], 0);
                commit = true;
            }
        }
        if (commit) {
            if (ESP_OK != nvs_commit(sfx_nvs_handle)) {
                return (SFX_ERR_NONE + 1);
            }
        }
        return SFX_ERR_NONE;
    }
}

/*!***************************************************************************
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
