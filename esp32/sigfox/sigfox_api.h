/*!
    \mainpage Sigfox Protocol V1 library API documentation

    Library to talk Sigfox protocol V1.
*/
/*!
 * \file sigfox_api.h
 * \brief Sigfox user functions
 * \author $(SIGFOX_LIB_AUTHOR)
 * \version $(SIGFOX_LIB_VERSION)
 * \date $(SIGFOX_LIB_DATE)
 * \copyright Copyright (c) 2011-2015 SIGFOX, All Rights Reserved. This is unpublished proprietary source code of SIGFOX.
 *
 * This file includes the user's functions to send data on sigfox's network,
 * such as sending a bit, a frame or a sigfox out of band message.
 * This library supports FH, DC and LBT spectrum access to be compliant with ETSI, FCC and ARIB standards.
 */

#ifndef SIGFOX_API_H_
#define SIGFOX_API_H_

#include "sigfox_types.h"


/********************************
 * \enum sfx_spectrum_access_t
 * \brief Data type for Spectrum Access
 * Define as bit mask value so that we can combine them
 * if needed
 *******************************/
typedef enum
{
    SFX_FH   = 1, /*!< Index of Frequency Hopping  */
    SFX_LBT  = 2, /*!< Index of Listen Before Talk */
    SFX_DC   = 4, /*!< Index of Duty Cycle         */
}sfx_spectrum_access_t;

typedef struct sfx_rcz_t
{
    sfx_u32 open_tx_frequency;                /*!< Uplink frequency used to open the library
                                                   This is not necessary the Transmitter center frequency in Hz
                                                   as it may depends on the values set in Config Words */

    sfx_u32 open_rx_frequency;                /*!< Downlink frequency used to open the library */
    sfx_u32 macro_channel_width;              /*!< Macro channel = SIGFOX Operational radio band */
    sfx_u16 uplink_baudrate;                  /*!< Uplink baudrate of the modulation */
    sfx_spectrum_access_t spectrum_access;    /*!< Spectrum access : can be Duty Cycle, Frequency Hopping or Listen Before Talk */
}sfx_rcz_t;

/*!
 * \defgroup SIGFOX_RCZ_CONFIGURATIONS Defines the SIGFOX RCZ configurations
 *
 *  @{
 */

/* Define Radio Configuration Zone */
#define RCZ1_OPEN_UPLINK_CENTER_FREQUENCY     (sfx_u32)(868130000) /* Hz */
#define RCZ1_OPEN_DOWNLINK_CENTER_FREQUENCY   (sfx_u32)(869525000) /* Hz */
#define RCZ1_MACRO_CHANNEL_WIDTH              (sfx_u32)(192000)    /* Hz */
#define RCZ1_UPLINK_BAUDRATE                  (sfx_u16)(100)       /* bps */
#define RCZ1_UPLINK_SPECTRUM_ACCESS           SFX_DC

#define RCZ2_OPEN_UPLINK_CENTER_FREQUENCY     (sfx_u32)(902200000) /* Hz */
#define RCZ2_OPEN_DOWNLINK_CENTER_FREQUENCY   (sfx_u32)(905200000) /* Hz */
#define RCZ2_MACRO_CHANNEL_WIDTH              (sfx_u32)(192000)    /* Hz */
#define RCZ2_UPLINK_BAUDRATE                  (sfx_u16)(600)       /* bps */
#define RCZ2_UPLINK_SPECTRUM_ACCESS           SFX_FH
#define RCZ2_SET_STD_CONFIG_WORD_0            (sfx_u32)0x000001FF
#define RCZ2_SET_STD_CONFIG_WORD_1            (sfx_u32)0x00000000
#define RCZ2_SET_STD_CONFIG_WORD_2            (sfx_u32)0x00000000
#define RCZ2_SET_STD_DEFAULT_CHANNEL          (sfx_u16)(1)

#define RCZ3_OPEN_UPLINK_CENTER_FREQUENCY     (sfx_u32)(923200000) /* Hz */
#define RCZ3_OPEN_DOWNLINK_CENTER_FREQUENCY   (sfx_u32)(922200000) /* Hz */
#define RCZ3_MACRO_CHANNEL_WIDTH              (sfx_u32)(36000)     /* Hz */
#define RCZ3_UPLINK_BAUDRATE                  (sfx_u16)(100)       /* bps */
#define RCZ3_UPLINK_SPECTRUM_ACCESS           SFX_LBT

#define RCZ4_OPEN_UPLINK_CENTER_FREQUENCY     (sfx_u32)(902200000) /* Hz */
#define RCZ4_OPEN_DOWNLINK_CENTER_FREQUENCY   (sfx_u32)(922300000) /* Hz */
#define RCZ4_MACRO_CHANNEL_WIDTH              (sfx_u32)(192000)    /* Hz */
#define RCZ4_UPLINK_BAUDRATE                  (sfx_u16)(600)       /* bps */
#define RCZ4_UPLINK_SPECTRUM_ACCESS           SFX_FH
#define RCZ4_SET_STD_CONFIG_WORD_0            (sfx_u32)0x00000000
#define RCZ4_SET_STD_CONFIG_WORD_1            (sfx_u32)0xF0000000
#define RCZ4_SET_STD_CONFIG_WORD_2            (sfx_u32)0x0000001F /* Value to be checked */
#define RCZ4_SET_STD_DEFAULT_CHANNEL          (sfx_u16)(63)

#define RCZ101_OPEN_UPLINK_CENTER_FREQUENCY   (sfx_u32)(68862500)  /* Hz */
#define RCZ101_OPEN_DOWNLINK_CENTER_FREQUENCY (sfx_u32)(72912500)  /* Hz */
#define RCZ101_MACRO_CHANNEL_WIDTH            (sfx_u32)(12500)     /* Hz */
#define RCZ101_UPLINK_BAUDRATE                (sfx_u16)(100)       /* bps */
#define RCZ101_UPLINK_SPECTRUM_ACCESS         SFX_DC


