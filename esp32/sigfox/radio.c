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


/******************************************************************************
* INCLUDES
*/
#include "stdbool.h"
#include "sigfox_api.h"
#include "modulation_table.h"
#include "targets/trx_rf_int.h"
#include "targets/cc112x_spi.h"
#include "radio.h"
#include "targets/hal_spi_rf_trxeb.h"
#include "sigfox_api.h"
#include "timer.h"

#include "lora/system/spi.h"
#include "modsigfox.h"

#include "esp_heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_intr.h"
#include "gpio.h"

extern sfx_u8 uplink_spectrum_access;
uint8  packetSemaphore;
sfx_u16 offset_value;

/******************************************************************************
* MACROS
*/
// Fast write on SPI for modulation
#define trx8BitWrite(addrByte, Data)\
{ \
\
  GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 17); \
  SpiOut((uint32_t)sigfox_spi.Spi, (Data << 8) | RADIO_BURST_ACCESS | RADIO_WRITE_ACCESS | addrByte);\
  GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 17); \
}

// Fast write to extended registers on SPI for modulation
#define trx16BitWrite(extAddr, regAddr, Data)\
{ \
\
  GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << 17); \
  SpiOut((uint32_t)sigfox_spi.Spi, RADIO_BURST_ACCESS|RADIO_WRITE_ACCESS|extAddr);\
  SpiOut((uint32_t)sigfox_spi.Spi, regAddr);\
  SpiOut((uint32_t)sigfox_spi.Spi, Data);\
  GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 17); \
}


/******************************************************************************
* DEFINES
*/

#define VCDAC_START_OFFSET  2
#define FS_VCO2_INDEX       0
#define FS_VCO4_INDEX       1
#define FS_CHP_INDEX        2

#define FOFF1	             0x02  // 0x0258 = 600 Value computed to be around
#define FOFF0	             0x58  // the middle of the register value

#define FREQ_STEP_LOW_FOFF1  0x00  // 0x00C8 = 200
#define FREQ_STEP_LOW_FOFF0  0xC8  // delta_f = 600 - 200 = 400 rf step

#define FREQ_STEP_HIGH_FOFF1 0x03  // 0x03E8 = 1000
#define FREQ_STEP_HIGH_FOFF0 0xE8  // delta_f = 1000 - 600 = 400 rf step

#define FOFF1_ETSI			 0x00  // 0x007F = 127
#define FOFF0_ETSI			 0x7F  // the middle of the register value

#define FREQ_OFFSET_BASE_FCC    0x0258  /* 0x0258 = 600 Value computed to be around the middle of the register value  */
#define FREQ_OFFSET_BASE_ETSI   0x0080  /* 0x0080 = 128 Value computed to be around the middle of the register value  */

#define SIGFOX_FREQ_STEP        400     // 400 rf step



/* Delay constants. (Depend on MCU SMCLK value and SPI speed) */
#define MODULATION_DELAY_CYCLES_100bps          305     // Calibrate the shape of 100 bps spectrum
#define PHASE_ACCUMULATION_DELAY_CYCLES         225     // Calibrate time to accumulate the 180 degree phase change. Depends on FOFFx values declared above.

#define FREQ_BIG_STEP   152.587890625    /* float ( 10000000)/float(2**16) = 152.587890625  */
#define FREQ_FINE_STEP   38.14697265625  /* float ( 10000000)/float(2**18) = 38.14697265625 */


/******************************************************************************
 * LOCAL VARIABLES
 */
static bool b_Diff;


/******************************************************************************
 * FUNCTION PROTOTYPE
 */

/******************************************************************************
* FUNCTIONS
*/

/*!****************************************************************************
 * \fn void RADIO_init_chip(sfx_rf_mode_t rf_mode)
 * \brief Function which initializes the RF Chip
 *
 * \param[in] param rf_mode  the mode ( RX or TX ) of RF programmation
 ******************************************************************************/
