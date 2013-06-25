/*
 / _____)             _              | |    
( (____  _____ ____ _| |_ _____  ____| |__  
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
    Lora gateway Hardware Abstraction Layer
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdlib.h>     /* malloc & free */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */

#include "loragw_reg.h"
#include "loragw_hal.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#ifdef DEBUG
    #define DEBUG_MSG(str)              fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)  fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)               if(a==NULL){fprintf(stderr,"%s:%d: ERROR, NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_HAL_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)               if(a==NULL){return LGW_HAL_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define     MCU_ARB     0
#define     MCU_AGC     1

const uint8_t if_list[LGW_IF_CHAIN_NB] = LGW_IF_CONFIG; /* define hardware capability */

#define     MCU_ARB_FW_BYTE     2048 /* size of the firmware IN BYTES (= twice the number of 14b words) */
#define     MCU_AGC_FW_BYTE     8192 /* size of the firmware IN BYTES (= twice the number of 14b words) */

#define     SX1257_CLK_OUT          1   
#define     SX1257_TX_DAC_CLK_SEL   1   /* 0:int, 1:ext */
#define     SX1257_TX_DAC_GAIN      2   /* 3:0, 2:-3, 1:-6, 0:-9 dBFS (default 2) */
#define     SX1257_TX_MIX_GAIN      14  /* -38 + 2*TxMixGain dB (default 14) */
#define     SX1257_TX_PLL_BW        3   /* 0:75, 1:150, 2:225, 3:300 kHz (default 3) */
#define     SX1257_TX_ANA_BW        0   /* 17.5 / 2*(41-TxAnaBw) MHz (default 0) */
#define     SX1257_TX_DAC_BW        7   /* 24 + 8*TxDacBw Nb FIR taps (default 2) */
#define     SX1257_RX_LNA_GAIN      1   /* 1 to 6, 1 highest gain */
#define     SX1257_RX_BB_GAIN       12  /* 0 to 15 , 15 highest gain */
#define     SX1257_RX_ADC_BW        7   /* 0 to 7, 2:100<BW<200, 5:200<BW<400,7:400<BW (kHz) */
#define     SX1257_RX_ADC_TRIM      7   /* 0 to 7, 6 for 32MHz ref, 5 for 36MHz ref */
#define     SX1257_RXBB_BW          2

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

#include "arb_fw.var" /* external definition of the variable */
#include "agc_fw.var" /* external definition of the variable */

/*
The following static variables are the configuration set that the user can
modify using rxrf_setconf and rxif_setconf functions.
The function _start then use that set to configure the hardware.

Parameters validity and coherency is verified by the _setconf functions and
the _start function assumes 
*/

static bool rf_enable[LGW_RF_CHAIN_NB] = {0, 0};
static uint32_t rf_freq[LGW_IF_CHAIN_NB] = {0, 0};

static uint8_t if_rf_switch = 0x00; /* each IF from 0 to 7 has 1 bit associated to it, 0 -> radio A, 1 -> radio B */
/* IF 8 and 9 are on radio A */

static bool if_enable[LGW_IF_CHAIN_NB] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int16_t if_freq[LGW_IF_CHAIN_NB] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static uint8_t lora_rx_bw = 0; /* for the Lora standalone modem(s) */
static uint8_t lora_rx_sf = 0; /* for the Lora standalone modem(s) */
static uint8_t fsk_rx_bw = 0; /* for the FSK standalone modem(s) */
static uint8_t fsk_rx_sf = 0; /* for the FSK standalone modem(s) */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

int count_ones_16(uint16_t a);

int load_firmware(uint8_t target, uint8_t *firmware, uint16_t size);

void sx125x_write(uint8_t rf_chain, uint8_t addr, uint8_t data);

uint8_t sx125x_read(uint8_t rf_chain, uint8_t addr);

void setup_sx1257(uint8_t rf_chain, uint32_t freq_hz);