/* ----------------------------------------------
   IMPORTANT INFORMATION :
   ----------------------------------------------
   The SIGFOX Library needs to be opened with
   one of the below configurations.
   ----------------------------------------------
*/
#define RCZ1 { RCZ1_OPEN_UPLINK_CENTER_FREQUENCY, RCZ1_OPEN_DOWNLINK_CENTER_FREQUENCY, RCZ1_MACRO_CHANNEL_WIDTH, RCZ1_UPLINK_BAUDRATE, RCZ1_UPLINK_SPECTRUM_ACCESS }
#define RCZ2 { RCZ2_OPEN_UPLINK_CENTER_FREQUENCY, RCZ2_OPEN_DOWNLINK_CENTER_FREQUENCY, RCZ2_MACRO_CHANNEL_WIDTH, RCZ2_UPLINK_BAUDRATE, RCZ2_UPLINK_SPECTRUM_ACCESS }
#define RCZ3 { RCZ3_OPEN_UPLINK_CENTER_FREQUENCY, RCZ3_OPEN_DOWNLINK_CENTER_FREQUENCY, RCZ3_MACRO_CHANNEL_WIDTH, RCZ3_UPLINK_BAUDRATE, RCZ3_UPLINK_SPECTRUM_ACCESS }
#define RCZ4 { RCZ4_OPEN_UPLINK_CENTER_FREQUENCY, RCZ4_OPEN_DOWNLINK_CENTER_FREQUENCY, RCZ4_MACRO_CHANNEL_WIDTH, RCZ4_UPLINK_BAUDRATE, RCZ4_UPLINK_SPECTRUM_ACCESS }
#define RCZ101 { RCZ101_OPEN_UPLINK_CENTER_FREQUENCY, RCZ101_OPEN_DOWNLINK_CENTER_FREQUENCY, RCZ101_MACRO_CHANNEL_WIDTH, RCZ101_UPLINK_BAUDRATE, RCZ101_UPLINK_SPECTRUM_ACCESS }

/*!
 * \defgroup SFX_ERR_CODES Return Error codes definition for SIGFOX APIs
 *
 *  @{
 */
#define SFX_ERR_NONE                          0x00 /*!< No error */

#define SFX_ERR_OPEN_MALLOC                   0x10 /*!< Error on MANUF_API_malloc or buffer pointer NULL */
#define SFX_ERR_OPEN_ID_PTR                   0x11 /*!< ID pointer NULL */
#define SFX_ERR_OPEN_GET_SEQ                  0x12 /*!< Error on MANUF_API_get_nv_mem w/ SFX_NVMEM_SEQ_CPT */
#define SFX_ERR_OPEN_GET_PN                   0x13 /*!< Error on MANUF_API_get_nv_mem w/ SFX_NVMEM_PN */
#define SFX_ERR_OPEN_STATE                    0x14 /*!< State is not idle, library should be closed before */
#define SFX_ERR_OPEN_GET_FH                   0x15 /*!< Error on MANUF_API_get_nv_mem w/ SFX_NVMEM_FH */
#define SFX_ERR_OPEN_MACRO_CHANNEL_WIDTH      0x16 /*!< Macro channel width not allowed */
#define SFX_ERR_OPEN_RCZ_PTR                  0x17 /*!< RCZ pointer is NULL */

#define SFX_ERR_CLOSE_FREE                    0x20 /*!< Error on MANUF_API_free */
#define SFX_ERR_CLOSE_RF_STOP                 0x21 /*!< Error on MANUF_API_rf_stop */

#define SFX_ERR_SEND_FRAME_DATA_LENGTH        0x30 /*!< Customer data length > 12 Bytes */
#define SFX_ERR_SEND_FRAME_STATE              0x31 /*!< State != READY, must close and reopen library */
#define SFX_ERR_SEND_FRAME_RESPONSE_PTR       0x32 /*!< Response data pointer NULL in case of downlink */
#define SFX_ERR_SEND_FRAME_BUILD_UPLINK       0x33 /*!< Build uplink frame failed */
#define SFX_ERR_SEND_FRAME_SEND_UPLINK        0x34 /*!< Send uplink frame failed */
#define SFX_ERR_SEND_FRAME_RECEIVE            0x35 /*!< Receive downlink frame failed or timeout */
#define SFX_ERR_SEND_FRAME_DELAY_OOB_ACK      0x36 /*!< Error on MANUF_API_delay w/ SFX_DLY_OOB_ACK (Downlink) */
#define SFX_ERR_SEND_FRAME_BUILD_OOB_ACK      0x37 /*!< Build out of band frame failed (Downlink) */
#define SFX_ERR_SEND_FRAME_SEND_OOB_ACK       0x38 /*!< Send out of band frame failed (Downlink) */
#define SFX_ERR_SEND_FRAME_DATA_PTR           0x39 /*!< Customer data pointer NULL */
#define SFX_ERR_SEND_FRAME_CARRIER_SENSE_CONFIG  0x3A /*!< Carrier Sense configuration need to be initialized */
#define SFX_ERR_SEND_FRAME_CARRIER_SENSE_TIMEOUT 0x3B /*!< Wait for clear channel has returned time out */
#define SFX_ERR_SEND_FRAME_WAIT_TIMEOUT       0x3E /*!< Wait frame has returned time out */
#define SFX_ERR_SEND_FRAME_INVALID_FH_CHAN    0x3F /*!< FH invalid channel, must call SIGFOX_API_reset */


