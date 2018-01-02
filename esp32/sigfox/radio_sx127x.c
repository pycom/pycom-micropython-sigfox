/*!****************************************************************************
 * \file radio.c
 * \brief Manage the low level modulation and configuration of the TI chipset
 * \author SigFox Test and Validation team
 * \version 0.1
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


/**************************************************************************//**
 * @addtogroup Radio
 * @{
 ******************************************************************************/


/******************************************************************************
* INCLUDES
*/
#include "stdbool.h"
#include <stdint.h>
#include "sigfox_api.h"
#include "radio_sx127x.h"
#include "sigfox_api.h"
#include "timer.h"
#include "board.h"

#include "modsigfox.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_intr.h"
#include "gpio.h"
#include "spi.h"
#include "sx1272/sx1272.h"

extern sfx_u8 uplink_spectrum_access;
uint8  packetSemaphore;


extern Spi_t sigfox_spi;
extern sigfox_settings_t sigfox_settings;
extern sigfox_obj_t sigfox_obj;

/******** DEFINE **************************************************************/
typedef enum{
	E_PHASE_0 = 0,
	E_PHASE_180
} te_phaseState;

/******* LOCAL VARIABLES ******************************************************/
te_phaseState Phase_State = E_PHASE_0;

/******************************************************************************
 * LOCAL VARIABLES
 */

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
void RADIO_init_chip(sfx_rf_mode_t rf_mode) {
    if (rf_mode == SFX_RF_MODE_TX) {
        SX12728BitWrite( 0x01, 0x80 );      // Device in StandBy in LoRa mode
        SX12728BitWrite( 0x1D, 0x00 );      // LoRa BW = 0
        SX12728BitWrite( 0x09, 0xBF );      // PA Boost output

        SX12728BitWrite( 0x3D, 0xAF );      // sd_max_freq_deviation

        SX12728BitWrite( 0x58, 0x09 );      // Only valid if a TXCO is used

        SX12728BitWrite( 0x4B, 0x3E );      // PA controlled manually
        SX12728BitWrite( 0x4D, 0x03 );

        SX12728BitWrite( 0x0A, 0x0F );      // PaRamp on 10us
        SX12728BitWrite( 0x5E, 0xD0 );      // PLL bandwidth 300 KHz

        if (uplink_spectrum_access == SFX_FH) {
            SX12728BitWrite(REG_LR_PADAC, 0x87);    // Up tp +20dBm on the PA_BOOST pin
        } else {
            SX12728BitWrite(REG_LR_PADAC, 0x84);
        }
    } else if (rf_mode == SFX_RF_MODE_RX) {
        SX12728BitWrite( 0x01, 0x00 );      // Device in Sleep mode
        SX12728BitWrite( 0x3D, 0xA1 );
        SX12728BitWrite( 0x4B, 0x2E );
        SX12728BitWrite( 0x4D, 0x03 );
        SX12728BitWrite( 0x63, 0x00 );
        SX12728BitWrite( 0x3E, 0x00 );
        SX12728BitWrite( 0x4C, 0x00 );
        SX12728BitWrite( 0x1E, 0x74 );
        SX12728BitWrite( 0x0A, 0x09 );
        SX12728BitWrite( 0x1D, 0x08 );
        SX12728BitWrite( 0x0A, 0x19 );
        SX12728BitWrite( 0x5E, 0xD0 );
        SX12728BitWrite(REG_LR_PADAC, 0x84);

        /* Write registers of the radio chip for RX mode */
        for(int i = 0; i < (sizeof(HighPerfModeRx)/sizeof(registerSetting_t)); i++) {
            SX12728BitWrite(HighPerfModeRx[i].addr, HighPerfModeRx[i].data);
        }

        /* Set radio in RX */
        SX12728BitWrite( REG_OPMODE, 0x0D);
    }
}


/**************************************************************************//**
 *  @brief 		This function puts the radio in Idle mode
 ******************************************************************************/
