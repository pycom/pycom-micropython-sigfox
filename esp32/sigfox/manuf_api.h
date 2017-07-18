/*!
 \if SIGFOX PATTERN
 ----------------------------------------------

   _____   _   _____   _____   _____  __    __      
  /  ___/ | | /  ___| |  ___| /  _  \ \ \  / /      
  | |___  | | | |     | |__   | | | |  \ \/ /       
  \___  \ | | | |  _  |  __|  | | | |   }  {        
   ___| | | | | |_| | | |     | |_| |  / /\ \
  /_____/ |_| \_____/ |_|     \_____/ /_/  \_\

  ----------------------------------------------

    !!!!  DO NOT MODIFY THIS FILE !!!!

  ----------------------------------------------
 \endif
  ----------------------------------------------*/

/*!
 * \file manuf_api.h
 * \brief Sigfox manufacturer functions
 * \author $(SIGFOX_LIB_AUTHOR)
 * \version $(SIGFOX_LIB_VERSION)
 * \date $(SIGFOX_LIB_DATE)
 * \copyright Copyright (c) 2011-2015 SIGFOX, All Rights Reserved. This is unpublished proprietary source code of SIGFOX.
 *
 * This file defines the manufacturer's functions to be implemented
 * for library usage.
 */

#ifndef MANUF_API_H_
#define MANUF_API_H_

#include "sigfox_types.h"
#include "sigfox_api.h"

/********************************************************
 * external API dependencies to link with this library.
 * Error codes for manufacturer api can be found in
 * sigfox_api.h. New error codes can be added. Each
 * Manufacturer error code is returned by SIGFOX_API
 * functions
 ********************************************************/

/*!
 * \defgroup SFX_ERR_MANUF_CODES Return Error codes definition for MANUFACTURER APIs
 *
 * \brief Can be customized to add new error codes.
 * All MANUF_API_ error codes will be piped with SIGFOX_API_xxx return code.<BR>
 * Bellow is an example of 1 error code per MANUF_API_ function (5 MSB for
 * 1 API definition & 3 LSB for intermediate error codes).<BR>
 * SFX_ERR_MANUF_NONE implementation is mandatory for each MANUF_API_xxx
 * functions.
 *
 *  @{
 */