#define SFX_ERR_SEND_BIT_STATE                0x41 /*!< State != READY, must close and reopen library */
#define SFX_ERR_SEND_BIT_RESPONSE_PTR         0x42 /*!< Response data pointer NULL in case of downlink */
#define SFX_ERR_SEND_BIT_BUILD_UPLINK         0x43 /*!< Build uplink frame failed */
#define SFX_ERR_SEND_BIT_SEND_UPLINK          0x44 /*!< Send uplink frame failed */
#define SFX_ERR_SEND_BIT_RECEIVE              0x45 /*!< Receive downlink frame failed or timeout */
#define SFX_ERR_SEND_BIT_DELAY_OOB_ACK        0x46 /*!< Error on MANUF_API_delay w/ SFX_DLY_OOB_ACK (Downlink) */
#define SFX_ERR_SEND_BIT_BUILD_OOB_ACK        0x47 /*!< Build out of band frame failed (Downlink) */
#define SFX_ERR_SEND_BIT_SEND_OOB_ACK         0x48 /*!< Send out of band frame failed (Downlink) */
#define SFX_ERR_SEND_BIT_DATA_PTR             0x49 /*!< Customer data pointer NULL */
#define SFX_ERR_SEND_BIT_CARRIER_SENSE_CONFIG  0x4A /*!< Carrier Sense configuration need to be initialized */
#define SFX_ERR_SEND_BIT_CARRIER_SENSE_TIMEOUT 0x4B /*!< Wait for clear channel has returned time out */
#define SFX_ERR_SEND_BIT_WAIT_TIMEOUT         0x4E /*!< Wait frame has returned time out */
#define SFX_ERR_SEND_BIT_INVALID_FH_CHAN      0x4F /*!< FH invalid channel, must call SIGFOX_API_reset */

#define SFX_ERR_SEND_OOB_STATE                0x51 /*!< State != READY, must close and reopen library */
#define SFX_ERR_SEND_OOB_BUILD_UPLINK         0x53 /*!< Build uplink frame failed */
#define SFX_ERR_SEND_OOB_SEND_UPLINK          0x54 /*!< Send uplink frame failed */
#define SFX_ERR_SEND_OOB_INVALID_FH_CHAN      0x5F /*!< Send out of band frame failed (Downlink) */

/* 0x006x to 0x008x codes available */

#define SFX_ERR_SET_STD_CONFIG_SIGFOX_CHAN    0x90 /*!< Default SIGFOX channel out of range */
#define SFX_ERR_SET_STD_CONFIG_SET            0x91 /*!< Unable to set configuration */

#define SFX_ERR_TEST_MODE_0_RF_INIT           0xA0 /*!< Error on MANUF_API_rf_init */
#define SFX_ERR_TEST_MODE_0_CHANGE_FREQ       0xA1 /*!< Error on MANUF_API_change_frequency */
#define SFX_ERR_TEST_MODE_0_RF_SEND           0xA2 /*!< Error on MANUF_API_rf_send */
#define SFX_ERR_TEST_MODE_0_DELAY             0xA3 /*!< Error on MANUF_API_delay */
#define SFX_ERR_TEST_MODE_0_RF_STOP           0xA4 /*!< Error on MANUF_API_rf_stop */

#define SFX_ERR_TEST_MODE_STATE               0xB1 /*!< State != READY, must close and reopen library */

#define SFX_ERR_TEST_MODE_2_REPORT_TEST       0xC0 /*!< Error on MANUF_API_report_test_result */

#define SFX_ERR_TEST_MODE_3_RF_INIT           0xD0 /*!< Error on MANUF_API_rf_init */
#define SFX_ERR_TEST_MODE_3_CHANGE_FREQ       0xD1 /*!< Error on MANUF_API_change_frequency */
#define SFX_ERR_TEST_MODE_3_TIMER_START       0xD2 /*!< Error on MANUF_API_timer_start */
#define SFX_ERR_TEST_MODE_3_REPORT_TEST       0xD3 /*!< Error on MANUF_API_report_test_result */
#define SFX_ERR_TEST_MODE_3_TIMER_STOP        0xD4 /*!< Error on MANUF_API_timer_stop */
#define SFX_ERR_TEST_MODE_3_RF_STOP           0xD5 /*!< Error on MANUF_API_rf_stop */

#define SFX_ERR_TEST_MODE_4_BUILD_UPLINK      0xE0 /*!< Build uplink frame failed */
#define SFX_ERR_TEST_MODE_4_SEND_UPLINK       0xE1 /*!< Send uplink frame failed */
#define SFX_ERR_TEST_MODE_4_REPORT_TEST       0xE2 /*!< Error on MANUF_API_report_test_result */
#define SFX_ERR_TEST_MODE_4_GET_RSSI          0xE3 /*!< Error on MANUF_API_get_rssi */
#define SFX_ERR_TEST_MODE_4_DELAY             0xE4 /*!< Error on MANUF_API_delay */

#define SFX_ERR_TEST_MODE_5_RF_INIT           0xF0 /*!< Error on MANUF_API_rf_init */
#define SFX_ERR_TEST_MODE_5_CHANGE_FREQ       0xF1 /*!< Error on MANUF_API_change_frequency */
#define SFX_ERR_TEST_MODE_5_BUILD_UPLINK      0xF2 /*!< Build uplink frame failed */
#define SFX_ERR_TEST_MODE_5_SEND_UPLINK       0xF3 /*!< Send uplink frame failed */
#define SFX_ERR_TEST_MODE_5_RF_STOP           0xF4 /*!< Error on MANUF_API_rf_stop */
/** @}*/


/********************************
 * \enum sfx_nvmem_t
 * \brief Data type for Nv memory access
 * Saving PN and Sequence are mandatory
 * for backend compatibility
 *******************************/
typedef enum
{
    SFX_NVMEM_PN      = 0, /*!< Index of nv memory for PN */
    SFX_NVMEM_SEQ_CPT = 1, /*!< Index of nv memory for Sequence Number */
    SFX_NVMEM_FH      = 2, /*!< Index of nv memory for dedicated FH information */
    SFX_NVMEM_RSSI    = 3, /*!< Index of nv memory for dedicated RSSI offset information */
    SFX_NVMEM_FREQ    = 4, /*!< Index of nv memory for dedicated Frequency offset information */
}sfx_nvmem_t;