void RADIO_close_chip(void) {
    SX12728BitWrite( 0x01, 0x00 );
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
void RADIO_change_frequency(unsigned long ul_Freq) {
    uint8_t tuc_Frequence[3];
    uint32_t CalibFrequency = 0;
    uint32_t freq_rf;
    uint32_t freq_reg_value;

    /* adding a calibration offset if it's necessary */
    freq_rf = ul_Freq + CalibFrequency;

    /* adding Frequency register offset value to compute new frequency value */
    freq_reg_value = (uint32_t)((freq_rf) / 61.035);

    /* save the value into table */
    tuc_Frequence[0] = (uint8_t)(freq_reg_value & 0x000000FF);
    tuc_Frequence[1] = (uint8_t)((freq_reg_value & 0x0000FF00) >> 8u);
    tuc_Frequence[2] = (uint8_t)((freq_reg_value & 0x00FF0000) >> 16u);

    /* send frequency registers value to the chip */
    SX12728BitWrite(REG_FRFMSB, tuc_Frequence[2]);
    SX12728BitWrite(REG_FRFMID, tuc_Frequence[1]);
    SX12728BitWrite(REG_FRFLSB, tuc_Frequence[0]);
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
IRAM_ATTR void RADIO_modulate(void) {
	int16_t count;
	uint8_t NewPhaseValue;

	if (Phase_State == E_PHASE_0) {
		Phase_State = E_PHASE_180;
		NewPhaseValue = 128;
	} else {
		Phase_State = E_PHASE_0;
		NewPhaseValue = 0;
	}

    if (uplink_spectrum_access == SFX_FH) {
        /* decrease PA */
        for (count = MAX_PA_VALUE_FCC; count > MIN_PA_VALUE; count--) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_FCC) {
                __delay_cycles(16);
            }  else {
                __delay_cycles(10);
            }
        }
    } else {
        /* decrease PA */
        for (count = MAX_PA_VALUE_ETSI; count > MIN_PA_VALUE; count--) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_ETSI) {
                __delay_cycles(725);
            } else {
                __delay_cycles(550);
            }
        }
    }

	/* Switch OFF PA */
	SX12728BitWrite(0x63, 0x00);
	/* Change signal phase */
	SX12728BitWrite(REG_IRQFLAGS1, NewPhaseValue);
	/* Switch ON PA */
	SX12728BitWrite(0x63, 0x60);

    if (uplink_spectrum_access == SFX_FH) {
        /* increase PA */
        for (count = MIN_PA_VALUE + 1; count < MAX_PA_VALUE_FCC ; count++) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_FCC) {
                __delay_cycles(16);
            } else {
                __delay_cycles(10);
            }
        }
        SX12728BitWrite(0x4C, MAX_PA_VALUE_FCC);
    } else {
        /* increase PA */
        for (count = MIN_PA_VALUE + 1; count < MAX_PA_VALUE_ETSI ; count++) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_ETSI) {
                __delay_cycles(725);
            } else {
                __delay_cycles(550);
            }
        }
        SX12728BitWrite(0x4C, MAX_PA_VALUE_ETSI);
    }
}


/**************************************************************************//**
 *  @brief this function starts the oscillator, and generates the ramp-up
 ******************************************************************************/
IRAM_ATTR void RADIO_start_rf_carrier(void) {
    /* Implement the ramp-up */
    uint8_t count;

    SX12728BitWrite( 0x63, 0x20 );      // Enable manual PA
    SX12728BitWrite( 0x3E, 0x00 );      // phase = 0
    SX12728BitWrite( 0x4C, 0x00 );      // Max value for the PA is 0xE7 DO NOT GOT OVER OR IT MAY DAMAGE THE CHIPSET
    SX12728BitWrite( 0x1e, 0x78 );      // Tx Continuous mode
    SX12728BitWrite( 0x01, 0x83 );      // Device in Tx mode

    if (uplink_spectrum_access == SFX_FH) {
        vTaskDelay(50);                 // Warm up the crystal and the PLL
        for (count = MIN_PA_VALUE; count < MAX_PA_VALUE_FCC; count++) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_FCC) {
                __delay_cycles(200);
            } else {
                __delay_cycles(140);
            }
        }
        SX12728BitWrite(0x4C, MAX_PA_VALUE_FCC);
    } else {
        vTaskDelay(550);                // Warm up the crystal and the PLL
        for (count = MIN_PA_VALUE; count < MAX_PA_VALUE_ETSI; count++) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_ETSI) {
                __delay_cycles(3000);
            } else {
                __delay_cycles(1600);
            }
        }
        SX12728BitWrite(0x4C, MAX_PA_VALUE_ETSI);
    }
}