#define SFX_ERR_MANUF_NONE                    0x00 /*!< No error */
#define SFX_ERR_MANUF_MALLOC                  0x08 /*!< Error on MANUF_API_malloc */
#define SFX_ERR_MANUF_INIT                    0x10 /*!< Error on MANUF_API_rf_init */
#define SFX_ERR_MANUF_SEND                    0x18 /*!< Error on MANUF_API_rf_send */
#define SFX_ERR_MANUF_FREE                    0x20 /*!< Error on MANUF_API_free */
#define SFX_ERR_MANUF_VOLT_TEMP               0x28 /*!< Error on MANUF_API_get_voltage_temperature */
#define SFX_ERR_MANUF_RF_STOP                 0x30 /*!< Error on MANUF_API_rf_stop */
#define SFX_ERR_MANUF_DLY                     0x38 /*!< Error on MANUF_API_delay */
#define SFX_ERR_MANUF_CH_FREQ                 0x40 /*!< Error on MANUF_API_change_frequency */
#define SFX_ERR_MANUF_AES                     0x48 /*!< Error on MANUF_API_aes_128_cbc_encrypt */
#define SFX_ERR_MANUF_GETNVMEM                0x50 /*!< Error on MANUF_API_get_nv_mem */
#define SFX_ERR_MANUF_SETNVMEM                0x58 /*!< Error on MANUF_API_set_nv_mem */
#define SFX_ERR_MANUF_GET_RSSI                0x60 /*!< Error on MANUF_API_get_rssi */
#define SFX_ERR_MANUF_GET_MCU_SERIAL          0x61 /*!< Error on MANUF_API_get_mcu_serial */
#define SFX_ERR_MANUF_GET_RF_SERIAL           0x62 /*!< Error on MANUF_API_get_rf_serial */
#define SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT      0x68 /*!< Error on MANUF_API_wait_frame */
#define SFX_ERR_MANUF_WAIT_CS_TIMEOUT         0x69 /*!< Error on MANUF_API_wait_for_clear_channel */
#define SFX_ERR_MANUF_TIMER_START             0x70 /*!< Error on MANUF_API_timer_start */
#define SFX_ERR_MANUF_TIMER_START_CS          0x71 /*!< Error on MANUF_API_timer_start_carrier_sense */
#define SFX_ERR_MANUF_TIMER_STOP_CS           0x72 /*!< Error on MANUF_API_timer_stop_carrier_sense */
#define SFX_ERR_MANUF_TIMER_STOP              0x78 /*!< Error on MANUF_API_timer_stop */
#define SFX_ERR_MANUF_TIMER_END               0x80 /*!< Error on MANUF_API_timer_wait_for_end */
#define SFX_ERR_MANUF_TEST_REPORT             0x88 /*!< Error on MANUF_API_report_test_result */
/** @}*/

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_malloc(sfx_u16 size, sfx_u8 **returned_pointer)
 * \brief Allocate memory for library usage (Memory usage = size (Bytes))
 *
 * IMPORTANT NOTE:
 * --------------
 * The address reported need to be aligned with architecture of the microprocessor used.
 * For a Microprocessor of:
 *   - 8bits   => any address is allowed
 *   - 16 bits => only address multiple of 2 are allowed
 *   - 32 bits => only address multiple of 4 are allowed
 *
 * \param[in] sfx_u16 size                  size of buffer to allocate in Bytes
 * \param[out] sfx_u8** returned_pointer    pointer to buffer (can be static)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_MALLOC:            Malloc error
 *******************************************************************/
sfx_error_t MANUF_API_malloc(sfx_u16 size, sfx_u8 **returned_pointer);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_free(sfx_u8 *p)
 * \brief Free memory allocated to library
 *
 * \param[in] sfx_u8 *p                     pointer to buffer
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_FREE:              Free error
 *******************************************************************/
sfx_error_t MANUF_API_free(sfx_u8 *p);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_get_voltage_temperature(sfx_u16 *voltage_idle, sfx_u16 *voltage_tx, sfx_u16 *temperature)
 * \brief Get voltage and temperature for Out of band frames
 * Value must respect the units bellow for <B>backend compatibility</B>
 *
 * \param[out] sfx_u16 *voltage_idle        Device's voltage in Idle state (mV)
 * \param[out] sfx_u16 *voltage_tx          Device's voltage in Tx state (mV)
 * \param[out] sfx_u16 *temperature         Device's temperature in 1/10 of degrees celcius
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_VOLT_TEMP:         Get voltage/temperature error
 *******************************************************************/
sfx_error_t MANUF_API_get_voltage_temperature(sfx_u16 *voltage_idle,
                                              sfx_u16 *voltage_tx,
                                              sfx_u16 *temperature);

/*!******************************************************************
 * \fn sfx_error_t sfx_error_t MANUF_API_rf_init(sfx_rf_mode_t rf_mode)
 * \brief Init and configure Radio link in RX/TX
 *
 * [RX Configuration]
 * To receive Sigfox Frame on your device, program the following:
 *  - Preamble  : 0xAAAAAAAAA
 *  - Sync Word : 0xB227
 *  - Packet of the Sigfox frame is 15 bytes length.
 *
 * \param[in] sfx_rf_mode_t rf_mode         Init Radio link in Tx or RX
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_INIT:              Init Radio link error
 *******************************************************************/
sfx_error_t MANUF_API_rf_init(sfx_rf_mode_t rf_mode);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_rf_stop(void)
 * \brief Close Radio link
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_RF_STOP:           Close Radio link error
 *******************************************************************/