void lgw_constant_adjust(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

int count_ones_16(uint16_t a) {
    uint16_t i;
    int count = 0;
    for (i=1; i != 0; i <<= 1) {
        if ((a & i) != 0)
            ++ count;
    }
    return count;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* size id the firmware size in bytes (not 14b words) */
int load_firmware(uint8_t target, uint8_t *firmware, uint16_t size) {
    int i;
    int32_t read_value;
    int reg_rst;
    int reg_sel;
    
    /* check parameters */
    CHECK_NULL(firmware);
    if (target == MCU_ARB) {
        if (size != MCU_ARB_FW_BYTE) {
            DEBUG_MSG("ERROR: NOT A VALID SIZE FOR MCU ARG FIRMWARE");
            return -1;
        }
        reg_rst = LGW_MCU_RST_0;
        reg_sel = LGW_MCU_SELECT_MUX_0;
    }else if (target == MCU_AGC) {
        if (size != MCU_AGC_FW_BYTE) {
            DEBUG_MSG("ERROR: NOT A VALID SIZE FOR MCU AGC FIRMWARE");
            return -1;
        }
        reg_rst = LGW_MCU_RST_1;
        reg_sel = LGW_MCU_SELECT_MUX_1;
    } else {
        DEBUG_MSG("ERROR: NOT A VALID TARGET FOR LOADING FIRMWARE");
        return -1;
    }
    
    /* reset the targeted MCU */
    lgw_reg_w(reg_rst, 1);
    
    /* set mux to access MCU program RAM and set address to 0 */
    lgw_reg_w(reg_sel, 1);
    lgw_reg_w(LGW_MCU_PROM_ADDR, 0);
    
    /* write the program in one burst */
    lgw_reg_wb(LGW_MCU_PROM_DATA, firmware, size);
    
    /* give back control of the MCU program ram to the MCU */
    lgw_reg_w(reg_sel, 0);
    
    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

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
            DEBUG_MSG("ERROR: DEFAULT CASE THAT SHOULD BE IMPOSSIBLE\n");
            return;
    }
    
    /* SPI master data write procedure */
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
        DEBUG_MSG("ERROR: DEFAULT CASE THAT SHOULD BE IMPOSSIBLE\n");
        return 0;
    }
    
    /* SPI master data read procedure */
    lgw_reg_w(reg_add, addr); /* MSB at 0 for read operation */
    lgw_reg_w(reg_dat, 0);
    lgw_reg_w(reg_cs, 1);
    lgw_reg_w(reg_cs, 0);
    lgw_reg_r(reg_rb, &read_value);
    
    return (uint8_t)read_value;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setup_sx1257(uint8_t rf_chain, uint32_t freq_hz) {
    uint32_t part_int;
    uint32_t part_frac;
    int i = 0;
    
    if (rf_chain >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN\n");
        return;
    }
    
    /* misc */
    sx125x_write(rf_chain, 0x10, SX1257_TX_DAC_CLK_SEL + SX1257_CLK_OUT*2);
    
    /* Tx gain and trim */
    sx125x_write(rf_chain, 0x08, SX1257_TX_MIX_GAIN + SX1257_TX_DAC_GAIN*16);
    sx125x_write(rf_chain, 0x0A, SX1257_TX_ANA_BW + SX1257_TX_PLL_BW*32);
    sx125x_write(rf_chain, 0x0B, SX1257_TX_DAC_BW);
    
    /* Rx gain and trim */
    sx125x_write(rf_chain, 0x0C, 0 + SX1257_RX_BB_GAIN*2 + SX1257_RX_LNA_GAIN*32);
    sx125x_write(rf_chain, 0x0D, SX1257_RXBB_BW + SX1257_RX_ADC_TRIM*4 + SX1257_RX_ADC_BW*32);
    
    /* set RX PLL frequency */
    part_int = freq_hz / LGW_SW1257_DENUM; /* integer part, gives the MSB and the middle byte */
    part_frac = ((freq_hz % LGW_SW1257_DENUM) << 8) / LGW_SW1257_DENUM; /* fractional part, gives LSB */
    sx125x_write(rf_chain, 0x01,0xFF & (part_int >> 8)); /* Most Significant Byte */
    sx125x_write(rf_chain, 0x02,0xFF & part_int); /* middle byte */
    sx125x_write(rf_chain, 0x03,0xFF & part_frac); /* Least Significant Byte */
    
    /* start and PLL lock */
    do {
        sx125x_write(rf_chain, 0x00, 1); /* enable Xtal oscillator */
        sx125x_write(rf_chain, 0x00, 3); /* Enable RX (PLL+FE) */
        ++i;
        DEBUG_PRINTF("SX1257 #%d PLL start (attempt %d)\n", rf_chain, i);
        wait_ms(1);
    } while(sx125x_read(rf_chain, 0x11) & 0x02 == 0);
    
    return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void lgw_constant_adjust(void) {
    
    /* I/Q path setup */
    // lgw_reg_w(LGW_RX_INVERT_IQ,0); /* default 0 */
    // lgw_reg_w(LGW_MODEM_INVERT_IQ,1); /* default 1 */
    // lgw_reg_w(LGW_CHIRP_INVERT_RX,1); /* default 1 */
    // lgw_reg_w(LGW_RX_EDGE_SELECT,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_MODEM_INVERT_IQ,0); /* default 0 */
    lgw_reg_w(LGW_DC_NOTCH_EN,1); /* default 0 */
    // lgw_reg_w(LGW_RSSI_BB_FILTER_ALPHA,7); /* default 7 */
    lgw_reg_w(LGW_RSSI_DEC_FILTER_ALPHA,7); /* default 5 */
    lgw_reg_w(LGW_RSSI_CHANN_FILTER_ALPHA,7); /* default 8 */
    // lgw_reg_w(LGW_RSSI_BB_DEFAULT_VALUE,32); /* default 32 */
    lgw_reg_w(LGW_RSSI_CHANN_DEFAULT_VALUE,90); /* default 100 */
    lgw_reg_w(LGW_RSSI_DEC_DEFAULT_VALUE,90); /* default 100 */
    
    /* Correlator setup */
    // lgw_reg_w(LGW_CORR_DETECT_EN,126); /* default 126 */
    // lgw_reg_w(LGW_CORR_NUM_SAME_PEAK,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_MAC_GAIN,5); /* default 5 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF6,0); /* default 0 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF7,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF8,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF9,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF10,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF11,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SAME_PEAKS_OPTION_SF12,1); /* default 1 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF6,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF7,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF8,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF9,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF10,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF11,4); /* default 4 */
    // lgw_reg_w(LGW_CORR_SIG_NOISE_RATIO_SF12,4); /* default 4 */
    
    /* Lora 'multi' modems setup */
    lgw_reg_w(LGW_CONCENTRATOR_MODEM_ENABLE,1); /* default 0 */
    lgw_reg_w(LGW_PREAMBLE_SYMB1_NB,4); /* default 10 */
    // lgw_reg_w(LGW_FREQ_TO_TIME_DRIFT,9); /* default 9 */
    // lgw_reg_w(LGW_FREQ_TO_TIME_INVERT,29); /* default 29 */
    // lgw_reg_w(LGW_FRAME_SYNCH_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_SYNCH_DETECT_TH,1); /* default 1 */
    // lgw_reg_w(LGW_ZERO_PAD,0); /* default 0 */
    lgw_reg_w(LGW_SNR_AVG_CST,3); /* default 2 */
    // lgw_reg_w(LGW_PPM_OFFSET,0); /* default 0 */
    // lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS,1); /* default 1 */
    // lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS,2); /* default 2 */
    // lgw_reg_w(LGW_PREAMBLE_FINE_TIMING_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_ONLY_CRC_EN,1); /* default 1 */
    // lgw_reg_w(LGW_PAYLOAD_FINE_TIMING_GAIN,2); /* default 2 */
    // lgw_reg_w(LGW_TRACKING_INTEGRAL,0); /* default 0 */
    // lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_RDX8,0); /* default 0 */
    // lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_RDX4,0); /* default 0 */
    // lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_SF12_RDX4,4092); /* default 4092 */
    // lgw_reg_w(LGW_MAX_PAYLOAD_LEN,255); /* default 255 */
    
    /* MBWSSF Modem */
    lgw_reg_w(LGW_MBWSSF_MODEM_ENABLE,1); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_PREAMBLE_SYMB1_NB,10); /* default 10 */
    // lgw_reg_w(LGW_MBWSSF_FREQ_TO_TIME_DRIFT,36); /* default 36 */
    // lgw_reg_w(LGW_MBWSSF_FREQ_TO_TIME_INVERT,29); /* default 29 */
    // lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_SYNCH_DETECT_TH,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_ZERO_PAD,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_PPM_OFFSET,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS,2); /* default 2 */
    // lgw_reg_w(LGW_MBWSSF_ONLY_CRC_EN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_PAYLOAD_FINE_TIMING_GAIN,2); /* default 2 */
    // lgw_reg_w(LGW_MBWSSF_PREAMBLE_FINE_TIMING_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_TRACKING_INTEGRAL,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_AGC_FREEZE_ON_DETECT,1); /* default 1 */
    
    /* TX */
    // lgw_reg_w(LGW_TX_MODE,0); /* default 0 */
    lgw_reg_w(LGW_TX_START_DELAY,5000); /* default 0 */
    
    return;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_rxrf_setconf(uint8_t rf_chain, struct lgw_conf_rxrf_s conf) {
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_rxif_setconf(uint8_t if_chain, struct lgw_conf_rxif_s conf) {
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_start(void) {
    int reg_stat;
    int32_t read_value;
    
    reg_stat = lgw_connect();
    if (reg_stat == LGW_REG_ERROR) {
        DEBUG_MSG("ERROR: FAIL TO CONNECT BOARD");
        return LGW_HAL_ERROR;
    }
    
    /* reset the registers (also shuts the radios down) */
    lgw_soft_reset();
    
    /* switch on and reset the radios (also starts the 32 MHz XTAL) */
    lgw_reg_w(LGW_RADIO_A_EN,1); /* radio A *must* be started to get 32 MHz clk */
    if (rf_enable[1] == 1) {
        lgw_reg_w(LGW_RADIO_B_EN,1);
    }
    wait_ms(10);
    lgw_reg_w(LGW_RADIO_RST,1);
    wait_ms(10);
    lgw_reg_w(LGW_RADIO_RST,0);
    
    /* setup the radios */
    if (rf_enable[0] == 1) {
        setup_sx1257(0, rf_freq[0]);
    }
    if (rf_enable[1] == 1) {
        setup_sx1257(1, rf_freq[1]);
    }
    
    /* gives the AGC MCU control over radio, RF front-end and filter gain */
    lgw_reg_w(LGW_FORCE_HOST_RADIO_CTRL,0);
    lgw_reg_w(LGW_FORCE_HOST_FE_CTRL,0);
    lgw_reg_w(LGW_FORCE_DEC_FILTER_GAIN,0);
    
    // /* TODO load the calibration firmware and wait for calibration to end */
    // load_firmware(MCU_AGC, cal_firmware, ARRAY_SIZE(cal_firmware));
    // lgw_reg_w(LGW_MCU_RST, 0); /* start the AGC MCU */
    // lgw_reg_w(LGW_FORCE_HOST_REG_CTRL,0); /* let the AGC MCU control the registers */
    // do {
        // lgw_reg_r(LGW_VERSION, &read_value);
    // } while (read_value == 0);
    // lgw_reg_w(LGW_MCU_RST, 3); /* reset all MCU */
    
    /* in the absence of calibration firmware, do a "manual" calibration */
    lgw_reg_w(LGW_TX_OFFSET_I,10);
    lgw_reg_w(LGW_TX_OFFSET_Q,5);
    lgw_reg_w(LGW_IQ_MISMATCH_A_AMP_COEFF,63);
    lgw_reg_w(LGW_IQ_MISMATCH_A_PHI_COEFF,9);
    lgw_reg_w(LGW_IQ_MISMATCH_B_AMP_COEFF,0);
    lgw_reg_w(LGW_IQ_MISMATCH_B_PHI_COEFF,0);
    
    /* load adjusted parameters */
    lgw_constant_adjust();
    // lgw_reg_w(LGW_PPM_OFFSET,0); /* default 0 */
    
    /* configure Lora 'multi' (aka. Lora 'sensor' channels */
    /* IF: 256 = 125 kHz */
    // lgw_reg_w(LGW_IF_FREQ_0,-384); /* default -384 */
    // lgw_reg_w(LGW_IF_FREQ_1,-128); /* default -128 */
    // lgw_reg_w(LGW_IF_FREQ_2, 128); /* default 128 */
    // lgw_reg_w(LGW_IF_FREQ_3, 384); /* default 384 */
    // lgw_reg_w(LGW_IF_FREQ_4,-384); /* default -384 */
    // lgw_reg_w(LGW_IF_FREQ_5,-128); /* default -128 */
    // lgw_reg_w(LGW_IF_FREQ_6, 128); /* default 128 */
    // lgw_reg_w(LGW_IF_FREQ_7, 384); /* default 384 */
    // lgw_reg_w(LGW_IF_FREQ_8,   0); /* MBWSSF modem (default 0) */
    // lgw_reg_w(LGW_IF_FREQ_9,   0); /* FSK modem  (default 0) */
    
    /* IF mapping to radio A/B (per bit, 0=A, 1=B) */
    //lgw_reg_w(LGW_RADIO_SELECT, 0xF0);

    /* Correlator mapping to IF */
    lgw_reg_w(LGW_CORR0_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR1_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR2_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR3_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR4_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR5_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR6_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    // lgw_reg_w(LGW_CORR7_DETECT_EN, 0x7E); /* all SF except 6, default 0 */
    
    /* Back-haul MBWSSF modem */
    // lgw_reg_w(LGW_MBWSSF_MODEM_BW,0); /* 0=125, 1=250, 2=500 kHz */
    // lgw_reg_w(LGW_MBWSSF_RATE_SF,7);
    // lgw_reg_w(LGW_MBWSSF_PPM_OFFSET,0); /* default 0 */
    
    /* Load firmware */
    load_firmware(MCU_ARB, arb_firmware, MCU_ARB_FW_BYTE);
    load_firmware(MCU_AGC, agc_firmware, MCU_AGC_FW_BYTE);
    
    /* Ungate clock (gated by default), needed for SPI master to SX1257 */
    lgw_reg_w(LGW_CLK32M_EN, 1);
    lgw_reg_w(LGW_CLKHS_EN, 1);
    
    /* Get MCUs out of reset */
    lgw_reg_w(LGW_MCU_RST_0, 0);
    lgw_reg_w(LGW_MCU_RST_1, 0);
    
    /* Show that nanoC is configured */
    lgw_reg_w(LGW_LED_REG, 5);
    
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_stop(void) {
    lgw_soft_reset();
    lgw_disconnect();
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data) {
    int nb_pkt_fetch; /* loop variable and return value */
    struct lgw_pkt_rx_s *current_pkt; /* pointer to the current structure in the struct array */
    uint8_t tmp_buff[300]; /* buffer to store the result of SPI read bursts */
    uint16_t data_addr; /* address read from the FIFO and programmed before the data buffer read operation */
    int s; /* size of the payload, uses to address metadata */
    int j;
    
    /* check input variables */
    if (max_pkt == 0) {
    } else if (max_pkt <= 0) {
        DEBUG_MSG("ERROR: INVALID MAX NUMBER OF PACKETS TO FETCH");
        return LGW_HAL_ERROR;
    }
    CHECK_NULL(pkt_data);
    
    /* iterate max_pkt times at most */
    for (nb_pkt_fetch = 0; nb_pkt_fetch <= max_pkt; ++nb_pkt_fetch) {
        
        /* point to the proper struct in the struct array */
        current_pkt = &pkt_data[nb_pkt_fetch];
        
        /* fetch all the RX FIFO data */
        lgw_reg_rb(LGW_RX_PACKET_DATA_FIFO_NUM_STORED, tmp_buff, 5);
        
        /* how many packets are in the RX buffer ? Break if zero */
        if (tmp_buff[0] = 0) {
            break; /* no more packets to fetch, exit out of FOR loop */
        }
        
        current_pkt->status = tmp_buff[3];
        current_pkt->size = tmp_buff[4];
        s = current_pkt->size;
        
        /* required or automated? */
        data_addr = (uint16_t)tmp_buff[1] + ((uint16_t)tmp_buff[2] << 8);
        lgw_reg_w(LGW_RX_DATA_BUF_ADDR, data_addr);
        
        /* dynamically allocate memory to store payload */
        current_pkt->payload = (uint8_t *)malloc(s);
        if (current_pkt->payload == NULL) {
            /* not enough memory to allocate for payload, abort with error */
            DEBUG_MSG("ERROR: IMPOSSIBLE TO ALLOCATE MEMORY TO FETCH PAYLOAD");
            return LGW_HAL_ERROR;
        }
        
        /* get payload + metedata */
        lgw_reg_rb(LGW_RX_DATA_BUF_DATA, tmp_buff, s+16); /* 16 is for metadata */
        
        /* copy payload */
        for (j = 0; j < s; ++j) {
            current_pkt->payload[j] = tmp_buff[j];
        }
        
        /* process metadata */
        // TODO: actually process them!
        current_pkt->if_chain = tmp_buff[s+0];
        current_pkt->modulation = 0; // TODO
        current_pkt->datarate = (tmp_buff[s+1] >> 4) & 0x0F;
        current_pkt->coderate = (tmp_buff[s+1] >> 1) & 0x07;
        
        current_pkt->snr = tmp_buff[s+2]; //TODO: need to rescale
        current_pkt->snr_min = tmp_buff[s+3]; //TODO: need to rescale
        current_pkt->snr_max = tmp_buff[s+4]; //TODO: need to rescale
        current_pkt->rssi = tmp_buff[s+5]; //TODO: need to rescale
        
        current_pkt->count_us = (uint32_t)tmp_buff[s+6] + ((uint32_t)tmp_buff[s+7] << 8) + ((uint32_t)tmp_buff[s+8] << 16) + ((uint32_t)tmp_buff[s+9] << 24);
        
        current_pkt->crc = (uint16_t)tmp_buff[s+10] + ((uint16_t)tmp_buff[s+11] << 8);
    }
    
    return nb_pkt_fetch;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_send(struct lgw_pkt_tx_s pkt_data) {
    return LGW_HAL_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