/**************************************************************************//**
 *  @brief This function stops the radio and produces the ramp down
 ******************************************************************************/
IRAM_ATTR void RADIO_stop_rf_carrier(void) {
    /* Implement the ramp-down */
    uint8_t count;

    uint32_t ilevel = MICROPY_BEGIN_ATOMIC_SECTION();

    if (uplink_spectrum_access == SFX_FH) {
        for (count = MAX_PA_VALUE_FCC; count > MIN_PA_VALUE; count--) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_FCC) {
                __delay_cycles(200);
            } else {
                __delay_cycles(140);
            }
        }
    } else {
        for (count = MAX_PA_VALUE_ETSI; count > MIN_PA_VALUE; count--) {
            SX12728BitWrite(0x4C, count);
            if (count > STEP_HIGH_ETSI) {
                __delay_cycles(3000);
            } else {
                __delay_cycles(1600);
            }
        }
    }

    /* Set the MIN Value to the PA */
    SX12728BitWrite(0x4C, MIN_PA_VALUE);

    SX12728BitWrite( 0x63, 0x00 );      // switch off the PA
    SX12728BitWrite( 0x01, 0x80 );      // Device in Sleep mode
    SX12728BitWrite( 0x1e, 0x70 );      // Tx Continuous mode

    MICROPY_END_ATOMIC_SECTION(ilevel);
}


/**************************************************************************//**
 *  @brief 		This function configures the cc112x for continuous wave (CW)
 *  			transmission at the given frequency and starts the TX.
 *
 *  @param 		ul_Freq 		is the frequency to use
 ******************************************************************************/
void RADIO_start_unmodulated_cw(unsigned long ul_Freq) {
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
void RADIO_stop_unmodulated_cw(unsigned long ul_Freq) {
    // Stop TX carrier wave
    RADIO_stop_rf_carrier();

    // Reinitialize the radio in TX mode
    RADIO_init_chip(SFX_RF_MODE_TX);

    /* Update the frequency */
    RADIO_change_frequency(ul_Freq);
}

void RADIO_warm_up_crystal (unsigned long ul_Freq) {
    // Initialize the radio in TX mode
    RADIO_init_chip(SFX_RF_MODE_TX);

    /* Update the frequency */
    RADIO_change_frequency(ul_Freq);

    SX12728BitWrite( 0x63, 0x20 );      // Enable manual PA
    SX12728BitWrite( 0x3E, 0x00 );      // phase = 0
    SX12728BitWrite( 0x4C, 0x00 );      // Max value for the PA is 0xE7 DO NOT GOT OVER OR IT MAY DAMAGE THE CHIPSET
    SX12728BitWrite( 0x1e, 0x78 );      // Tx Continuous mode
    SX12728BitWrite( 0x01, 0x83 );      // Device in Tx mode

    vTaskDelay(500);                // Wait for the crystal and the PLL to warm up

    SX12728BitWrite( 0x63, 0x00 );      // switch off the PA
    SX12728BitWrite( 0x01, 0x80 );      // Device in Sleep mode
    SX12728BitWrite( 0x1e, 0x70 );      // Tx Continuous mode
}

/**************************************************************************//**
 * Close the Doxygen group.
 * @}
 ******************************************************************************/