void RADIO_init_chip(sfx_rf_mode_t rf_mode)
{
	uint8 writeByte;
	uint16 i;

	// Reset radio
	trxSpiCmdStrobe(CC112X_SRES);

	// Set the radio in IDLE mode
	trxSpiCmdStrobe(CC112X_SIDLE);

	// Program the proper registers depending on the RF mode ( RX / TX )
	if (  rf_mode == SFX_RF_MODE_TX)
	{
        // printf("tx mode init\n");
        // Write registers of the radio chip for TX mode
		for( i = 0; i < (sizeof(HighPerfModeTx)/sizeof(registerSetting_t)); i++)
		{
			writeByte = HighPerfModeTx[i].data;
			cc112xSpiWriteReg(HighPerfModeTx[i].addr, &writeByte, 1);
		}
	}
	else if (  rf_mode == SFX_RF_MODE_RX )
	{
        // Write registers of the radio chip for RX mode
		for( i = 0; i < (sizeof(HighPerfModeRx)/sizeof(registerSetting_t)); i++)
		{
			writeByte = HighPerfModeRx[i].data;
			cc112xSpiWriteReg(HighPerfModeRx[i].addr, &writeByte, 1);
		}
	}
    else if( rf_mode == SFX_RF_MODE_CS_RX )
    {
        for( i = 0; i < (sizeof(CarrierSenseConfig)/sizeof(registerSetting_t)); i++ )
        {
            writeByte = CarrierSenseConfig[i].data;
            cc112xSpiWriteReg(CarrierSenseConfig[i].addr, &writeByte, 1);
        }
        // printf("carrier sense mode init\n");
    }
}


/**************************************************************************//**
 *  @brief 		This function puts the radio in Idle mode
 ******************************************************************************/
void RADIO_close_chip(void)
{
    trxSpiCmdStrobe(CC112X_SIDLE);
}