/********************************
 * \enum sfx_rf_mode_t
 * \brief Functionnal mode for RF chip
 *******************************/
typedef enum
{
    SFX_RF_MODE_TX = 0,    /*!< Set RF chip as transmitter */
    SFX_RF_MODE_RX = 1,    /*!< Set RF chip as receiver */
    SFX_RF_MODE_CS_RX = 2, /*!< Set RF chip as receiver for Carrier Sense */
}sfx_rf_mode_t;

/********************************
 * \enum sfx_delay_t
 * \brief Delay type
 *******************************/
typedef enum
{
    SFX_DLY_INTER_FRAME_TRX = 0, /*!< Delay inter frames in TX/RX (send frame with initiate_downlink_flag = SFX_TRUE) + FH Uplink : (500ms) */
    SFX_DLY_INTER_FRAME_TX  = 1, /*!< Delay inter frames in TX only (0-2000ms) */
    SFX_DLY_OOB_ACK         = 2, /*!< Delay between frame reception and send followed out of band message (1400ms-4000ms) */
    SFX_DLY_SENSI           = 3, /*!< Delay between RF switching RX to TX in sensitivity test mode */
    SFX_DLY_CS_SLEEP        = 4, /*!< Delay between attempts of carrier sense for the first frame */
}sfx_delay_t;

/********************************
 * \enum sfx_test_mode_t
 * \brief Define all the test mode
 *******************************/
typedef enum
{
    SFX_TEST_MODE_TX_BPSK     = 0,  /*!< only BPSK with Synchro Bit + Synchro frame + PN sequence : no hopping centered on the TX_frequency */
    SFX_TEST_MODE_TX_PROTOCOL = 1,  /*!< with full protocol with AES key defined at SIGFOX_API_open call: send all SIGFOX protocol frames available with hopping */
    SFX_TEST_MODE_RX_PROTOCOL = 2,  /*!< with full protocol with AES key defined at SIGFOX_API_open call: send SIGFOX protocol frames w/ initiate downlink flag = SFX_TRUE */
    SFX_TEST_MODE_RX_GFSK     = 3,  /*!< with known pattern with SB + SF + Pattern on RX_Frequency defined at SIGFOX_API_open function : od internaly compare received frame <=> known pattern and call sfx_test_report() */
    SFX_TEST_MODE_RX_SENSI    = 4,  /*!< Do uplink +  downlink frame with AES key defined at SIGFOX_API_open call but specific shorter timings */
    SFX_TEST_MODE_TX_SYNTH    = 5,  /*!< Do 1 uplink frame on each sigfox channel to measure frequency synthesis step */
}sfx_test_mode_t;