sfx_error_t MANUF_API_rf_stop(void);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_rf_send(sfx_u8 *stream, sfx_u8 size)
 * \brief BPSK Modulation of data stream
 * (from synchro bit field to CRC)
 *
 * \param[in] sfx_u8 *stream                Complete stream to modulate
 * \param[in] sfx_u8 size                   Length of stream
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_SEND:              Send data stream error
 *******************************************************************/
sfx_error_t MANUF_API_rf_send(sfx_u8 *stream,
                              sfx_u8 size);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_delay(sfx_delay_t delay_type)
 * \brief Inter stream delay, called between each MANUF_API_rf_send
 * - SFX_DLY_INTER_FRAME_TX  : 0 to 2s in Uplink DC
 * - SFX_DLY_INTER_FRAME_TRX : 500 ms in Uplink/Downlink FH & Downlink DC 
 * - SFX_DLY_OOB_ACK :         1.4s to 4s for Downlink OOB
 * - SFX_DLY_SENSI :           4s for sensitivity test mode
 * - SFX_CS_SLEEP :            delay between several trials of Carrier Sense ( for the first frame only ) 
 *
 * \param[in] sfx_delay_t delay_type        Type of delay to call
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_DLY:               Delay error
 *******************************************************************/
sfx_error_t MANUF_API_delay(sfx_delay_t delay_type);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_change_frequency(sfx_u32 frequency)
 * \brief Change synthesizer carrier frequency
 *
 * \param[in] sfx_u32 frequency             Frequency in Hz to program in the radio chipset
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_CH_FREQ:           Change frequency error
 *******************************************************************/
sfx_error_t MANUF_API_change_frequency(sfx_u32 frequency);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_aes_128_cbc_encrypt(sfx_u8 *encrypted_data, sfx_u8 *data_to_encrypt, sfx_u8 data_len);
 * \brief Encrypt a complete buffer with Secret or Test key.<BR>
 * <B>These keys must be stored in a secure place.</B> <BR>
 * Can be hardcoded or soft coded (iv vector contains '0')
 *
 * \param[out] sfx_u8 *encrypted_data       Result of AES Encryption
 * \param[in] sfx_u8 *data_to_encrypt       Input data to Encrypt
 * \param[in] sfx_u8 data_len               Input data length
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_AES:               AES Encryption error
 *******************************************************************/
sfx_error_t MANUF_API_aes_128_cbc_encrypt(sfx_u8 *encrypted_data,
                                          sfx_u8 *data_to_encrypt,
                                          sfx_u8 data_len);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_get_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 *return_value)
 * \brief Get value from an index in non volatile memory
 *
 * \param[in] sfx_nvmem_t nvmem_datatype    Type of data to read
 * \param[out] sfx_u16 *return_value        Read value from memory
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_GETNVMEM:          Read nvmem error
 *******************************************************************/
sfx_error_t MANUF_API_get_nv_mem(sfx_nvmem_t nvmem_datatype,
                                 sfx_u16 *return_value);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_set_nv_mem(sfx_nvmem_t nvmem_datatype, sfx_u16 value)
 * \brief Set value to an index in non volatile memory.<BR> It is strongly
 * recommanded to use NV memory like EEPROM since this function
 * is called 2 times per SIGFOX_API_send_xxx and 3 times in
 * downlink (4 Bytes to reserve in memory)
 *
 * \param[in] sfx_nvmem_t nvmem_datatype    Type of data to write
 * \param[in] sfx_u16 value                 Value to write in memory
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_SETNVMEM:          Write nvmem error
 *******************************************************************/
sfx_error_t MANUF_API_set_nv_mem(sfx_nvmem_t nvmem_datatype,
                                 sfx_u16 value);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_get_rssi(sfx_s8 *rssi)
 * \brief Get RSSI level for out of band frame in downlink
 * Returned RSSI = Chipset RSSI + 100 <B>(backend compatibility)</B>
 * Example : Chipset RSSI = -126 dBm, Returned RSSI = -26.
 *
 * \param[out] sfx_s8 *rssi                 Returned signed RSSI
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_GET_RSSI:          Read RSSI error
 *******************************************************************/