static void RADIO_manual_calibration(void)
{
    uint8 original_fs_cal2;
    uint8 calResults_for_vcdac_start_high[3];
    uint8 calResults_for_vcdac_start_mid[3];
    uint8 marcstate;
    uint8 writeByte;

    // 1) Set VCO cap-array to 0 (FS_VCO2 = 0x00)
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);

    // 2) Start with high VCDAC (original VCDAC_START + 2):
    cc112xSpiReadReg(CC112X_FS_CAL2, &original_fs_cal2, 1);
    writeByte = original_fs_cal2 + VCDAC_START_OFFSET;
    cc112xSpiWriteReg(CC112X_FS_CAL2, &writeByte, 1);

    // 3) Calibrate and wait for calibration to be done
    //   (radio back in IDLE state)
    trxSpiCmdStrobe(CC112X_SCAL);

    do {
        cc112xSpiReadReg(CC112X_MARCSTATE, &marcstate, 1);
    } while(marcstate != 0x41);

    // 4) Read FS_VCO2, FS_VCO4 and FS_CHP register obtained with
    //    high VCDAC_START value
    cc112xSpiReadReg(CC112X_FS_VCO2, &calResults_for_vcdac_start_high[FS_VCO2_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_VCO4, &calResults_for_vcdac_start_high[FS_VCO4_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_CHP, &calResults_for_vcdac_start_high[FS_CHP_INDEX], 1);

    // 5) Set VCO cap-array to 0 (FS_VCO2 = 0x00)
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);

    // 6) Continue with mid VCDAC (original VCDAC_START):
    writeByte = original_fs_cal2;
    cc112xSpiWriteReg(CC112X_FS_CAL2, &writeByte, 1);

    // 7) Calibrate and wait for calibration to be done
    //   (radio back in IDLE state)
    trxSpiCmdStrobe(CC112X_SCAL);

    do {
        cc112xSpiReadReg(CC112X_MARCSTATE, &marcstate, 1);
    } while(marcstate != 0x41);

    // 8) Read FS_VCO2, FS_VCO4 and FS_CHP register obtained
    //    with mid VCDAC_START value
    cc112xSpiReadReg(CC112X_FS_VCO2,
                     &calResults_for_vcdac_start_mid[FS_VCO2_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_VCO4,
                     &calResults_for_vcdac_start_mid[FS_VCO4_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_CHP,
                     &calResults_for_vcdac_start_mid[FS_CHP_INDEX], 1);

    // 9) Write back highest FS_VCO2 and corresponding FS_VCO
    //    and FS_CHP result
    if(calResults_for_vcdac_start_high[FS_VCO2_INDEX] >
        calResults_for_vcdac_start_mid[FS_VCO2_INDEX]) {
        writeByte = calResults_for_vcdac_start_high[FS_VCO2_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_high[FS_VCO4_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO4, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_high[FS_CHP_INDEX];
        cc112xSpiWriteReg(CC112X_FS_CHP, &writeByte, 1);
    } else {
        writeByte = calResults_for_vcdac_start_mid[FS_VCO2_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_mid[FS_VCO4_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO4, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_mid[FS_CHP_INDEX];
        cc112xSpiWriteReg(CC112X_FS_CHP, &writeByte, 1);
    }
}

/**************************************************************************//**
 *  @brief 		This function allows to change the central frequency used by the chip
 *
 *  @param 		ul_Freq is the new frequency to use
 *
 *  @note		RF frequency Programming : From the CC1125 documentation,
 * 				frequencies have to be programmed following the relations
 *
 *	@note 	 	Freq_rf = Freq_vco / LO_Divider [Hz]
 *  @note		Freq_vco = FREQ * Fxosc/2^16 + FREOFF * Fxosc/2^18  [Hz]
 *
 *  @note		With LO_Divider = 4 and Fxosc = 40 MHz or 32 MHz, we can simplify the equation to:
 *
 *  @note		\li FREQ = 0.0065536 * Freq_rf - 0.25 FREQOFF for 40 MHz XTAL
 *  @note		\li FREQ = 0.008192 * Freq_rf - 0.25 FREQOFF for 32 MHz XTAL
 ******************************************************************************/
void RADIO_change_frequency(unsigned long ul_Freq)
{
    uint8 tuc_Frequence[3];
    signed long CalibFrequency = FREQ_ADJUST_VALUE;
    unsigned long freq_rf;
    unsigned long freq_reg_value;
    uint16 offset_reg_value;
    sfx_u8 writeByte_FOFF0;
    sfx_u8 writeByte_FOFF1;
    sfx_u16 base_offset;

    /* adding a calibration offset if it's necessary */
    freq_rf = ul_Freq + CalibFrequency;

    /* Ensure the FREQOFF register is set properly */
    if( uplink_spectrum_access == SFX_FH )
    {
        base_offset = FREQ_OFFSET_BASE_FCC;
    }
    else /* ETSI */
    {
        base_offset = FREQ_OFFSET_BASE_ETSI;
    }

    /* adding Frequency register offset value to compute new frequency value */
    freq_reg_value   = (sfx_u32)(freq_rf / FREQ_BIG_STEP) - (sfx_u32)(base_offset/4);
    offset_reg_value = (freq_rf - freq_reg_value * FREQ_BIG_STEP) / FREQ_FINE_STEP;

    offset_value    = offset_reg_value;
    writeByte_FOFF1 = offset_reg_value >> 8;
    writeByte_FOFF0 = offset_reg_value & 0x00FF;

    /* save the value into table */
    tuc_Frequence[0]= (sfx_u8) (freq_reg_value & 0x000000FF);
    tuc_Frequence[1]= (sfx_u8) ((freq_reg_value & 0x0000FF00)>> 8u);
    tuc_Frequence[2]= (sfx_u8) ((freq_reg_value & 0x00FF0000)>> 16u);

    /* send the FREQOFF register settings */
    cc112xSpiWriteReg(CC112X_FREQOFF1, &writeByte_FOFF1,1);
    cc112xSpiWriteReg(CC112X_FREQOFF0, &writeByte_FOFF0,1);

    /* send frequency registers value to the chip */
    cc112xSpiWriteReg(CC112X_FREQ2, tuc_Frequence + 2, 1);
    cc112xSpiWriteReg(CC112X_FREQ1, tuc_Frequence + 1, 1);
    cc112xSpiWriteReg(CC112X_FREQ0, tuc_Frequence, 1);

    RADIO_manual_calibration();
}


/**************************************************************************//**
 *  @brief 		This function produces the modulation (PA + Freq).
 *         		It is called only when a '0' bit is encountered in the frame.
 *
 *  @note		BPSK modulation: we need to accumulate phase ( 180 degrees )
 *  			during a defined time with the following relation :
 *
 * 	@note 	  	2*pi*delta_frequency = ( delta_phase / delta_time )
 *
 * 	@note		\li delta_phase for a BPSK modulation has to be pi ( 180 degrees )
 * 	@note		\li delta_frequency will be set to 15240 Hz ( 400 * 38.1 Hz ( RF Step for Fxosc = 40MHz ) )
 *  	 										( 500 * 30.5 Hz ( RF Step for Fxosc = 32MHz) )
 * 	@note		\li delta_time will then be compute to 32.8 us which is 656 ticks - for MSP430 clock at 20 Mhz
 *
 *  @note       To ensure there is no frequency drift, we alternate
 *  			the delta frequency: One time we increase the frequency,
 *  			the other we decrease it.
 *  @note       During the modulation time, the PA is OFF
 ******************************************************************************/
IRAM_ATTR void RADIO_modulate(void)
{
    sfx_s16 count;
    sfx_s8 sign;
    uint8 writeByte_FOFF0_pi_shifting;
    uint8 writeByte_FOFF1_pi_shifting;
    sfx_u8 writeByte_FOFF1_init = offset_value >>8;
    sfx_u8 writeByte_FOFF0_init = offset_value & 0x00FF;

	if (b_Diff == false)
	{
		b_Diff = true;
		// Frequency step down
		sign = -1;
	}
	else
	{
		b_Diff = false;
		// Frequency step high
		sign = +1;
	}

	writeByte_FOFF1_pi_shifting = (offset_value + sign * SIGFOX_FREQ_STEP) >> 8;
	writeByte_FOFF0_pi_shifting = (offset_value + sign * SIGFOX_FREQ_STEP) & 0x00FF;

	// decrease the PA
    if(uplink_spectrum_access == SFX_FH)
    {
        for (count = (NB_PTS_PA-1); count >= 0; count--)
        {
            // Write the PA ramp levels to PA_CFG2 register
            trx8BitWrite(CC112X_PA_CFG2, Table_Pa_600bps[NB_PTS_PA-count-1]);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps / 45);
        }
    } else {
        for (count = (NB_PTS_PA-1); count >= 0; count--)
        {
            // Write the PA ramp levels to PA_CFG2 register
            trx8BitWrite(CC112X_PA_CFG2, Table_Pa_600bps[NB_PTS_PA-count-1]);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps);
        }
    }

    // Program the frequency offset
    cc112xSpiWriteReg(CC112X_FREQOFF1, &writeByte_FOFF1_pi_shifting, 1);
    cc112xSpiWriteReg(CC112X_FREQOFF0, &writeByte_FOFF0_pi_shifting, 1);

    /* IMPORTANT NOTE
        *
        * As explained above in the Modulation section,
        * a  __delay_cycles(PHASE_ACCUMULATION_DELAY_CYCLES)
        * instruction should take place here
        * to ensure a phase accumation of pi.
        *
        * This value of delay cycles must account for the SPI write delays
        * and was obtained experimentally. This value can be used to calibrate
        * the modulation for best possible quality ( SNR ) of the BPSK
        * signal. The quality of BPSK modulation might change for different
        * compiler optimization settings and different MCU clock frequencies.
        * Thus, requiring new calibration for this delay value.
        */
    __delay_cycles(PHASE_ACCUMULATION_DELAY_CYCLES);

    cc112xSpiWriteReg(CC112X_FREQOFF1, &writeByte_FOFF1_init, 1);
    cc112xSpiWriteReg(CC112X_FREQOFF0, &writeByte_FOFF0_init, 1);

    // increase the PA
    if (uplink_spectrum_access == SFX_FH) {
        for (count = NB_PTS_PA-1; count >= (0); count--)
        {
            trx8BitWrite(CC112X_PA_CFG2, Table_Pa_600bps[count]);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps / 45);
        }
    } else {
        for (count = NB_PTS_PA-1; count >= (0); count--)
        {
            trx8BitWrite(CC112X_PA_CFG2, Table_Pa_600bps[count]);
            // Wait after changing PA level to reduce spurrs
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps);
        }
    }
}


/**************************************************************************//**
 *  @brief this function starts the oscillator, and generates the ramp-up
 ******************************************************************************/
void IRAM_ATTR
RADIO_start_rf_carrier(void)
{
    int16 count_start;
    uint8 writeByte;

    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_PA_CFG2, &writeByte, 1);
    trxSpiCmdStrobe(CC112X_STX);
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_PA_CFG2, &writeByte, 1);

    // Ramp up the PA
    if (uplink_spectrum_access == SFX_FH) {
        for (count_start = NB_PTS_PA-1; count_start >= (0); count_start--)
        {
            cc112xSpiWriteReg(CC112X_PA_CFG2, (uint8*) &Table_Pa_600bps[count_start], 1);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps / 4);
        }
    } else {
        for (count_start = NB_PTS_PA-1; count_start >= (0); count_start--)
        {
            cc112xSpiWriteReg(CC112X_PA_CFG2, (uint8*) &Table_Pa_600bps[count_start], 1);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps * 3);
        }
    }
}


