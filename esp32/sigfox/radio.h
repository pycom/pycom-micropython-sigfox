//*****************************************************************************
//! @file       radio.c
//! @brief      Manage the low level modulation and configuration of the TI chipset
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
 * @addtogroup Radio
 * @{
 ******************************************************************************/


#ifndef RADIO_H
#define RADIO_H


/******************************************************************************
 * INCLUDES
 */
#include "sigfox_api.h"
#include "targets/cc112x_spi.h"


/******************************************************************************
 * FUNCTION PROTOTYPES
 */
void RADIO_init_chip(sfx_rf_mode_t rf_mode);
void RADIO_close_chip(void);
void RADIO_change_frequency(unsigned long ul_Freq);
void RADIO_modulate(void);
void RADIO_start_rf_carrier(void);
void RADIO_stop_rf_carrier(void);
void RADIO_start_unmodulated_cw(unsigned long ul_Freq);
void RADIO_stop_unmodulated_cw(unsigned long ul_Freq);


/******************************************************************************
* DEFINES
*/
#define ISR_ACTION_REQUIRED 1
#define ISR_IDLE            0

extern uint8  packetSemaphore;
extern sfx_u16 offset_value;

#define GAIN_ADJUST_VALUE           0x9D            // +14dBm board without the LNA
#define FREQ_ADJUST_VALUE           1000            // SiPy has a deviation of -1KHz at 25 C.

/******************************************************************************
* CC112X Register Settings
*/

/*! CC112x High Performance TX register settings. { Register address    ,   Value } */
static const registerSetting_t HighPerfModeTx[] = {
    /* Register address     ,   Value */
    {  CC112X_FS_DIG1       ,   0x00 },   /* Frequency Synthesizer Digital Reg. 1 NOT USED                             */
    {  CC112X_FS_DIG0       ,   0x5f },   /* Frequency Synthesizer Digital Reg. 0
                                             - RX_LPF_BW (FS loop bandwidth in RX ) = 170.8 kHz
                                             - TX_LPF_BW (FS loop bandwidth in TX ) = 170.8 kHz
                                          */

    { CC112X_IOCFG0,            0x73},    /* Enable the PA */
    { CC112X_IOCFG2,            0x33},    /* Disable the LNA */
    { CC112X_IOCFG3,            0x33},    /* Reset GPIO3 config */


    /* Frequency Synthesizer Calibration */
    {  CC112X_FS_CAL2       ,   0x20 },   /* FS_CAL2 : VCDAC_START = 0x20                                              */
    {  CC112X_FS_CAL1       ,   0x40 },   /* FS_CAL1 : FOR TEST PURPOSE - NOT USED                                     */
    {  CC112X_FS_CAL0       ,   0x0e },   /* FS_CAL0 : LOCK_CFG : Out of lock detector average time = Infinite average */
    {  CC112X_FS_CAL3       ,   0x08 },   /* FS_CAL3 : KVCO_HIGH_RES_CFG : High Resolution Enable                      */

    {  CC112X_FS_DIVTWO     ,   0x03 },   /* Frequency Synthesizer Divide by 2 - NOT USED                              */
    {  CC112X_FS_DSM1       ,   0x00 },   /* FS Digital Synthesizer Module Configuration Reg. 1 - NOT USED             */
    {  CC112X_FS_DSM0       ,   0x33 },   /* FS Digital Synthesizer Module Configuration Reg. 0 - NOT USED             */
    {  CC112X_FS_DVC1       ,   0xff },   /* Frequency Synthesizer Divider Chain Configuration Reg. 1 - NOT USED       */
    {  CC112X_FS_DVC0       ,   0x17 },   /* Frequency Synthesizer Divider Chain Configuration Reg. 0 - NOT USED       */

    {  CC112X_FS_PFD        ,   0x50 },   /* Frequency Synthesizer Phase Frequency Detector Configuration - NOT USED   */
    {  CC112X_FS_PRE        ,   0x6e },   /* Frequency Synthesizer Prescaler Configuration - NOT USED                  */
    {  CC112X_FS_REG_DIV_CML,   0x14 },   /* Frequency Synthesizer Divider Regulator Configuration - NOT USED          */
    {  CC112X_FS_SPARE      ,   0xac },   /* Frequency Synthesizer Spare - IMPORTANT parameter                         */
    {  CC112X_FS_VCO0       ,   0xb4 },   /* FS Voltage Controlled Oscillator Configuration Reg. 0 - NOT USED          */
    {  CC112X_XOSC5         ,   0x0e },   /* Crystal Oscillator Configuration Reg. 5 - NOT USED                        */
    {  CC112X_XOSC1         ,   0x03 },   /* Crystal Oscillator Configuration Reg. 1 -
                                           * - XOSC_BUF_SEL - XOSC buffer select. Selects internal XOSC buffer for RF PLL
                                           *    1 => Low phase noise, differential buffer (low power buffer still used for digital clock)
                                           *  - XOSC_STABLE ( Read bit ) XOSC is stable (has finished settling)
                                           */

    { CC112X_EXT_CTRL        ,  0x00 },   /* External Control Configuration
                                           *  - BURST_ADDR_INCR_EN set to 0 (disabled)
                                           * (i.e. consecutive writes to the same address location in burst mode)
                                           */

    { CC112X_PA_CFG2        ,   0x7F },   /* Power Amplifier Configuration Reg. 2 - PA_POWER_RAMP - PA power ramp target level */
    { CC112X_PA_CFG1        ,   0x56 },   /* Power Amplifier Configuration Reg. 1
                                           *  - FIRST_IPL ( First Intermediate Power Level ) = 0x02
                                           *  - SECOND_IPL = 0x05
                                           *  - RAMP_SHAPE = 3 symbol ramp time and 1/8 symbol ASK/OOK shape length
                                           */
    { CC112X_PA_CFG0        ,   0x1C },    /*! Power Amplifier Configuration Reg. 0
                                              - ASK_DEPTH = 0x0E
                                              - UPSAMPLER_P = 0x04 TX upsampler Factor P = 16 */

    { CC112X_CFM_DATA_CFG   ,   0x01 },   /* Custom Frequency Modulation Configuration
                                           *  CFM_DATA_EN : Custom Frequency Modulation Enabled
                                           */
    { CC112X_FREQOFF_CFG    ,   0x22 },   /* Frequency Offset Correction Configuration
                                           *  - FOC_EN : Frequency offset correction enabled
                                           *  - FOC_CFG : FOC after channel filter (typical 0 - 1 preamble bytes for settling)
                                           *  - FOC_KI_FACTOR : Frequency offset correction :
                                           *    Frequency offset compensation during packet reception with loop gain factor = 1/64
                                           */
};

/*! CC112x High Performance TX register settings. { Register address    ,   Value } */
static const registerSetting_t HighPerfModeRx[] =
{
	/*! IOCFG3 is configured as followed :
		- Analog Transfer Disabled
		- Invert Mode Disabled
		- GPIO3_CFG set to signal RxTx    									 	*/
	{ CC112X_IOCFG3			,	0x06 },

	/*! Set the CC1190 in RX mode. LNA_EN has to be set to 1.
		IOCFG2 is configured as followed :
		- Analog Transfer Disable
		- Invert Mode Enable ( to have Hardwired to 1 )							 	*/
	{ CC112X_IOCFG2			,	0x73 },		//

	/*! PA_EN has to be set to 0. IOCFG0 is configured as followed :
		- Analog Transfer Disable
		- Invert Mode disable
		- GPIO0_CFG hardwired to 0							 	*/
	{ CC112X_IOCFG0			,	0x33 },

	/*! The Downlink SigFox Frame is composed of several bytes of synchro bits : 0x55 ( b01010101 )
		13 bits of SYNCHRO Frame ( The one used in DOWNLINK is b1001000100111 )
		b010101011001000100111 = 0xAB27 -> SYNC WORD = 0xAAAAB227					*/
	{ CC112X_SYNC3			,	0xAA },
	{ CC112X_SYNC2			,	0xAA },
	{ CC112X_SYNC1			,	0xB2 },
	{ CC112X_SYNC0			,	0x27 },

	/*! Sync Word Detection Configuration Reg. 1
		- PQT_GATING_EN = 1 : Do not wait for a preamble to be detected before checking the sync word
		- SYNC_THR = 8 : A sync word is accepted when the calculated sync word qualifier value (PQT_SYNC_ERR.SYNC_ERROR) is less than SYNC_THR/2). */
	{ CC112X_SYNC_CFG1		,	0x48 },

	/*! Sync Word Detection Configuration Reg. 0
		- SYNC_MODE = b010 : 16 bits
		- SYNC_NUM_ERROR = b11 : Bit error qualifier disabled. No check on bit errors */
	{ CC112X_SYNC_CFG0		,	0x0B },

	/*! Deviation = 800 Hz : DEV_E = 0 , DEV_M = 0xA7 								*/
	{ CC112X_DEVIATION_M	, 	0xA7 },

	/*! Modulation format = 2-GFSK and Frequency Deviation 						 	*/
	{ CC112X_MODCFG_DEV_E	,	0x08 },

	/*! DCFILT_BW_SETTLE = 011 : 256 samples 										*/
	{ CC112X_DCFILT_CFG		,	0x1C },

	/*! PREAMBLE_CFG1 - Preamble Length Configuration Reg. 1
		- NUM_PREAMBLE = b0000 : no preamble
		- PREAMBLE_WORD= b01 : 01010101 (0x55) 										*/
	{ CC112X_PREAMBLE_CFG1	,	0x01 },

	/*! PREAMBLE_CFG0 - Preamble Length Configuration Reg. 0
		- PQT_EN = 0 : preamble detection enabled
		- PQT_VALID_TIMEOUT = 0 : 16 Symbols received before PQT_VALID is asserted
		- PQT = d10 : A preamble is detected when the calculated preamble qualifier value (PQT_SYNC_ERR.PQT_ERROR) is less than PQT. */
	{ CC112X_PREAMBLE_CFG0	,	0x2A },

	/*! RX Mixer Frequency Configuration
	 	FREQ_IF = 62.25 kHz => f_IF = 51 = 0x33										*/
	{ CC112X_FREQ_IF_CFG	,	0x33 },

	/*! Channel Filter Configuration
		- CHFILT_BYPASS = 0 : Channel filter enabled
		- ADC_CIC_DECFACT = 1 : Decimation factor = 32
		- BB_CIC_DECFACT = b100001 : RX Filter BW Range [kHz] between 3.6 - 156.3 (kHz)
		=> Channel Bandwidth = 4700 Hz 											 	*/
	{ CC112X_CHAN_BW		,	0x6C },

	/*! General Modem Parameter Configuration Reg. 1 - Same as default values
		- CARRIER_SENSE_GATE : 0 : Search for sync word regardless of CS
		- FIFO_EN = 1 : Data in/out through the FIFOs
		- MANCHESTER_EN = 0 : NRZ
		- INVERT_DATA_EN = 0 : Invert Data Disabled
		- COLLISION_DETECT_EN = 0 : Collision detect disabled
		- DVGA_GAIN = b11 : 9dB DVGA
		- SINGLE_ADC_EN = 0 : IQ-channels										 	*/
	{ CC112X_MDMCFG1		,	0x46 },

	/*! General Modem Parameter Configuration Reg. 0
		- TRANSPARENT_MODE_EN = 0 : Transparent mode disabled
		- TRANSPARENT_INTFACT = b00 : 1x transparent signal interpolated one time before output (reset)
		- DATA_FILTER_EN = 0 : Transparent data filter disabled and extended data filter disabled
		- VITERBI_EN = 1 : Viterbi detection enabled							 	*/
	{ CC112X_MDMCFG0		,	0x05 },

	/*! Symbol rate = 600 symbols per second.
		SRATE_E = 2 ; SRATE_M = 1013008 = 0xF7510 => Bit rate = 600 bits/s		 	*/
	{ CC112X_SYMBOL_RATE2	, 	0x2F },
	{ CC112X_SYMBOL_RATE1	,	0x75 },
	{ CC112X_SYMBOL_RATE0	,	0x10 },

	/*! FIFO_CFG - FIFO Configuration
		- CRC_AUTOFLUSH = 0
		- FIFO_THR = E : FIFO RX threshold = 15 bytes
		15 Bytes is the length of the SigFox Frame ( Without Sync Bit and Sync Frame)
		= ECC + Data + HMAC + CRC 												 	*/
	{ CC112X_FIFO_CFG		,	0x0E },

	/*! Frequency Synthesizer Configuration
		- FS_LOCK_EN = 1 : Out of lock detector enabled
		- FSD_BANDSELECT = b010 : 820.0-960.6 MHz band (LO_Divider = 4)			 	*/
	{ CC112X_FS_CFG			,	0x12 },

	/*! Packet Configuration Reg. 2
		- CCA_MODE = b001 : Indicates clear channel when RSSI is below threshold
		- PKT_FORMAT = b00 : NORMAL MODE / FIFO									 	*/
	{ CC112X_PKT_CFG2		,	0x04 },

	/*! Packet Configuration Reg. 1
		- WHITE_DATA = 0 : whitening disabled
		- ADDR_CHECK_CFG = b00 : no address check
		- CRC_CFG = b00 : CRC disabled for TX and RX
		- BYTE_SWAP_EN = 0 : Data byte swap disabled
		- APPEND_STATUS = 1 : Status byte appended								 	*/
	{ CC112X_PKT_CFG1		,	0x01 },

	/*! Packet Configuration Reg. 0
		- LENGTH_CONFIG = b00 : Fixed packet length mode. specified in PKT_LEN
		- PKT_BIT_LEN = b000 : Number of bits to receive after PKT_LEN number of bytes are received.
		- UART_MODE_EN = 0 : UART mode disabled
		- UART_SWAP_EN = 0 : Swap disabled. Start/stop bit values are '1'/'0'	 	*/
	{ CC112X_PKT_CFG0		,	0x00 },

	/*! RFEND Configuration Reg. 1
		- RXOFF_MODE : Determines the state the radio will enter after receiving a good packet : IDLE
		- RX_TIME : The RX timeout is disabled when RX_TIME = 111 b
		- RX_TIME_QUAL : Continue RX mode on RX timeout if sync word has been
		  found or if PQT is reached or CS is asserted. 						 	*/
	{ CC112X_RFEND_CFG1		,	0x0F },

	/*! RFEND Configuration Reg. 0
		- CAL_END_WAKE_UP_EN : 0 : Disable additional wake-up pulse
		- TXOFF_MODE : Determines the state the radio will enter after transmitting a packet : IDLE
		- TERM_ON_BAD_PACKET_EN : 0 : Terminate on bad packet disabled
		- ANT_DIV_RX_TERM_CFG : Antenna diversity and termination based on CS/PQT are disabled */
	{ CC112X_RFEND_CFG0		,	0x00 },

	/*! Packet Length Configuration : 15 bytes for each packet 			 			*/
	{ CC112X_PKT_LEN		,	0x0F },

	/*! Frequency Offset Correction Configuration
		- FOC_EN = 1 : Frequency offset correction enabled
		- FOC_CFG = b00 : FOC after channel filter
		- FOC_LIMIT = 0 : RX filter BW/4
		- FOC_KI_FACTOR = b10 : FOC during packet reception with loop gain factor = 1/64 */
	{ CC112X_FREQOFF_CFG	,	0x22 },

	/*! Frequency Synthesizer settings for High Perf. RX mode	 					*/
	{ CC112X_FS_DIG0		,	0x5F },
	{ CC112X_FS_SPARE		,	0xAC },

	/*! Serial Status
		- IOC_SYNC_PINS_EN = 1 : Added to be able to read the GPIO_STATUS register 	*/
	{ CC112X_SERIAL_STATUS	,	0x08 },
};

static const registerSetting_t CarrierSenseConfig[] = {
    /* Register address     ,   Value    */

    /*! Set the CC1190 in RX mode. LNA_EN has to be set to 1.
    IOCFG2 is configured as followed :
    - Analog Transfer Disable
    - Invert Mode Enable ( to have Hardwired to 1 )                             */
    { CC112X_IOCFG2         ,   0x73 },     //  0x06

    /*! PA_EN has to be set to 0. IOCFG0 is configured as followed :
        - Analog Transfer Disable
        - Invert Mode disable
        - GPIO0_CFG hardwired to 0                              */
    { CC112X_IOCFG0         ,   0x33 },     // Disable the PA

    { CC112X_IOCFG3,              0x0F},  /* IOCFG3 is configured as followed :
                                           *    - Analog Transfer Disabled
                                           *    - Invert Mode Disabled
                                           *    - GPIO2_CFG set to signal CCA_STATUS (0xF) - Current CCA Status */

    { CC112X_PKT_CFG2,            0x04},  /* Same as the default value :
                                           * - PKT_FORMAT :0: NORMAL MODE / FIFO
                                           * - CCA_MODE :001 : Indicates clear channel when RSSI is below threshold */


    /*{ CC112X_AGC_CS_THR,          0xB0},*/  /* AGC_CS_THRESHOLD : 0xB0 = 176 ( Two's complement number ) => which means -80dBm
	                                             0xAC = 172 ( Two's complement number ) => which means -84dBm
	                                             0xBA = 186 ( Two's complement number ) => which means -70dBm*/

/*	{ CC112X_AGC_CS_THR,          0xBA}, */   /* 0x88 => -120 dBm ; 0x92 => -110 dBm ; */


    { CC112X_AGC_GAIN_ADJUST,     GAIN_ADJUST_VALUE},  /* AGC Adjustement offset : (0xA1) ( Two's complement number ) => which means -95 dBm */





    { CC112X_MDMCFG0,             0x05},  /* General Modem Parameter Configuration Reg. 0
                                           * - TRANSPARENT_MODE_EN : Transparent mode disabled
                                           * - TRANSPARENT_INTFACT : 1x transparent signal interpolated one time before output (reset)
                                           * - DATA_FILTER_EN : Transparent data filter disabled and extended data filter disabled
                                           * - VITERBI_EN : Viterbi detection enabled */

    { CC112X_MDMCFG1,             0x46},  /* General Modem Parameter Configuration Reg. 1 - Same as default values
                                           * - CARRIER_SENSE_GATE : 0 : Search for sync word regardless of CS
                                           * - FIFO_EN : 1 : Data in/out through the FIFOs
                                           * - MANCHESTER_EN : 0 ( NRZ )
                                           * - INVERT_DATA_EN : 0 Invert Data Disabled
                                           * - COLLISION_DETECT_EN : 0 : Collision detect disabled
                                           * - DVGA_GAIN : b11 - 9dB DVGA - The DVGA configuration has impact on the RSSI offset
                                           * - SINGLE_ADC_EN : 0 : IQ-channels */

    { CC112X_MODCFG_DEV_E,        0x08},  /* Modulation format = 2-GFSK and Frequency Deviation */
    { CC112X_DEVIATION_M,         0xA7},  /* 800 Hz - DEV_E = 0 , DEV_M = 0xA7 */

    { CC112X_SYMBOL_RATE2,        0x2F},  /* Symbol rate = 600 symbols per second as we are in 2GFSK */
    { CC112X_SYMBOL_RATE1,        0x75},  /* SRATE_E = 2 ; SRATE_M = 1013008 */
    { CC112X_SYMBOL_RATE0,        0x10},  /* Bit rate = 600 bits/s */

    { CC112X_SYNC3,               0xAA},  /* The Downlink SigFox Frame is composed of                                     */
    { CC112X_SYNC2,               0xAA},  /* - several bytes of synchro bits : 0x55 ( b01010101 )                         */
    { CC112X_SYNC1,               0xB2},  /* - 13 bits of SYNCHRO Frame ( The one used in DOWNLINK is b1001000100111 )    */
    { CC112X_SYNC0,               0x27},

    { CC112X_SYNC_CFG1,           0x48},  /* SYNC_CFG1 - Sync Word Detection Configuration Reg. 1
                                           * - PQT_GATING_EN = 1 : Do not wait for a preamble to be detected before
                                           *                       checking the sync word
                                           *
                                           * - SYNC_THR = 8 : A sync word is accepted when the calculated sync
                                           *                  word qualifier value (PQT_SYNC_ERR.SYNC_ERROR) is less than SYNC_THR/2). */

    { CC112X_SYNC_CFG0,           0x0B},  /* SYNC_CFG0 - Sync Word Detection Configuration Reg. 0
                                           * - SYNC_MODE = b010 : 16 bits
                                           * - SYNC_NUM_ERROR : b 11 : Bit error qualifier disabled. No check on bit errors */

    { CC112X_PREAMBLE_CFG1,       0x01},  /* PREAMBLE_CFG1 - Preamble Length Configuration Reg. 1
                                           * - NUM_PREAMBLE : b0000 : no preamble
                                           * - NUM_PREAMBLE : b0001 : 1 byte of preamble
                                           * - PREAMBLE_WORD : 00 : 10101010 (0xAA) */

    { CC112X_PREAMBLE_CFG0,       0x2A},  /* PREAMBLE_CFG0 - Preamble Length Configuration Reg. 0
                                           *
                                           * - PQT_EN : preamble detection enabled
                                           * - PQT_VALID_TIMEOUT : 0 : 16 Symbols received before PQT_VALID is asserted
                                           * - PQT : d10 : A preamble is detected when the calculated preamble
                                           *         qualifier value (PQT_SYNC_ERR.PQT_ERROR) is less than PQT.*/

    { CC112X_FIFO_CFG,            0x0E}, /* FIFO_CFG - FIFO Configuration
                                          * - CRC_AUTOFLUSH = 0
                                          * - FIFO_THR = E : FIFO RX threshold = 15 bytes
                                          *
                                          *  15 Bytes is the length of the SigFox Frame ( Without Sync Bit and Sync Frame )
                                          *  = ECC + Data + HMAC + CRC */

    { CC112X_PKT_CFG0,            0x00},  /* PKT_CFG0 - Packet Configuration Reg. 0
                                           * LENGTH_CONFIG = Fixed packet length mode. specified in PKT_LEN */

    { CC112X_PKT_LEN,             0x0F},  /*  PKT_LEN - Packet Length Configuration : 15 bytes for each packet */

    { CC112X_PKT_CFG1,            0x01},  /* Packet Configuration Reg. 1
                                           * - WHITE_DATA : whitening disabled
                                           * - ADDR_CHECK_CFG : no address check
                                           * - CRC_CFG : CRC disabled for TX and RX
                                           * - BYTE_SWAP_EN : Data byte swap disabled
                                           * - APPEND_STATUS : Status byte ( RSSI, LQI and CRC appended to the received packet */

    { CC112X_RFEND_CFG1,          0x0F},  /* RFEND Configuration Reg. 1
                                           * - RXOFF_MODE : Determines the state the radio will enter after receiving a good packet : IDLE
                                           * - RX_TIME : The RX timeout is disabled when RX_TIME = 111 b
                                           * - RX_TIME_QUAL : Continue RX mode on RX timeout if sync word has been found,
                                           *                  or if PQT is reached or CS is asserted  */

    { CC112X_RFEND_CFG0,          0x00},  /* RFEND Configuration Reg. 0
                                           * - CAL_END_WAKE_UP_EN : 0 : Disable additional wake-up pulse
                                           * - TXOFF_MODE : Determines the state the radio will enter after transmitting a packet : IDLE
                                           * - TERM_ON_BAD_PACKET_EN : 0 : Terminate on bad packet disabled
                                           * - ANT_DIV_RX_TERM_CFG : Antenna diversity and termination based on CS/PQT are disabled */

    { CC112X_CHAN_BW,             0x41},  /* Channel Filter Configuration
                                           * - CHFILT_BYPASS = 0 : Channel filter enabled
                                           * - ADC_CIC_DECFACT = 0 : Decimation factor = 20
                                           * - BB_CIC_DECFACT = b000001 : RX Filter Bandwidth = 250kHz */

    { CC112X_FREQOFF_CFG,         0x22},
    { CC112X_FS_CFG,              0x12},

    { CC112X_FS_DIG0,             0x5F},
    { CC112X_FS_SPARE,            0xAC},  /* IMPORTANT PARAMETER : without this register, the receive does not work */

    { CC112X_SERIAL_STATUS,       0x08},  /* Serial Status
                                           * IOC_SYNC_PINS_EN : 1  : Added to be able to read the GPIO_STATUS register */

    { CC112X_SETTLING_CFG,        0x03},  /* Frequency Synthesizer Calibration and Settling Configuration
                                           * - FS_AUTOCAL = 0 : to ensure we do not go into auto calibration as this is done
                                           * manually. */
    {CC112X_FREQ_IF_CFG,          0x00},


    {CC112X_DCFILT_CFG,           0x1C},  /* - DCFILT_BW_SETTLE = 011 : 256 samples */

};

#endif	// RADIO_H


/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