/****************************************/
/*          Sigfox Library API          */
/****************************************/

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_open(sfx_rcz_t *rcz, sfx_u8 *id_ptr)
 * \brief This function initialises library (mandatory). The
 * SIGFOX_API_open function will :
 *  - Allocate memory for library
 *  - Save the input parameters once (Can't be changed till SIGFOX_API_close call)
 *  - Read the non volatile memory content
 *  - Set the global state to SFX_STATE_READY
 *  .
 *
 * \param[in] sfx_rcz_t *rcz                    Pointer on the Radio Configuration Zone :it is mandatory
 *                                              to use already existing RCZx define.
 * \param[in] sfx_u8 *id_ptr                    Pointer for 32 bits ID
 *
 * \retval SFX_ERR_NONE:                        No error
 * \retval SFX_ERR_OPEN_MALLOC:                 Error on MANUF_API_malloc or buffer pointer NULL
 * \retval SFX_ERR_OPEN_ID_PTR:                 ID pointer NULL
 * \retval SFX_ERR_OPEN_GET_SEQ:                Error on MANUF_API_get_nv_mem w/ SFX_NVMEM_SEQ_CPT
 * \retval SFX_ERR_OPEN_GET_PN:                 Error on MANUF_API_get_nv_mem w/ SFX_NVMEM_PN
 * \retval SFX_ERR_OPEN_STATE:                  State is not idle, library should be closed before
 *******************************************************************/
sfx_error_t SIGFOX_API_open(sfx_rcz_t *rcz, sfx_u8 *id_ptr);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_close(void)
 * \brief This function closes the library (Free the allocated memory
 * of SIGFOX_API_open and close RF)
 *
 * \retval SFX_ERR_NONE:                        No error
 * \retval SFX_ERR_CLOSE_FREE:                  Error on MANUF_API_free
 * \retval SFX_ERR_CLOSE_RF_STOP:               Error on MANUF_API_rf_stop
 *******************************************************************/
sfx_error_t SIGFOX_API_close(void);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_send_frame(sfx_u8 *customer_data, sfx_u8 customer_data_length, sfx_u8 *customer_response, sfx_u8 tx_repeat, sfx_bool initiate_downlink_flag)
 * \brief Send a standard SIGFOX frame with customer payload. Customer
 * payload cannot exceed 12 Bytes.<BR>
 * If initiate_downlink_flag is set, the frame will be send as
 * many times as (tx_repeat + 1). tx repeat cannot exceed 2.<BR>
 * If initiate_downlink_flag is unset, the tx_repeat is forced to 2.
 *  - In downlink :
 *      * Send uplink frames (1 to 3)
 *      * Receive downlink frame
 *      * Send out of band frame (Voltage, temperature and RSSI)
 *      .
 *  - In uplink :
 *      * Send uplink frames (3)
 *      .
 *  .
 *
 * \param[in] sfx_u8 *customer_data               Data to transmit
 * \param[in] sfx_u8 customer_data_length         Data length in Bytes
 * \param[out] sfx_u8 *customer_response          Returned 8 Bytes data in case of downlink
 * \param[in] sfx_u8 tx_repeat                    Number of repetition (downlink only)
 * \param[in] sfx_bool initiate_downlink_flag     Downlink flag (when SFX_TRUE)
 *
 * \retval SFX_ERR_NONE:                          No error
 * \retval SFX_ERR_SEND_FRAME_DATA_LENGTH:        Customer data length > 12 Bytes
 * \retval SFX_ERR_SEND_FRAME_STATE:              State != READY, must close and reopen library
 * \retval SFX_ERR_SEND_FRAME_RESPONSE_PTR:       Response data pointer NULL in case of downlink
 * \retval SFX_ERR_SEND_FRAME_BUILD_UPLINK:       Build uplink frame failed
 * \retval SFX_ERR_SEND_FRAME_SEND_UPLINK:        Send uplink frame failed
 * \retval SFX_ERR_SEND_FRAME_RECEIVE:            Receive downlink frame failed or timeout
 * \retval SFX_ERR_SEND_FRAME_DELAY_OOB_ACK:      Error on MANUF_API_delay w/ SFX_DLY_OOB_ACK (Downlink)
 * \retval SFX_ERR_SEND_FRAME_BUILD_OOB_ACK:      Build out of band frame failed (Downlink)
 * \retval SFX_ERR_SEND_FRAME_SEND_OOB_ACK:       Send out of band frame failed (Downlink)
 * \retval SFX_ERR_SEND_FRAME_WAIT_TIMEOUT:       Received frame loop has ended with timeout
 * \retval SFX_ERR_SEND_FRAME_INVALID_FH_CHAN:    FH invalid channel, must call SIGFOX_API_reset
 *******************************************************************/
sfx_error_t SIGFOX_API_send_frame(sfx_u8 *customer_data,
                                  sfx_u8 customer_data_length,
                                  sfx_u8 *customer_response,
                                  sfx_u8 tx_repeat,
                                  sfx_bool initiate_downlink_flag);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_send_bit(sfx_bool bit_value, sfx_u8 *customer_response, sfx_u8 tx_repeat, sfx_bool initiate_downlink_flag)
 * \brief Send a standard SIGFOX frame with null customer payload.
 * This frame is the shortest that SIGFOX library can generate.
 * Data is contained on 1 sfx_bool
 * If initiate_downlink_flag is set, the frame will be send as
 * many times as (tx_repeat + 1). tx repeat cannot exceed 2. If
 * initiate_downlink_flag is unset, the tx_repeat is forced to 2.
 *  - In downlink :
 *      * Send uplink frames (1 to 3)
 *      * Receive downlink frame
 *      * Send out of band frame (Voltage, temperature and RSSI)
 *      .
 *  - In uplink :
 *      * Send uplink frames (3)
 *      .
 *  .
 *
 * \param[in] sfx_bool bit_value                Bit state (SFX_TRUE or SFX_FALSE)
 * \param[out] sfx_u8 *customer_response        Returned 8 Bytes data in case of downlink
 * \param[in] sfx_u8 tx_repeat                  Number of repetition (downlink only)
 * \param[in] sfx_bool initiate_downlink_flag   Downlink flag (when SFX_TRUE)
 *
 * \retval SFX_ERR_NONE:                        No error
 * \retval SFX_ERR_SEND_BIT_STATE:              State != READY, must close and reopen library
 * \retval SFX_ERR_SEND_BIT_RESPONSE_PTR:       Response data pointer NULL in case of downlink
 * \retval SFX_ERR_SEND_BIT_BUILD_UPLINK:       Build uplink frame failed
 * \retval SFX_ERR_SEND_BIT_SEND_UPLINK:        Send uplink frame failed
 * \retval SFX_ERR_SEND_BIT_RECEIVE:            Receive downlink frame failed or timeout
 * \retval SFX_ERR_SEND_BIT_DELAY_OOB_ACK:      Error on MANUF_API_delay w/ SFX_DLY_OOB_ACK (Downlink)
 * \retval SFX_ERR_SEND_BIT_BUILD_OOB_ACK:      Build out of band frame failed (Downlink)
 * \retval SFX_ERR_SEND_BIT_SEND_OOB_ACK:       Send out of band frame failed (Downlink)
 * \retval SFX_ERR_SEND_BIT_WAIT_TIMEOUT:       Received frame loop has ended with timeout
 * \retval SFX_ERR_SEND_BIT_INVALID_FH_CHAN:    FH invalid channel, must call SIGFOX_API_reset
 *******************************************************************/
sfx_error_t SIGFOX_API_send_bit(sfx_bool bit_value,
                                sfx_u8 *customer_response,
                                sfx_u8 tx_repeat,
                                sfx_bool initiate_downlink_flag);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_send_outofband(void)
 * \brief Send an out of band SIGFOX frame.<BR>
 * Data is composed of information about the chip itself (Voltage,
 * Temperature).<BR>
 *  - In uplink :
 *      * Send uplink frames (3)
 *      .
 *  .
 * This function must be called by application every 24hour at least
 * or never if application has some energy critical constraints.
 *
 * \retval SFX_ERR_NONE:                        No error
 * \retval SFX_ERR_SEND_OOB_STATE:              State != READY, must close and reopen library
 * \retval SFX_ERR_SEND_OOB_BUILD_UPLINK:       Build uplink frame failed
 * \retval SFX_ERR_SEND_OOB_SEND_UPLINK:        Send uplink frame failed
 * \retval SFX_ERR_SEND_OOB_INVALID_FH_CHAN:    Send out of band frame failed (Downlink)
 *******************************************************************/
sfx_error_t SIGFOX_API_send_outofband(void);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_reset(void)
 * \brief This function is used for FH and will reset macro channel to
 * default_sigfox_channel configured with SIGFOX_API_set_std_config.<BR>
 * This function should be called only 20 seconds after last transmit
 * on FH channel to be compliant with FCC specifications.<BR>
 * <B>WARNING : This function must be called the first time you boot
 * the device in FCC after called SIGFOX_API_set_std_conf.<BR>
 * <B>This function has no effect in DC.</B><BR>
 * <B>This function has no effect in LBT.</B><BR>
 *
 * \retval SFX_ERR_NONE:                        No error
 *******************************************************************/
sfx_error_t SIGFOX_API_reset(void);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_set_std_config(sfx_u32 config_words[3], sfx_u16 default_sigfox_channel)
 * \brief This function must be used to configure specific variables for standard.
 *        It is mandatory to call this function after SIGFOX_API_open() for FH and LBT.
 *
 * <B> FH (Frequency Hopping )</B>: config words to enable/disable 192KHz macro channels authorized for
 * transmission.<BR>Each macro channel is separated from another of 300 kHz<BR>
 * At least 9 macro channel must be enabled to ensure the
 * minimum of 50 FCC channels (9*6 = 54).<BR> The configured default_sigfox_channel
 * must be at least enabled in configuration word.<BR>
 * <B>WARNING : This function should be call each time you open the library
 * or your FCC configuration will not be applied</B><BR>
 * <B>WARNING : This example and the following table are only available
 * if you set DEFAULT_SIGFOX_FCC_CHANNEL_TABLE_1 in SIGFOX_API_open.</B><BR><BR>
 * <B>Example :</B> To enable Macro channel 1 to 9, that is to say 902.2MHz
 * to 904.8MHz with 902.2MHz as main Macro channel, you must set :<BR>
 * <B>config_words[0]</B> = [0x000001FF]<BR>
 * <B>config_words[1]</B> = [0x00000000]<BR>
 * <B>config_words[2]</B> = [0x00000000]<BR>
 * <B>default_sigfox_channel</B> = 1<BR>
 * \verbatim
   Macro Channel Value MHz : | 902.2MHz | 902.5MHz | 902.8MHz | 903.1MHz | 903.4MHz | 903.7MHz | 904.0MHz | 904.3MHz | 904.6MHz | 904.9MHz | 905.2MHz | ...     ...      | 911.5MHz |
   Macro Channel Value     : | Chn 1    | Chn 2    | Chn 3    | Chn 4    | Chn 5    | Chn 6    | Chn 7    | Chn 8    | Chn 9    | Chn 10   | Chn 11   | ...     ...      | Chn 32   |
   config_words[0] bit     : | bit 0    | bit 1    | bit 2    | bit 3    | bit 4    | bit 5    | bit 6    | bit 7    | bit 8    | bit 9    | bit 10   | ...     ...      | bit 31   |

   Macro Channel Value MHz : | 911.8MHz | 912.1MHz | 912.4MHz | 912.7MHz | 913.0MHz | 913.3MHz | 913.6MHz | 913.9MHz | 914.2MHz | 914.5MHz | 914.8MHz | ...     ...      | 921.1MHz |
   Macro Channel Value     : | Chn 33   | Chn 34   | Chn 35   | Chn 36   | Chn 37   | Chn 38   | Chn 39   | Chn 40   | Chn 41   | Chn 42   | Chn 43   | ...     ...      | Chn 64   |
   config_words[1] bit     : | bit 0    | bit 1    | bit 2    | bit 3    | bit 4    | bit 5    | bit 6    | bit 7    | bit 8    | bit 9    | bit 10   | ...     ...      | bit 31   |

   Macro Channel Value MHz : | 921.4MHz | 921.7MHz | 922.0MHz | 922.3MHz | 922.6MHz | 922.9MHz | 923.2MHz | 923.5MHz | 923.8MHz | 924.1MHz | 924.4MHz | ... | 927.7MHz |
   Macro Channel Value     : | Chn 65   | Chn 66   | Chn 67   | Chn 68   | Chn 69   | Chn 70   | Chn 71   | Chn 72   | Chn 73   | Chn 74   | Chn 75   | ... | Chn 86   |
   config_words[2] bit     : | bit 0    | bit 1    | bit 2    | bit 3    | bit 4    | bit 5    | bit 6    | bit 7    | bit 8    | bit 9    | bit 10   | ... | bit 21   |
   \endverbatim
 *
 * <B>DC (Duty Cycle)</B>: This function has no effect in DC spectrum access ( used for the ETSI standard ).</B><BR>
 *
 * <B>LBT (Listen Before Talk)</B> : Carrier Sense feature for the First frame can be configured.
 *           - config_word[0] : number of attempts to send the first frame [ has to be greater or equal to 1]
 *           - config_word[1] : maximum carrier sense sliding window (in ms) [ has to be greater than 6 ms ( CS_MIN_DURATION_IN_MS + 1 ) ]
 *           - config_word[2] :
 *                  . bit 8   : set the value to 1 to indicate that the device will use the full operationnal radio band.( 192kHz )
 *                  . bit 7-3 : number of Carrier Sense attempts.
 *                  . bit 2-0 : number of frames sent.
 *           - default_sigfox_channel : unused
 * <BR><BR>
 * The delay between several attempts of Carrier Sense for the first frame is set by SFX_DLY_CS_SLEEP
 *
 * \param[in] sfx_u32 config_words[3]           Meaning depends on the standard (as explained above)
 * \param[in] sfx_u16 default_sigfox_channel    Default SIGFOX FCC channel (1 to 82)
 *
 * \retval SFX_ERR_NONE:                        No error
 * \retval SFX_ERR_SET_STD_CONFIG_SIGFOX_CHAN:  default_sigfox_channel is out of range
 * \retval SFX_ERR_SET_STD_CONFIG_SET:          Unable to set configuration
 *******************************************************************/
sfx_error_t SIGFOX_API_set_std_config(sfx_u32 config_words[3],
                                      sfx_u16 default_sigfox_channel);
/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_get_version(sfx_u8** version, sfx_u8* size)
 * \brief Returns current SIGFOX library version coded in ASCII
 *
 * \param[out] sfx_u8** version                 Pointer to Byte array containing library version
 * \param[out] sfx_u8* size                     Size of version buffer
 *
 * \retval SFX_ERR_NONE:                        No error
 *******************************************************************/
sfx_error_t SIGFOX_API_get_version(sfx_u8** version,
                                   sfx_u8* size);

/*!******************************************************************
 * \fn sfx_error_t SIGFOX_API_test_mode(sfx_test_mode_t test_mode, sfx_u8 config)
 * \brief This function is used for protocol/RF/sensitivity tests.
 * All tests must be accessible for UNB modem Qualification P1.
 *
 *  - A1 <B>If test_mode = SFX_TEST_MODE_TX_BPSK (TEST_MODE_0).</B><BR>
 *  This test consists in sending PRBS data in a 26 Bytes frame @ constant frequency.<BR>
 *  Bit 7 of config defines if a delay is applied of not in the loop (continuous
 *  modulated wave mode for power measuring).<BR>
 *  This test is used to check :
 *      - 1 Spectrum analysis
 *      - 2 Modulation bit rate
 *      - 3 Modulation phase
 *      - 4 Ramp up sequence
 *      - 5 Ramp down sequence
 *      - 6 Dynamic drift
 *      .
 *  - A2 <B>SFX_TEST_MODE_TX_BPSK Schedule :</B>
 *      - 1 Init RF w/ SFX_RF_MODE_TX
 *      - 2 Change synthesizer frequency @ tx_frequency configured in SIGFOX_API_open
 *      - 3 Loop 0 to (config & 0x7F) (input argument of function)
 *          - 3a Build a 26 Bytes frame based on PRBS generator
 *          - 3b Send it over RF
 *          - 3c (if bit 7 of config == 0) Apply delay w/ SFX_DLY_INTER_FRAME_TRX else no delay
 *          .
 *      - 4 End Loop
 *      - 5 Close RF
 *      .
 *  .
 *  ______________________________________________________________________________________________
 *  - B1 <B>If test_mode = SFX_TEST_MODE_TX_PROTOCOL (TEST_MODE_1).</B><BR>
 *  This test consists in calling SIGFOX_API_send_xxx functions to test the
 *  complete protocol in Uplink only.<BR>
 *  This test is used to check :
 *      - 1 Frames w/ 1 Byte to 12 Bytes payloads
 *      - 2 Bit frames
 *      - 3 Out of band frame (and Voltage + Temperature consistency in payload)
 *      - 4 Sequence number saving
 *      - 5 Frequency hopping
 *      - 6 Static drift
 *      - 7 Interframe delay w/ SFX_DLY_INTER_FRAME_TX
 *      .
 *  - B2 <B>SFX_TEST_MODE_TX_PROTOCOL Schedule :</B>
 *      - Loop 0 to config (input argument of function)
 *          - 1 Call SIGFOX_API_send_bit w/ bit_value = SFX_FALSE
 *          - 2 Call SIGFOX_API_send_bit w/ bit_value = SFX_TRUE
 *          - 3 Call SIGFOX_API_send_outofband
 *          - 4 Loop 1 to 12
 *              - 4a Call SIGFOX_API_send_frame w/ customer_data_length = loop. Data = 0x30 to 0x3B
 *              .
 *          .
 *      - End Loop
 *      .
 *  .
 *  ______________________________________________________________________________________________
 *  - C1 <B>If test_mode = SFX_TEST_MODE_RX_PROTOCOL (TEST_MODE_2).</B><BR>
 *  This test consists in calling SIGFOX_API_send_xxx functions to test the
 *  complete protocol in Downlink only.<BR>
 *  This test is used to check :
 *      - 1 Downlink frames
 *      - 2 Interframe delay w/ SFX_DLY_INTER_FRAME_TRX
 *      - 3 Out of band frame (and Voltage + Temperature + RSSI consistency in payload)
 *      - 4 20 seconds wait window
 *      - 5 25 seconds listening window
 *      - 6 Delay w/ SFX_DLY_OOB_ACK
 *      .
 *  - C2 <B>SFX_TEST_MODE_RX_PROTOCOL Schedule :</B>
 *      - Loop 0 to config (input argument of function)
 *          - 1 Call SIGFOX_API_send_frame w/ customer_data_length = 12, Data = 0x30 to 0x3B,
 *          downlink_flag = SFX_TRUE, tx_repeat = 2.
 *          .
 *      - End Loop
 *      .
 *  .
 *  ______________________________________________________________________________________________
 *  - D1 <B>If test_mode = SFX_TEST_MODE_RX_GFSK (TEST_MODE_3).</B><BR>
 *  This test consists in receiving constant GFSK frames @ constant frequency.<BR>
 *  The pattern used for test is : <B>AA AA B2 27 1F 20 41 84 32 68 C5 BA 53 AE 79 E7 F6 DD 9B</B>
 *  with <B>AA AA B2 27</B> configured in RF chip<BR>
 *  This test is used to check :
 *      - 1 GFSK receiver
 *      - 2 Approximative sensitivity
 *      .
 *  - D2 <B>SFX_TEST_MODE_RX_GFSK Schedule :</B>
 *      - 1 Init RF w/ SFX_RF_MODE_RX
 *      - 2 Change synthesizer frequency @ rx_frequency configured in SIGFOX_API_open
 *      - 3 Start timer w/ config in seconds (input argument of function)
 *      - 4 While MANUF_API_wait_frame does not return TIME_OUT
 *          - 4a Read data from RF chip
 *          - 4b Compare w/ constant pattern (above)
 *          - 4b If OK (pattern == received_data), call report_test_result w/ SFX_TRUE else SFX_FALSE
 *          .
 *      - 5 End Loop
 *      - 6 Stop timer
 *      - 7 Close RF
 *      .
 *  .
 *  ______________________________________________________________________________________________
 *  - E1 <B>If test_mode = SFX_TEST_MODE_RX_SENSI (TEST_MODE_4).</B><BR>
 *  This test is specific to SIGFOX's test equipments & softwares.<BR>
 *  It is mandatory to measure the real sensitivity of device.<BR>
 *  This test is used to check :
 *      - 1 Device sensitivity
 *      .
 *  - E2 <B>SFX_TEST_MODE_RX_SENSI Schedule :</B>
 *      - Loop 0 to (config x 10) (input argument of function)
 *          - 1 Send 1 Uplink Frame @ tx_frequency + hopping
 *          - 2 Receive 1 Downlink Frame @ rx_frequency + hopping
 *          - 3 If OK, call report_test_result w/ SFX_TRUE else SFX_FALSE
 *          - 4 Delay w/ SFX_DLY_SENSI
 *          .
 *      - End Loop
 *      .
 *  .
 *  ______________________________________________________________________________________________
 *
 *  - F1 <B>If test_mode = SFX_TEST_MODE_TX_SYNTH (TEST_MODE_5).</B><BR>
 *  This test consists in sending SIGFOX frames with 4Bytes payload @ forced frequency.<BR>
 *  This test is used to check :
 *      - 1 Synthetiser's frequency step
 *      - 2 Static drift
 *      .
 *  - F2 <B>SFX_TEST_MODE_TX_SYNTH Schedule :</B>
 *      - 1 Init RF w/ SFX_RF_MODE_TX
 *      - 2 Loop 0 to Maximum number of 100 Hz channels
 *          - 2a Change synthesizer frequency @ forced frequency
 *          - 2b Build a 4 Bytes payload frame w/ forced frequency value
 *          - 2c Send it over RF
 *          .
 *      - 3 End Loop
 *      - 4 Close RF
 *      .
 *  .
 *
 * \param[in] sfx_test_mode_t test_mode           Test mode selection
 * \param[in] sfx_u8 config                       The use of config depends on test mode (see above)
 *
 * \retval SFX_ERR_TEST_MODE_STATE:               State != READY, must close and reopen library
 * \retval SFX_ERR_TEST_MODE_0_RF_INIT:           Error on MANUF_API_rf_init
 * \retval SFX_ERR_TEST_MODE_0_CHANGE_FREQ:       Error on MANUF_API_change_frequency
 * \retval SFX_ERR_TEST_MODE_0_RF_SEND:           Error on MANUF_API_rf_send
 * \retval SFX_ERR_TEST_MODE_0_DELAY:             Error on MANUF_API_delay
 * \retval SFX_ERR_TEST_MODE_0_RF_STOP:           Error on MANUF_API_rf_stop
 * \retval SFX_ERR_TEST_MODE_2_REPORT_TEST:       Error on MANUF_API_report_test_result
 * \retval SFX_ERR_TEST_MODE_3_RF_INIT:           Error on MANUF_API_rf_init
 * \retval SFX_ERR_TEST_MODE_3_CHANGE_FREQ:       Error on MANUF_API_change_frequency
 * \retval SFX_ERR_TEST_MODE_3_TIMER_START:       Error on MANUF_API_timer_start
 * \retval SFX_ERR_TEST_MODE_3_WAIT_FRAME:        Error on MANUF_API_wait_frame
 * \retval SFX_ERR_TEST_MODE_3_REPORT_TEST:       Error on MANUF_API_report_test_result
 * \retval SFX_ERR_TEST_MODE_3_TIMER_STOP:        Error on MANUF_API_timer_stop
 * \retval SFX_ERR_TEST_MODE_3_RF_STOP:           Error on MANUF_API_rf_stop
 * \retval SFX_ERR_TEST_MODE_4_BUILD_UPLINK:      Build uplink frame failed
 * \retval SFX_ERR_TEST_MODE_4_SEND_UPLINK:       Send uplink frame failed
 * \retval SFX_ERR_TEST_MODE_4_REPORT_TEST:       Error on MANUF_API_report_test_result
 * \retval SFX_ERR_TEST_MODE_4_GET_RSSI:          Error on MANUF_API_get_rssi
 * \retval SFX_ERR_TEST_MODE_4_DELAY:             Error on MANUF_API_delay
 * \retval SFX_ERR_TEST_MODE_5_RF_INIT:           Error on MANUF_API_rf_init
 * \retval SFX_ERR_TEST_MODE_5_CHANGE_FREQ:       Error on MANUF_API_change_frequency
 * \retval SFX_ERR_TEST_MODE_5_BUILD_UPLINK:      Build uplink frame failed
 * \retval SFX_ERR_TEST_MODE_5_SEND_UPLINK:       Send uplink frame failed
 * \retval SFX_ERR_TEST_MODE_5_RF_STOP:           Error on MANUF_API_rf_stop
 *******************************************************************/
sfx_error_t SIGFOX_API_test_mode(sfx_test_mode_t test_mode,
                                 sfx_u8 config);

 /*!******************************************************************
  * \fn sfx_error_t SIGFOX_API_get_info(sfx_u8* info);
  * \brief This function is to return info on send frame depending on
  * the mode you're using.<BR>
  * <B> In DC  :</B> returned_info is always 0.<BR>
  * <B> In FH  :</B> returned_info[bit 3 - 0] = 1 when the current FCC marco channel
  * is NOT the default_sigfox_channel set in SIGFOX_API_set_std_conf.<BR>
  * returned_info[bit 7 - 4] = number of free micro channel in current FCC
  * macro channel.<BR>
  * <B> In LBT :</B> returned_info = bit[7-3]: Carrier Sense attempts
  * and bit[2-0]: Number of frames sent
  *
  * \param[out] sfx_u8* returned_info            Returned value by library
  *
  * \retval SFX_ERR_NONE:                        No error
  *******************************************************************/
 sfx_error_t SIGFOX_API_get_info(sfx_u8* returned_info);


#endif /* SIGFOX_API_H_ */
