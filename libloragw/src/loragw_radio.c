/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Functions used to handle LoRa concentrator radios.

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Michael Coracin
*/

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */

#include "loragw_sx125x.h"
#include "loragw_sx1272_fsk.h"
#include "loragw_sx1272_lora.h"
#include "loragw_spi.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_hal.h"
#include "loragw_radio.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_REG == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_REG_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)                if(a==NULL){return LGW_REG_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES -------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define PLL_LOCK_MAX_ATTEMPTS 5

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

extern void *lgw_spi_target; /*! generic pointer to the SPI device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

void sx125x_write(uint8_t channel, uint8_t addr, uint8_t data);

uint8_t sx125x_read(uint8_t channel, uint8_t addr);

int setup_sx1272_FSK(uint32_t frequency);

int setup_sx1272_LoRa(uint32_t frequency);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

void sx125x_write(uint8_t channel, uint8_t addr, uint8_t data) {
    int reg_add, reg_dat, reg_cs;

    /* checking input parameters */
    if (channel >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN\n");
        return;
    }
    if (addr >= 0x7F) {
        DEBUG_MSG("ERROR: ADDRESS OUT OF RANGE\n");
        return;
    }

    /* selecting the target radio */
    switch (channel) {
        case 0:
            reg_add = LGW_SPI_RADIO_A__ADDR;
            reg_dat = LGW_SPI_RADIO_A__DATA;
            reg_cs  = LGW_SPI_RADIO_A__CS;
            break;

        case 1:
            reg_add = LGW_SPI_RADIO_B__ADDR;
            reg_dat = LGW_SPI_RADIO_B__DATA;
            reg_cs  = LGW_SPI_RADIO_B__CS;
            break;

        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", channel);
            return;
    }

    /* SPI master data write procedure */
    lgw_reg_w(reg_cs, 0);
    lgw_reg_w(reg_add, 0x80 | addr); /* MSB at 1 for write operation */
    lgw_reg_w(reg_dat, data);
    lgw_reg_w(reg_cs, 1);
    lgw_reg_w(reg_cs, 0);

    return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint8_t sx125x_read(uint8_t channel, uint8_t addr) {
    int reg_add, reg_dat, reg_cs, reg_rb;
    int32_t read_value;

    /* checking input parameters */
    if (channel >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN\n");
        return 0;
    }
    if (addr >= 0x7F) {
        DEBUG_MSG("ERROR: ADDRESS OUT OF RANGE\n");
        return 0;
    }

    /* selecting the target radio */
    switch (channel) {
        case 0:
            reg_add = LGW_SPI_RADIO_A__ADDR;
            reg_dat = LGW_SPI_RADIO_A__DATA;
            reg_cs  = LGW_SPI_RADIO_A__CS;
            reg_rb  = LGW_SPI_RADIO_A__DATA_READBACK;
            break;

        case 1:
            reg_add = LGW_SPI_RADIO_B__ADDR;
            reg_dat = LGW_SPI_RADIO_B__DATA;
            reg_cs  = LGW_SPI_RADIO_B__CS;
            reg_rb  = LGW_SPI_RADIO_B__DATA_READBACK;
            break;

        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", channel);
            return 0;
    }

    /* SPI master data read procedure */
    lgw_reg_w(reg_cs, 0);
    lgw_reg_w(reg_add, addr); /* MSB at 0 for read operation */
    lgw_reg_w(reg_dat, 0);
    lgw_reg_w(reg_cs, 1);
    lgw_reg_w(reg_cs, 0);
    lgw_reg_r(reg_rb, &read_value);

    return (uint8_t)read_value;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int setup_sx1272_FSK(uint32_t frequency) {
    uint64_t freq_reg;
    uint8_t ModulationShaping = 1;
    uint8_t PllHop = 1;
    uint8_t LnaGain = 1;
    uint8_t AdcBwAuto = 0;
    uint8_t AdcBw = 7;
    uint8_t AdcLowPwr = 0;
    uint8_t AdcTrim = 6;
    uint8_t AdcTest = 0;
    uint8_t RxBwExp = 2;
    uint8_t RxBwMant = 1;
    uint8_t reg_val;
    int x;

    /* Set in FSK mode */
    x = lgw_sx1272_reg_w(SX1272_REG_OPMODE, 0);
    wait_ms(100);
    x |= lgw_sx1272_reg_w(SX1272_REG_OPMODE, RF_OPMODE_SLEEP | (ModulationShaping << 3));
    wait_ms(100);
    x |= lgw_sx1272_reg_w(SX1272_REG_OPMODE, RF_OPMODE_STANDBY | (ModulationShaping << 3));
    wait_ms(100);

    /* Set RF carrier frequency */
    x |= lgw_sx1272_reg_w(SX1272_REG_PLLHOP, PllHop << 7);
    freq_reg = ((uint64_t)frequency << 19) / (uint64_t)32000000;
    x |= lgw_sx1272_reg_w(SX1272_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
    x |= lgw_sx1272_reg_w(SX1272_REG_FRFMID, (freq_reg >> 8) & 0xFF);
    x |= lgw_sx1272_reg_w(SX1272_REG_FRFLSB, (freq_reg >> 0) & 0xFF);

    /* Config */
    x |= lgw_sx1272_reg_w(SX1272_REG_LNA, RF_LNA_BOOST_ON | (LnaGain << 5));
    x |= lgw_sx1272_reg_w(0x68, AdcBw | (AdcBwAuto << 3));
    x |= lgw_sx1272_reg_w(0x69, AdcTest | (AdcTrim << 4) | (AdcLowPwr << 7));

    /* set BR and FDEV for 200 kHz bandwidth*/
    x |= lgw_sx1272_reg_w(SX1272_REG_BITRATEMSB, RF_BITRATEMSB_1000_BPS);
    x |= lgw_sx1272_reg_w(SX1272_REG_BITRATELSB, RF_BITRATELSB_1000_BPS);
    x |= lgw_sx1272_reg_w(SX1272_REG_FDEVMSB, RF_FDEVMSB_45000_HZ);
    x |= lgw_sx1272_reg_w(SX1272_REG_FDEVLSB, RF_FDEVLSB_45000_HZ);

    /* Config continues... */
    x |= lgw_sx1272_reg_w(SX1272_REG_RXCONFIG, RF_RXCONFIG_AGCAUTO_OFF);
    x |= lgw_sx1272_reg_w(SX1272_REG_RSSICONFIG, RF_RSSICONFIG_SMOOTHING_64 | RF_RSSICONFIG_OFFSET_P_03_DB);
    x |= lgw_sx1272_reg_w(SX1272_REG_RXBW, RxBwExp | (RxBwMant << 3)); /* RX BW = 100kHz, Mant=20, Exp=2 */
    x |= lgw_sx1272_reg_w(SX1272_REG_RXDELAY, 2);
    x |= lgw_sx1272_reg_w(SX1272_REG_PLL, RF_PLL_BANDWIDTH_75);
    x |= lgw_sx1272_reg_w(0x47, 1); /* optimize PLL start-up time */

    if (x != LGW_REG_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to configure SX1272\n");
        return x;
    }

    /* set Rx continuous mode */
    x = lgw_sx1272_reg_w(SX1272_REG_OPMODE, RF_OPMODE_RECEIVER | (ModulationShaping << 3));
    wait_ms(500);
    x |= lgw_sx1272_reg_r(SX1272_REG_IRQFLAGS1, &reg_val);
    /* Check if RxReady and ModeReady */
    if ((TAKE_N_BITS_FROM(reg_val, 6, 1) == 0) || (TAKE_N_BITS_FROM(reg_val, 7, 1) == 0) || (x != LGW_REG_SUCCESS)) {
        DEBUG_MSG("ERROR: SX1272 failed to enter RX continuous mode\n");
        return LGW_REG_ERROR;
    }
    wait_ms(500);

    DEBUG_MSG("INFO: Successfully configured SX1272 for FSK modulation\n");

    return LGW_REG_SUCCESS;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int setup_sx1272_LoRa(uint32_t frequency) {
    uint64_t freq_reg;
    uint8_t LoRaMode = 1;
    uint8_t bw = 0;
    uint8_t LowZin = 1;
    uint8_t sf = 7;
    uint8_t AgcAuto = 1;
    uint8_t LnaGain = 1;
    uint8_t TrimRxCrFo = 0;
    uint8_t LnaBoost = 3;
    uint8_t AdcBwAuto = 0;
    uint8_t AdcBw = 7;
    uint8_t AdcLowPwr = 0;
    uint8_t AdcTrim = 6;
    uint8_t AdcTest = 0;
    uint8_t reg_val;
    int x;

    /* Set in LoRa mode */
    x = lgw_sx1272_reg_w(SX1272_REG_LR_OPMODE, 0);
    wait_ms(100);
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_OPMODE, RFLR_OPMODE_SLEEP | (LoRaMode << 7));
    wait_ms(100);
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_OPMODE, RFLR_OPMODE_STANDBY | (LoRaMode << 7));
    wait_ms(100);

    /* Set RF carrier frequency */
    freq_reg = ((uint64_t)frequency << 19) / (uint64_t)32000000;
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_FRFMSB, (freq_reg >> 16) & 0xFF);
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_FRFMID, (freq_reg >>  8) & 0xFF);
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_FRFLSB,  freq_reg        & 0xFF);

    /* Config */
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_MODEMCONFIG1, bw << 6);
    x |= lgw_sx1272_reg_w(0x50, LowZin);
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_MODEMCONFIG2, (sf << 4) | (AgcAuto << 2));
    x |= lgw_sx1272_reg_w(SX1272_REG_LR_LNA, LnaBoost | (TrimRxCrFo << 3) | (LnaGain << 5));
    x |= lgw_sx1272_reg_w(0x68, AdcBw | (AdcBwAuto << 3));
    x |= lgw_sx1272_reg_w(0x69, AdcTest | (AdcTrim << 4) | (AdcLowPwr << 7));

    if (x != LGW_REG_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to configure SX1272\n");
        return x;
    }

    /* Set in Rx continuous mode */
    x = lgw_sx1272_reg_w(SX1272_REG_LR_OPMODE, RFLR_OPMODE_RECEIVER | (LoRaMode << 7));
    wait_ms(100);
    x |= lgw_sx1272_reg_r(SX1272_REG_LR_OPMODE, &reg_val);
    if ((reg_val != (RFLR_OPMODE_RECEIVER | (LoRaMode << 7))) || (x != LGW_REG_SUCCESS)) {
        DEBUG_MSG("ERROR: SX1272 failed to enter RX continuous mode\n");
        return x;
    }

    DEBUG_MSG("INFO: Successfully configured SX1272 for LoRa modulation\n");

    return LGW_REG_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int setup_sx125x(uint8_t rf_chain, uint8_t rf_clkout, bool rf_enable, uint8_t rf_radio_type, uint32_t freq_hz) {
    uint32_t part_int = 0;
    uint32_t part_frac = 0;
    int cpt_attempts = 0;

    if (rf_chain >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN\n");
        return -1;
    }

    /* Get version to identify SX1255/57 silicon revision */
    DEBUG_PRINTF("Note: SX125x #%d version register returned 0x%02x\n", rf_chain, sx125x_read(rf_chain, 0x07));

    /* General radio setup */
    if (rf_clkout == rf_chain) {
        sx125x_write(rf_chain, 0x10, SX125x_TX_DAC_CLK_SEL + 2);
        DEBUG_PRINTF("Note: SX125x #%d clock output enabled\n", rf_chain);
    } else {
        sx125x_write(rf_chain, 0x10, SX125x_TX_DAC_CLK_SEL);
        DEBUG_PRINTF("Note: SX125x #%d clock output disabled\n", rf_chain);
    }

    switch (rf_radio_type) {
        case LGW_RADIO_TYPE_SX1255:
            sx125x_write(rf_chain, 0x28, SX125x_XOSC_GM_STARTUP + SX125x_XOSC_DISABLE*16);
            break;
        case LGW_RADIO_TYPE_SX1257:
            sx125x_write(rf_chain, 0x26, SX125x_XOSC_GM_STARTUP + SX125x_XOSC_DISABLE*16);
            break;
        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d FOR RADIO TYPE\n", rf_radio_type);
            break;
    }

    if (rf_enable == true) {
        /* Tx gain and trim */
        sx125x_write(rf_chain, 0x08, SX125x_TX_MIX_GAIN + SX125x_TX_DAC_GAIN*16);
        sx125x_write(rf_chain, 0x0A, SX125x_TX_ANA_BW + SX125x_TX_PLL_BW*32);
        sx125x_write(rf_chain, 0x0B, SX125x_TX_DAC_BW);

        /* Rx gain and trim */
        sx125x_write(rf_chain, 0x0C, SX125x_LNA_ZIN + SX125x_RX_BB_GAIN*2 + SX125x_RX_LNA_GAIN*32);
        sx125x_write(rf_chain, 0x0D, SX125x_RX_BB_BW + SX125x_RX_ADC_TRIM*4 + SX125x_RX_ADC_BW*32);
        sx125x_write(rf_chain, 0x0E, SX125x_ADC_TEMP + SX125x_RX_PLL_BW*2);

        /* set RX PLL frequency */
        switch (rf_radio_type) {
            case LGW_RADIO_TYPE_SX1255:
                part_int = freq_hz / (SX125x_32MHz_FRAC << 7); /* integer part, gives the MSB */
                part_frac = ((freq_hz % (SX125x_32MHz_FRAC << 7)) << 9) / SX125x_32MHz_FRAC; /* fractional part, gives middle part and LSB */
                break;
            case LGW_RADIO_TYPE_SX1257:
                part_int = freq_hz / (SX125x_32MHz_FRAC << 8); /* integer part, gives the MSB */
                part_frac = ((freq_hz % (SX125x_32MHz_FRAC << 8)) << 8) / SX125x_32MHz_FRAC; /* fractional part, gives middle part and LSB */
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d FOR RADIO TYPE\n", rf_radio_type);
                break;
        }

        sx125x_write(rf_chain, 0x01,0xFF & part_int); /* Most Significant Byte */
        sx125x_write(rf_chain, 0x02,0xFF & (part_frac >> 8)); /* middle byte */
        sx125x_write(rf_chain, 0x03,0xFF & part_frac); /* Least Significant Byte */

        /* start and PLL lock */
        do {
            if (cpt_attempts >= PLL_LOCK_MAX_ATTEMPTS) {
                DEBUG_MSG("ERROR: FAIL TO LOCK PLL\n");
                return -1;
            }
            sx125x_write(rf_chain, 0x00, 1); /* enable Xtal oscillator */
            sx125x_write(rf_chain, 0x00, 3); /* Enable RX (PLL+FE) */
            ++cpt_attempts;
            DEBUG_PRINTF("Note: SX125x #%d PLL start (attempt %d)\n", rf_chain, cpt_attempts);
            wait_ms(1);
        } while((sx125x_read(rf_chain, 0x11) & 0x02) == 0);
    } else {
        DEBUG_PRINTF("Note: SX125x #%d kept in standby mode\n", rf_chain);
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_sx1272_reg_w(uint8_t address, uint8_t reg_value) {
    return lgw_spi_w(lgw_spi_target, LGW_SPI_MUX_MODE1, LGW_SPI_MUX_TARGET_SX1272, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_sx1272_reg_r(uint8_t address, uint8_t *reg_value) {
    return lgw_spi_r(lgw_spi_target, LGW_SPI_MUX_MODE1, LGW_SPI_MUX_TARGET_SX1272, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_setup_sx1272(uint32_t frequency, uint8_t modulation) {
    int x;
    uint8_t reg_val;

    /* Check parameters */
    if ((modulation != MOD_FSK) && (modulation != MOD_LORA)) {
        DEBUG_PRINTF("ERROR: modulation not supported for SX1272 (%u)\n", modulation);
        return LGW_REG_ERROR;
    }

    /* Test SX1272 version register */
    x = lgw_sx1272_reg_r(0x42, &reg_val);
    if (x != LGW_SPI_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to read sx1272 version register\n");
        return x;
    }
    if (reg_val != 0x22) {
        DEBUG_PRINTF("ERROR: Unexpected SX1272 register version (%u)\n", reg_val);
        return LGW_REG_ERROR;
    }

    DEBUG_PRINTF("SX1272 version : %u\n", reg_val);

    switch (modulation) {
        case MOD_LORA:
            x = setup_sx1272_LoRa(frequency);
            break;
        case MOD_FSK:
            x = setup_sx1272_FSK(frequency);
            break;
        default:
            /* Should not happen */
            break;
    }
    if (x != LGW_REG_SUCCESS) {
        DEBUG_MSG("ERROR: failed to setup SX1272\n");
        return x;
    }

    return LGW_REG_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