/**************************************************************************//**
 *  @brief This function stops the radio and produces the ramp down
 ******************************************************************************/
void IRAM_ATTR
RADIO_stop_rf_carrier(void)
{
    uint16 count_stop;
    uint8 writeByte;

    // Ramp down the PA
    if (uplink_spectrum_access == SFX_FH) {
        for (count_stop = 0; count_stop < (NB_PTS_PA); count_stop++)
        {
            cc112xSpiWriteReg(CC112X_PA_CFG2, (uint8*) &Table_Pa_600bps[count_stop], 1);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps / 4);
        }
    } else {
        for (count_stop = 0; count_stop < (NB_PTS_PA); count_stop++)
        {
            cc112xSpiWriteReg(CC112X_PA_CFG2, (uint8*) &Table_Pa_600bps[count_stop], 1);
            __delay_cycles(MODULATION_DELAY_CYCLES_100bps * 3);
        }
    }

    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_PA_CFG2, &writeByte, 1);
    trxSpiCmdStrobe(CC112X_SIDLE);
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_PA_CFG2, &writeByte, 1);
}


/**************************************************************************//**
 *  @brief 		This function configures the cc112x for continuous wave (CW)
 *  			transmission at the given frequency and starts the TX.
 *
 *  @param 		ul_Freq 		is the frequency to use
 ******************************************************************************/
void
RADIO_start_unmodulated_cw(unsigned long ul_Freq)
{
    // Initialize the radio in TX mode
    RADIO_init_chip(SFX_RF_MODE_TX);

    /* Update the frequency */
    RADIO_change_frequency(ul_Freq);

    // Start TX carrier wave
    RADIO_start_rf_carrier();
}


/**************************************************************************//**
 *  @brief 		This function turns off the CW transmission at the given frequency
 *
 *  @param 		ul_Freq 		is the frequency to use
 ******************************************************************************/
void
RADIO_stop_unmodulated_cw(unsigned long ul_Freq)
{
    // Stop TX carrier wave
    RADIO_stop_rf_carrier();

    // Reinitialize the radio in TX mode
    RADIO_init_chip(SFX_RF_MODE_TX );

    /* Update the frequency */
    RADIO_change_frequency(ul_Freq);
}

/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