sfx_error_t MANUF_API_get_rssi(sfx_s8 *rssi);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_wait_frame(sfx_u8 *frame)
 * \brief Get all GFSK frames received in Rx buffer, structure of
 * frame is : Synchro bit + Synchro frame + 15 Bytes.<BR> This function must
 * be blocking state since data is received or timer of 25 s has elapsed.
 *
 * - If received buffer, function returns SFX_ERR_MANUF_NONE then the
 *   library will try to decode frame. If the frame is not correct, the
 *   library will recall MANUF_API_wait_frame .
 *
 * - If 25 seconds timer has elapsed, function returns
 *   SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT then library will stop receive
 *   frame phase.
 *
 * \param[out] sfx_s8 *frame                  Receive buffer
 *
 * \retval SFX_ERR_MANUF_NONE:                No error
 * \retval SFX_ERR_MANUF_WAIT_FRAME_TIMEOUT:  Wait frame error
 *******************************************************************/
sfx_error_t MANUF_API_wait_frame(sfx_u8 *frame);


/*!******************************************************************
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
 *******************************************************************/
sfx_error_t MANUF_API_wait_for_clear_channel ( sfx_u8 cs_min,
                                               sfx_u8 cs_threshold);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_timer_start_carrier_sense(sfx_u16 time_duration_in_ms)
 * \brief Start timer for : 
 * - carrier sense maximum window ( used in ARIB standard )
 *
 * \param[in] sfx_u16 time_duration_in_ms    Timer value in milliseconds
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_START_CS:    Start CS timer error
 *******************************************************************/
sfx_error_t MANUF_API_timer_start_carrier_sense(sfx_u16 time_duration_in_ms);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_timer_start(sfx_u16 time_duration_in_s)
 * \brief Start timer for : 
 * - 20 seconds wait in downlink
 * - 25 seconds listening in downlink
 * - 6 seconds listening in sensitivity test mode
 * - 0 to 255 seconds listening in GFSK test mode (config argument)
 *
 * \param[in] sfx_u16 time_duration_in_s    Timer value in seconds
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_START:       Start timer error
 *******************************************************************/
sfx_error_t MANUF_API_timer_start(sfx_u16 time_duration_in_s);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_timer_stop(void)
 * \brief Stop the timer (started with MANUF_API_timer_start)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_STOP:        Stop timer error
 *******************************************************************/
sfx_error_t MANUF_API_timer_stop(void);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_timer_stop_carrier_sense(void)
 * \brief Stop the timer (started with MANUF_API_timer_start_carrier_sense)
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_STOP_CS:     Stop timer error
 *******************************************************************/
sfx_error_t MANUF_API_timer_stop_carrier_sense(void);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_timer_wait_for_end(void)
 * \brief Blocking function to wait for interrupt indicating timer
 * elapsed.<BR> This function is only used for the 20 seconds wait
 * in downlink.
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TIMER_END:         Wait end of timer error
 *******************************************************************/
sfx_error_t MANUF_API_timer_wait_for_end(void);

/*!******************************************************************
 * \fn sfx_error_t MANUF_API_report_test_result(sfx_bool status)
 * \brief To report the result of Rx test for each valid message
 * received/validated by library.<BR> Manufacturer api to show the result
 * of RX test mode : can be uplink radio frame or uart print or
 * gpio output.
 *
 * \param[in] sfx_bool status               Is SFX_TRUE when result ok else SFX_FALSE
 *                                          See SIGFOX_API_test_mode summary
 *
 * \retval SFX_ERR_MANUF_NONE:              No error
 * \retval SFX_ERR_MANUF_TEST_REPORT:       Report test result error
 *******************************************************************/
sfx_error_t MANUF_API_report_test_result(sfx_bool status);

#endif /* MANUF_API_H_ */
