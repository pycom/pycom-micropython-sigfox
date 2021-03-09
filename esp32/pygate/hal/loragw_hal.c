/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
    LoRa concentrator Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <string.h>     /* memcpy */
#include <math.h>       /* pow, cell */
#include <inttypes.h>   /* format macro constants... */

#include "loragw_reg.h"
#include "loragw_mcu.h"
#include "loragw_hal.h"
#include "loragw_aux.h"
#include "loragw_radio.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */
#define DEBUG_HAL 1
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_HAL == 1
#define DEBUG_MSG(str)                fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define DEBUG_ARRAY(a,b,c)            for(a=0;a<b;++a) fprintf(stderr,"%x.",c[a]);fprintf(stderr,"end\n")
#define CHECK_NULL(a)                 if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_HAL_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define DEBUG_ARRAY(a,b,c)            for(a=0;a!=0;){}
#define CHECK_NULL(a)                 if(a==NULL){return LGW_HAL_ERROR;}
#endif

#define IF_HZ_TO_REG(f)     (f << 5)/15625
#define SET_PPM_ON(bw,dr)   (((bw == BW_125KHZ) && ((dr == DR_LORA_SF11) || (dr == DR_LORA_SF12))) || ((bw == BW_250KHZ) && (dr == DR_LORA_SF12)))
#define TRACE()             fprintf(stderr, "@ %s %d\n", __FUNCTION__, __LINE__);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

#define MCU_ARB             0
#define MCU_AGC             1
#define MCU_ARB_FW_BYTE     8192 /* size of the firmware IN BYTES (= twice the number of 14b words) */
#define MCU_AGC_FW_BYTE     8192 /* size of the firmware IN BYTES (= twice the number of 14b words) */
#define FW_VERSION_ADDR     0x20 /* Address of firmware version in data memory */
#define FW_VERSION_CAL      2 /* Expected version of calibration firmware */
#define FW_VERSION_AGC      4 /* Expected version of AGC firmware */
#define FW_VERSION_ARB      1 /* Expected version of arbiter firmware */

#define TX_METADATA_NB      16
#define RX_METADATA_NB      16

#define AGC_CMD_WAIT        16
#define AGC_CMD_ABORT       17

#define MIN_LORA_PREAMBLE   4
#define STD_LORA_PREAMBLE   6
#define MIN_FSK_PREAMBLE    3
#define STD_FSK_PREAMBLE    5

#define TX_START_DELAY      1500

#define RSSI_MULTI_BIAS     -35 /* difference between "multi" modem RSSI offset and "stand-alone" modem RSSI offset */
#define RSSI_FSK_POLY_0     60 /* polynomiam coefficients to linearize FSK RSSI */
#define RSSI_FSK_POLY_1     1.5351
#define RSSI_FSK_POLY_2     0.003

#define LGW_RF_RX_BANDWIDTH_125KHZ  925000      /* for 125KHz channels */
#define LGW_RF_RX_BANDWIDTH_250KHZ  1000000     /* for 250KHz channels */
#define LGW_RF_RX_BANDWIDTH_500KHZ  1100000     /* for 500KHz channels */

/* constant arrays defining hardware capability */
const uint8_t ifmod_config[LGW_IF_CHAIN_NB] = LGW_IFMODEM_CONFIG;

/* Version string, used to identify the library version/options once compiled */
const char lgw_version_string[] = "Version: \" 0.2.3 \";";

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

#include "arb_fw.var" /* external definition of the variable */
#include "agc_fw.var" /* external definition of the variable */
#include "cal_fw.var" /* external definition of the variable */
#include "cal_fw5-12.var" /* external definition of the variable */

/*
The following static variables are the configuration set that the user can
modify using rxrf_setconf, rxif_setconf and txgain_setconf functions.
The functions _start and _send then use that set to configure the hardware.

Parameters validity and coherency is verified by the _setconf functions and
the _start and _send functions assume they are valid.
*/

static bool lgw_is_started = false;

static bool rf_enable[LGW_RF_CHAIN_NB];
static uint32_t rf_rx_freq[LGW_RF_CHAIN_NB]; /* absolute, in Hz */
static float rf_rssi_offset[LGW_RF_CHAIN_NB];
static bool rf_tx_enable[LGW_RF_CHAIN_NB];
static enum lgw_radio_type_e rf_radio_type[LGW_RF_CHAIN_NB];

static bool if_enable[LGW_IF_CHAIN_NB];
static bool if_rf_chain[LGW_IF_CHAIN_NB]; /* for each IF, 0 -> radio A, 1 -> radio B */
static int32_t if_freq[LGW_IF_CHAIN_NB]; /* relative to radio frequency, +/- in Hz */

static uint8_t lora_multi_sfmask[LGW_MULTI_NB]; /* enables SF for LoRa 'multi' modems */

static uint8_t lora_rx_bw; /* bandwidth setting for LoRa standalone modem */
static uint8_t lora_rx_sf; /* spreading factor setting for LoRa standalone modem */
static bool lora_rx_ppm_offset;

static uint8_t fsk_rx_bw; /* bandwidth setting of FSK modem */
static uint32_t fsk_rx_dr; /* FSK modem datarate in bauds */
static uint8_t fsk_sync_word_size = 3; /* default number of bytes for FSK sync word */
static uint64_t fsk_sync_word = 0xC194C1; /* default FSK sync word (ALIGNED RIGHT, MSbit first) */

static bool lorawan_public = false;
static uint8_t rf_clkout = 0;

static struct lgw_tx_gain_lut_s txgain_lut = {
    .size = 2,
    .lut[0] = {
        .dig_gain = 0,
        .pa_gain = 2,
        .dac_gain = 3,
        .mix_gain = 10,
        .rf_power = 14
    },
    .lut[1] = {
        .dig_gain = 0,
        .pa_gain = 3,
        .dac_gain = 3,
        .mix_gain = 14,
        .rf_power = 27
    }
};

/* TX I/Q imbalance coefficients for mixer gain = 8 to 15 */
static int8_t cal_offset_a_i[8]; /* TX I offset for radio A */
static int8_t cal_offset_a_q[8]; /* TX Q offset for radio A */
static int8_t cal_offset_b_i[8]; /* TX I offset for radio B */
static int8_t cal_offset_b_q[8]; /* TX Q offset for radio B */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

int load_firmware(uint8_t target, uint8_t *firmware, uint16_t size);

void lgw_constant_adjust(void);

int32_t lgw_sf_getval(int x);
int32_t lgw_bw_getval(int x);

int lgw_calibrate_sx125x(uint8_t *cal_fw, uint8_t idx_start, uint8_t idx_nb);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* size is the firmware size in bytes (not 14b words) */
int load_firmware(uint8_t target, uint8_t *firmware, uint16_t size) {
    int reg_rst;
    int reg_sel;
    uint8_t fw_check[8192];
    int32_t dummy;

    /* check parameters */
    CHECK_NULL(firmware);
    if (target == MCU_ARB) {
        if (size != MCU_ARB_FW_BYTE) {
            DEBUG_MSG("ERROR: NOT A VALID SIZE FOR MCU ARG FIRMWARE\n");
            return -1;
        }
        reg_rst = LGW_MCU_RST_0;
        reg_sel = LGW_MCU_SELECT_MUX_0;
    } else if (target == MCU_AGC) {
        if (size != MCU_AGC_FW_BYTE) {
            DEBUG_MSG("ERROR: NOT A VALID SIZE FOR MCU AGC FIRMWARE\n");
            return -1;
        }
        reg_rst = LGW_MCU_RST_1;
        reg_sel = LGW_MCU_SELECT_MUX_1;
    } else {
        DEBUG_MSG("ERROR: NOT A VALID TARGET FOR LOADING FIRMWARE\n");
        return -1;
    }

    /* reset the targeted MCU */
    lgw_reg_w(reg_rst, 1);

    /* set mux to access MCU program RAM and set address to 0 */
    lgw_reg_w(reg_sel, 0);
    lgw_reg_w(LGW_MCU_PROM_ADDR, 0);

    /* write the program in one burst */
    lgw_reg_wb(LGW_MCU_PROM_DATA, firmware, size);

    /* Read back firmware code for check */
    lgw_reg_r( LGW_MCU_PROM_DATA, &dummy ); /* bug workaround */
    lgw_reg_rb( LGW_MCU_PROM_DATA, fw_check, size );
    if (memcmp(firmware, fw_check, size) != 0) {

        return -1;
    }

    /* give back control of the MCU program ram to the MCU */
    lgw_reg_w(reg_sel, 1);

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void lgw_constant_adjust(void) {

    /* I/Q path setup */
    // lgw_reg_w(LGW_RX_INVERT_IQ,0); /* default 0 */
    // lgw_reg_w(LGW_MODEM_INVERT_IQ,1); /* default 1 */
    // lgw_reg_w(LGW_CHIRP_INVERT_RX,1); /* default 1 */
    // lgw_reg_w(LGW_RX_EDGE_SELECT,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_MODEM_INVERT_IQ,0); /* default 0 */
    // lgw_reg_w(LGW_DC_NOTCH_EN,1); /* default 1 */
    lgw_reg_w(LGW_RSSI_BB_FILTER_ALPHA, 6); /* default 7 */
    lgw_reg_w(LGW_RSSI_DEC_FILTER_ALPHA, 7); /* default 5 */
    lgw_reg_w(LGW_RSSI_CHANN_FILTER_ALPHA, 7); /* default 8 */
    lgw_reg_w(LGW_RSSI_BB_DEFAULT_VALUE, 23); /* default 32 */
    lgw_reg_w(LGW_RSSI_CHANN_DEFAULT_VALUE, 85); /* default 100 */
    lgw_reg_w(LGW_RSSI_DEC_DEFAULT_VALUE, 66); /* default 100 */
    lgw_reg_w(LGW_DEC_GAIN_OFFSET, 7); /* default 8 */
    lgw_reg_w(LGW_CHAN_GAIN_OFFSET, 6); /* default 7 */

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

    /* LoRa 'multi' demodulators setup */
    // lgw_reg_w(LGW_PREAMBLE_SYMB1_NB,10); /* default 10 */
    // lgw_reg_w(LGW_FREQ_TO_TIME_INVERT,29); /* default 29 */
    // lgw_reg_w(LGW_FRAME_SYNCH_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_SYNCH_DETECT_TH,1); /* default 1 */
    // lgw_reg_w(LGW_ZERO_PAD,0); /* default 0 */
    lgw_reg_w(LGW_SNR_AVG_CST, 3); /* default 2 */
    if (lorawan_public) { /* LoRa network */
        lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else { /* private network */
        lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
    }

    // lgw_reg_w(LGW_PREAMBLE_FINE_TIMING_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_ONLY_CRC_EN,1); /* default 1 */
    // lgw_reg_w(LGW_PAYLOAD_FINE_TIMING_GAIN,2); /* default 2 */
    // lgw_reg_w(LGW_TRACKING_INTEGRAL,0); /* default 0 */
    // lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_RDX8,0); /* default 0 */
    // lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_SF12_RDX4,4092); /* default 4092 */
    // lgw_reg_w(LGW_MAX_PAYLOAD_LEN,255); /* default 255 */

    /* LoRa standalone 'MBWSSF' demodulator setup */
    // lgw_reg_w(LGW_MBWSSF_PREAMBLE_SYMB1_NB,10); /* default 10 */
    // lgw_reg_w(LGW_MBWSSF_FREQ_TO_TIME_INVERT,29); /* default 29 */
    // lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_SYNCH_DETECT_TH,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_ZERO_PAD,0); /* default 0 */
    if (lorawan_public) { /* LoRa network */
        lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else {
        lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
    }
    // lgw_reg_w(LGW_MBWSSF_ONLY_CRC_EN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_PAYLOAD_FINE_TIMING_GAIN,2); /* default 2 */
    // lgw_reg_w(LGW_MBWSSF_PREAMBLE_FINE_TIMING_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_TRACKING_INTEGRAL,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_AGC_FREEZE_ON_DETECT,1); /* default 1 */

    /* Improvement of reference clock frequency error tolerance */
    lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_RDX4, 1); /* default 0 */
    lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_SF12_RDX4, 4094); /* default 4092 */
    lgw_reg_w(LGW_CORR_MAC_GAIN, 7); /* default 5 */

    /* FSK datapath setup */
    lgw_reg_w(LGW_FSK_RX_INVERT, 1); /* default 0 */
    lgw_reg_w(LGW_FSK_MODEM_INVERT_IQ, 1); /* default 0 */

    /* FSK demodulator setup */
    lgw_reg_w(LGW_FSK_RSSI_LENGTH, 4); /* default 0 */
    lgw_reg_w(LGW_FSK_PKT_MODE, 1); /* variable length, default 0 */
    lgw_reg_w(LGW_FSK_CRC_EN, 1); /* default 0 */
    lgw_reg_w(LGW_FSK_DCFREE_ENC, 2); /* default 0 */
    // lgw_reg_w(LGW_FSK_CRC_IBM,0); /* default 0 */
    lgw_reg_w(LGW_FSK_ERROR_OSR_TOL, 10); /* default 0 */
    lgw_reg_w(LGW_FSK_PKT_LENGTH, 255); /* max packet length in variable length mode */
    // lgw_reg_w(LGW_FSK_NODE_ADRS,0); /* default 0 */
    // lgw_reg_w(LGW_FSK_BROADCAST,0); /* default 0 */
    // lgw_reg_w(LGW_FSK_AUTO_AFC_ON,0); /* default 0 */
    lgw_reg_w(LGW_FSK_PATTERN_TIMEOUT_CFG, 128); /* sync timeout (allow 8 bytes preamble + 8 bytes sync word, default 0 */

    /* TX general parameters */
    lgw_reg_w(LGW_TX_START_DELAY, TX_START_DELAY); /* default 0 */

    /* TX LoRa */
    // lgw_reg_w(LGW_TX_MODE,0); /* default 0 */
    lgw_reg_w(LGW_TX_SWAP_IQ, 1); /* "normal" polarity; default 0 */
    if (lorawan_public) { /* LoRa network */
        lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else { /* Private network */
        lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
    }

    /* TX FSK */
    // lgw_reg_w(LGW_FSK_TX_GAUSSIAN_EN,1); /* default 1 */
    lgw_reg_w(LGW_FSK_TX_GAUSSIAN_SELECT_BT, 2); /* Gaussian filter always on TX, default 0 */
    // lgw_reg_w(LGW_FSK_TX_PATTERN_EN,1); /* default 1 */
    // lgw_reg_w(LGW_FSK_TX_PREAMBLE_SEQ,0); /* default 0 */

    return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t lgw_bw_getval(int x) {
    switch (x) {
        case BW_500KHZ:
            return 500000;
        case BW_250KHZ:
            return 250000;
        case BW_125KHZ:
            return 125000;
        case BW_62K5HZ:
            return 62500;
        case BW_31K2HZ:
            return 31200;
        case BW_15K6HZ:
            return 15600;
        case BW_7K8HZ :
            return 7800;
        default:
            return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t lgw_sf_getval(int x) {
    switch (x) {
        case DR_LORA_SF7:
            return 7;
        case DR_LORA_SF8:
            return 8;
        case DR_LORA_SF9:
            return 9;
        case DR_LORA_SF10:
            return 10;
        case DR_LORA_SF11:
            return 11;
        case DR_LORA_SF12:
            return 12;
        default:
            return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_calibrate_sx125x(uint8_t *cal_fw, uint8_t idx_start, uint8_t idx_nb) {
    int i, err;
    int32_t read_val;
    uint8_t fw_version;
    uint8_t cal_cmd;
    uint16_t cal_time;
    uint8_t cal_status;

    /* check parameters */
    if (cal_fw == NULL) {
        DEBUG_MSG("ERROR: invalid parameter, null pointer\n");
        return LGW_HAL_ERROR;
    }
    if ((idx_start < 5) || (idx_start > 15)) {
        DEBUG_MSG("ERROR: invalid parameter, calibration offset index start must be [5-15]\n");
        return LGW_HAL_ERROR;
    }
    if (idx_nb > 8) {
        DEBUG_MSG("ERROR: invalid parameter, calibration offset index number must be <= 8\n");
        return LGW_HAL_ERROR;
    }

    /* reset the registers (also shuts the radios down) */
    lgw_soft_reset();

    /* gate clocks */
    lgw_reg_w(LGW_GLOBAL_EN, 0);
    lgw_reg_w(LGW_CLK32M_EN, 0);

#if 0
    /* switch on and reset the radios (also starts the 32 MHz XTAL) */
    lgw_reg_w(LGW_RADIO_A_EN, 1);
    lgw_reg_w(LGW_RADIO_B_EN, 1);
    wait_ms(500); /* TODO: optimize */
#endif
    lgw_reg_w(LGW_RADIO_RST, 1);
    wait_ms(5);
    lgw_reg_w(LGW_RADIO_RST, 0);

    /* setup the radios */
    err = lgw_setup_sx125x(0, rf_clkout, rf_enable[0], rf_radio_type[0], rf_rx_freq[0]);
    if (err != 0) {
        DEBUG_MSG("ERROR: Failed to setup sx125x radio for RF chain 0\n");
        return LGW_HAL_ERROR;
    }
    err = lgw_setup_sx125x(1, rf_clkout, rf_enable[1], rf_radio_type[1], rf_rx_freq[1]);
    if (err != 0) {
        DEBUG_MSG("ERROR: Failed to setup sx125x radio for RF chain 0\n");
        return LGW_HAL_ERROR;
    }

    /* Enable clocks */
    lgw_reg_w(LGW_GLOBAL_EN, 1);
    lgw_reg_w(LGW_CLK32M_EN, 1);

    /* GPIOs table :
    DGPIO0 -> N/A
    DGPIO1 -> N/A
    DGPIO2 -> N/A
    DGPIO3 -> TX digital filter ON
    DGPIO4 -> TX ON
    */

    /* select calibration command */
    cal_cmd = 0;
    cal_cmd |= rf_enable[0] ? 0x01 : 0x00; /* Bit 0: Calibrate Rx IQ mismatch compensation on radio A */
    cal_cmd |= rf_enable[1] ? 0x02 : 0x00; /* Bit 1: Calibrate Rx IQ mismatch compensation on radio B */
    cal_cmd |= (rf_enable[0] && rf_tx_enable[0]) ? 0x04 : 0x00; /* Bit 2: Calibrate Tx DC offset on radio A */
    cal_cmd |= (rf_enable[1] && rf_tx_enable[1]) ? 0x08 : 0x00; /* Bit 3: Calibrate Tx DC offset on radio B */
    cal_cmd |= 0x10; /* Bit 4: 0: calibrate with DAC gain=2, 1: with DAC gain=3 (use 3) */

    switch (rf_radio_type[0]) { /* we assume that there is only one radio type on the board */
        case LGW_RADIO_TYPE_SX1255:
            cal_cmd |= 0x20; /* Bit 5: 0: SX1257, 1: SX1255 */
            break;
        case LGW_RADIO_TYPE_SX1257:
            cal_cmd |= 0x00; /* Bit 5: 0: SX1257, 1: SX1255 */
            break;
        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d FOR RADIO TYPE\n", rf_radio_type[0]);
            break;
    }

    cal_cmd |= 0x00; /* Bit 6-7: Board type 0: ref, 1: FPGA, 3: board X */
    cal_time = 2300; /* measured between 2.1 and 2.2 sec, because 1 TX only */

    /* Load the calibration firmware  */
    load_firmware(MCU_AGC, cal_fw, MCU_AGC_FW_BYTE);
    lgw_reg_w(LGW_FORCE_HOST_RADIO_CTRL, 0); /* gives to AGC MCU the control of the radios */
    lgw_reg_w(LGW_RADIO_SELECT, cal_cmd); /* send calibration configuration word */
    lgw_reg_w(LGW_MCU_RST_1, 0);

    /* Check firmware version */
    lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, FW_VERSION_ADDR);
    lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
    fw_version = (uint8_t)read_val;
    if (fw_version != FW_VERSION_CAL) {

        return LGW_HAL_ERROR;
    }

    lgw_reg_w(LGW_PAGE_REG, 3); /* Calibration will start on this condition as soon as MCU can talk to concentrator registers */
    lgw_reg_w(LGW_EMERGENCY_FORCE_HOST_CTRL, 0); /* Give control of concentrator registers to MCU */

    /* Wait for calibration to end */
    DEBUG_PRINTF("Note: calibration started (time: %u ms)\n", cal_time);
    wait_ms(cal_time); /* Wait for end of calibration */
    lgw_reg_w(LGW_EMERGENCY_FORCE_HOST_CTRL, 1); /* Take back control */

    /* Get calibration status */
    lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    cal_status = (uint8_t)read_val;
    /*
        bit 7: calibration finished
        bit 0: could access SX1301 registers
        bit 1: could access radio A registers
        bit 2: could access radio B registers
        bit 3: radio A RX image rejection successful
        bit 4: radio B RX image rejection successful
        bit 5: radio A TX DC Offset correction successful
        bit 6: radio B TX DC Offset correction successful
    */
    if ((cal_status & 0x81) != 0x81) {
        DEBUG_PRINTF("ERROR: CALIBRATION FAILURE (STATUS = 0x%X)\n", cal_status);
        return LGW_HAL_ERROR;
    } else {
        DEBUG_PRINTF("Note: calibration finished (status = 0x%X)\n", cal_status);
    }
    if (rf_enable[0] && ((cal_status & 0x02) == 0)) {
        DEBUG_MSG("ERROR: calibration could not access radio A\n");
        return LGW_HAL_ERROR;
    }
    if (rf_enable[1] && ((cal_status & 0x04) == 0)) {
        DEBUG_MSG("ERROR: calibration could not access radio B\n");
        return LGW_HAL_ERROR;
    }
    if (rf_enable[0] && ((cal_status & 0x08) == 0)) {
        DEBUG_MSG("WARNING: problem in calibration of radio A for image rejection\n");
    }
    if (rf_enable[1] && ((cal_status & 0x10) == 0)) {
        DEBUG_MSG("WARNING: problem in calibration of radio B for image rejection\n");
    }
    if (rf_enable[0] && rf_tx_enable[0] && ((cal_status & 0x20) == 0)) {
        DEBUG_MSG("WARNING: problem in calibration of radio A for TX DC offset\n");
    }
    if (rf_enable[1] && rf_tx_enable[1] && ((cal_status & 0x40) == 0)) {
        DEBUG_MSG("WARNING: problem in calibration of radio B for TX DC offset\n");
    }

    /* Get TX DC offset values */
    for(i = 0; i < (int)idx_nb; ++i) {
        lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xA0 + i);
        lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        cal_offset_a_i[i] = (int8_t)read_val;
        lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xA8 + i);
        lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        cal_offset_a_q[i] = (int8_t)read_val;
        lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xB0 + i);
        lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        cal_offset_b_i[i] = (int8_t)read_val;
        lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xB8 + i);
        lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        cal_offset_b_q[i] = (int8_t)read_val;
        DEBUG_PRINTF("calibration a_i = %d\n", cal_offset_a_i[i]);
    }

    /* Require MCU to capture calibration values */
    lgw_mcu_commit_radio_calibration(idx_start, idx_nb);

    return LGW_HAL_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_board_setconf(struct lgw_conf_board_s* conf) {
    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        DEBUG_MSG("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return LGW_HAL_ERROR;
    }

    /* set internal config according to parameters */
    lorawan_public = conf->lorawan_public;
    rf_clkout = conf->clksrc;

    DEBUG_PRINTF("Note: board configuration; lorawan_public:%d, clksrc:%d\n", lorawan_public, rf_clkout);

    /* send configuration to concentrator MCU */
    return lgw_mcu_board_setconf(conf);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_rxrf_setconf(uint8_t rf_chain, struct lgw_conf_rxrf_s* conf) {
    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        DEBUG_MSG("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return LGW_HAL_ERROR;
    }

    /* check input range (segfault prevention) */
    if (rf_chain >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: NOT A VALID RF_CHAIN NUMBER\n");
        return LGW_HAL_ERROR;
    }

    /* check if radio type is supported */
    if ((conf->type != LGW_RADIO_TYPE_SX1255) && (conf->type != LGW_RADIO_TYPE_SX1257)) {
        DEBUG_MSG("ERROR: NOT A VALID RADIO TYPE\n");
        return LGW_HAL_ERROR;
    }

    /* set internal config according to parameters */
    rf_enable[rf_chain] = conf->enable;
    rf_rx_freq[rf_chain] = conf->freq_hz;
    rf_rssi_offset[rf_chain] = conf->rssi_offset;
    rf_radio_type[rf_chain] = conf->type;
    rf_tx_enable[rf_chain] = conf->tx_enable;

    DEBUG_PRINTF("Note: rf_chain %d configuration; en:%d freq:%d rssi_offset:%f radio_type:%d tx_enable:%d\n", rf_chain, rf_enable  [rf_chain], rf_rx_freq[rf_chain], rf_rssi_offset[rf_chain], rf_radio_type[rf_chain], rf_tx_enable[rf_chain]);

    /* send configuration to concentrator MCU */
    return lgw_mcu_rxrf_setconf(rf_chain, conf);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_rxif_setconf(uint8_t if_chain, struct lgw_conf_rxif_s* conf) {
    int32_t bw_hz;
    uint32_t rf_rx_bandwidth;

    /* check if the concentrator is running */
    if (lgw_is_started == true) {
        DEBUG_MSG("ERROR: CONCENTRATOR IS RUNNING, STOP IT BEFORE TOUCHING CONFIGURATION\n");
        return LGW_HAL_ERROR;
    }

    /* check input range (segfault prevention) */
    if (if_chain >= LGW_IF_CHAIN_NB) {
        DEBUG_PRINTF("ERROR: %d NOT A VALID IF_CHAIN NUMBER\n", if_chain);
        return LGW_HAL_ERROR;
    }

    /* if chain is disabled, don't care about most parameters */
    if (conf->enable == false) {
        if_enable[if_chain] = false;
        if_freq[if_chain] = 0;
        DEBUG_PRINTF("Note: if_chain %d disabled\n", if_chain);
        return LGW_HAL_SUCCESS;
    }

    /* check 'general' parameters */
    if (ifmod_config[if_chain] == IF_UNDEFINED) {
        DEBUG_PRINTF("ERROR: IF CHAIN %d NOT CONFIGURABLE\n", if_chain);
    }
    if (conf->rf_chain >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN TO ASSOCIATE WITH A LORA_STD IF CHAIN\n");
        return LGW_HAL_ERROR;
    }
    switch (conf->bandwidth) {
        case BW_250KHZ:
            rf_rx_bandwidth = LGW_RF_RX_BANDWIDTH_250KHZ;
            break;
        case BW_500KHZ:
            rf_rx_bandwidth = LGW_RF_RX_BANDWIDTH_500KHZ;
            break;
        default:
            rf_rx_bandwidth = LGW_RF_RX_BANDWIDTH_125KHZ;
            break;
    }

    bw_hz = lgw_bw_getval(conf->bandwidth);
    if ((conf->freq_hz + ((bw_hz == -1) ? LGW_REF_BW : bw_hz) / 2) > ((int32_t)rf_rx_bandwidth / 2)) {
        DEBUG_PRINTF("ERROR: IF FREQUENCY %d TOO HIGH\n", conf->freq_hz);
        return LGW_HAL_ERROR;
    } else if ((conf->freq_hz - ((bw_hz == -1) ? LGW_REF_BW : bw_hz) / 2) < -((int32_t)rf_rx_bandwidth / 2)) {
        DEBUG_PRINTF("ERROR: IF FREQUENCY %d TOO LOW\n", conf->freq_hz);
        return LGW_HAL_ERROR;
    }

    /* check parameters according to the type of IF chain + modem,
    fill default if necessary, and commit configuration if everything is OK */
    switch (ifmod_config[if_chain]) {
        case IF_LORA_STD:
            /* fill default parameters if needed */
            if (conf->bandwidth == BW_UNDEFINED) {
                conf->bandwidth = BW_250KHZ;
            }
            if (conf->datarate == DR_UNDEFINED) {
                conf->datarate = DR_LORA_SF9;
            }
            /* check BW & DR */
            if (!IS_LORA_BW(conf->bandwidth)) {
                DEBUG_MSG("ERROR: BANDWIDTH NOT SUPPORTED BY LORA_STD IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            if (!IS_LORA_STD_DR(conf->datarate)) {
                DEBUG_MSG("ERROR: DATARATE NOT SUPPORTED BY LORA_STD IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            /* set internal configuration  */
            if_enable[if_chain] = conf->enable;
            if_rf_chain[if_chain] = conf->rf_chain;
            if_freq[if_chain] = conf->freq_hz;
            lora_rx_bw = conf->bandwidth;
            lora_rx_sf = (uint8_t)(DR_LORA_MULTI & conf->datarate); /* filter SF out of the 7-12 range */
            if (SET_PPM_ON(conf->bandwidth, conf->datarate)) {
                lora_rx_ppm_offset = true;
            } else {
                lora_rx_ppm_offset = false;
            }

            DEBUG_PRINTF("Note: LoRa 'std' if_chain %d configuration; en:%d freq:%d bw:%d dr:%d\n", if_chain, if_enable[if_chain], if_freq[if_chain], lora_rx_bw, lora_rx_sf);
            break;

        case IF_LORA_MULTI:
            /* fill default parameters if needed */
            if (conf->bandwidth == BW_UNDEFINED) {
                conf->bandwidth = BW_125KHZ;
            }
            if (conf->datarate == DR_UNDEFINED) {
                conf->datarate = DR_LORA_MULTI;
            }
            /* check BW & DR */
            if (conf->bandwidth != BW_125KHZ) {
                DEBUG_MSG("ERROR: BANDWIDTH NOT SUPPORTED BY LORA_MULTI IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            if (!IS_LORA_MULTI_DR(conf->datarate)) {
                DEBUG_MSG("ERROR: DATARATE(S) NOT SUPPORTED BY LORA_MULTI IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            /* set internal configuration  */
            if_enable[if_chain] = conf->enable;
            if_rf_chain[if_chain] = conf->rf_chain;
            if_freq[if_chain] = conf->freq_hz;
            lora_multi_sfmask[if_chain] = (uint8_t)(DR_LORA_MULTI & conf->datarate); /* filter SF out of the 7-12 range */

            DEBUG_PRINTF("Note: LoRa 'multi' if_chain %d configuration; en:%d freq:%d SF_mask:0x%02x\n", if_chain, if_enable[if_chain], if_freq[if_chain], lora_multi_sfmask[if_chain]);
            break;

        case IF_FSK_STD:
            /* fill default parameters if needed */
            if (conf->bandwidth == BW_UNDEFINED) {
                conf->bandwidth = BW_250KHZ;
            }
            if (conf->datarate == DR_UNDEFINED) {
                conf->datarate = 64000; /* default datarate */
            }
            /* check BW & DR */
            if(!IS_FSK_BW(conf->bandwidth)) {
                DEBUG_MSG("ERROR: BANDWIDTH NOT SUPPORTED BY FSK IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            if(!IS_FSK_DR(conf->datarate)) {
                DEBUG_MSG("ERROR: DATARATE NOT SUPPORTED BY FSK IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            /* set internal configuration  */
            if_enable[if_chain] = conf->enable;
            if_rf_chain[if_chain] = conf->rf_chain;
            if_freq[if_chain] = conf->freq_hz;
            fsk_rx_bw = conf->bandwidth;
            fsk_rx_dr = conf->datarate;
            if (conf->sync_word > 0) {
                fsk_sync_word_size = conf->sync_word_size;
                fsk_sync_word = conf->sync_word;
            }
            DEBUG_PRINTF("Note: FSK if_chain %d configuration; en:%d freq:%d bw:%u dr:%u (%u real dr) sync:0x%0*" PRIx64 "\n", if_chain, if_enable[if_chain], if_freq[if_chain], fsk_rx_bw, fsk_rx_dr, LGW_XTAL_FREQU / (LGW_XTAL_FREQU / fsk_rx_dr), 2 * fsk_sync_word_size, fsk_sync_word);
            break;

        default:
            DEBUG_PRINTF("ERROR: IF CHAIN %d TYPE NOT SUPPORTED\n", if_chain);
            return LGW_HAL_ERROR;
    }

    /* send configuration to concentrator MCU */
    return lgw_mcu_rxif_setconf(if_chain, conf);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_txgain_setconf(struct lgw_tx_gain_lut_s *conf) {
    int i;

    /* Check LUT size */
    if ((conf->size < 1) || (conf->size > TX_GAIN_LUT_SIZE_MAX)) {
        DEBUG_PRINTF("ERROR: TX gain LUT must have at least one entry and  maximum %d entries\n", TX_GAIN_LUT_SIZE_MAX);
        return LGW_HAL_ERROR;
    }

    txgain_lut.size = conf->size;

    for (i = 0; i < txgain_lut.size; i++) {
        /* Check gain range */
        if (conf->lut[i].dig_gain > 3) {
            DEBUG_MSG("ERROR: TX gain LUT: SX1301 digital gain must be between 0 and 3\n");
            return LGW_HAL_ERROR;
        }
        if (conf->lut[i].dac_gain != 3) {
            DEBUG_MSG("ERROR: TX gain LUT: SX1257 DAC gains != 3 are not supported\n");
            return LGW_HAL_ERROR;
        }
        if (conf->lut[i].mix_gain > 15) {
            DEBUG_MSG("ERROR: TX gain LUT: SX1257 mixer gain must not exceed 15\n");
            return LGW_HAL_ERROR;
        }
        if (conf->lut[i].pa_gain > 3) {
            DEBUG_MSG("ERROR: TX gain LUT: External PA gain must not exceed 3\n");
            return LGW_HAL_ERROR;
        }

        /* Set internal LUT */
        txgain_lut.lut[i].dig_gain = conf->lut[i].dig_gain;
        txgain_lut.lut[i].dac_gain = conf->lut[i].dac_gain;
        txgain_lut.lut[i].mix_gain = conf->lut[i].mix_gain;
        txgain_lut.lut[i].pa_gain  = conf->lut[i].pa_gain;
        txgain_lut.lut[i].rf_power = conf->lut[i].rf_power;
    }

    /* send configuration to concentrator MCU */
    return lgw_mcu_txgain_setconf(conf);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_start(void) {
    int i, err;
    unsigned x;
    uint8_t radio_select;
    int32_t read_val;
    uint8_t load_val;
    uint8_t fw_version;
    uint64_t fsk_sync_word_reg;

    if (lgw_is_started == true) {
        DEBUG_MSG("Note: LoRa concentrator already started, restarting it now\n");
    }

    /* Calibrate radios */
    err = lgw_calibrate_sx125x(callow_firmware, 5, 8);
    if (err != LGW_HAL_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to calibrate sx125x radios (5-12)\n");
        return LGW_HAL_ERROR;
    }
    wait_ms(5);
    err = lgw_calibrate_sx125x(cal_firmware, 8, 8);
    if (err != LGW_HAL_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to calibrate sx125x radios (8-15)\n");
        return LGW_HAL_ERROR;
    }

    /* RX and TX packets signalling through GPIOs */
    lgw_reg_w(LGW_GPIO_MODE, 31); /* Set all GPIOs as output */
    lgw_reg_w(LGW_GPIO_SELECT_OUTPUT, 0);

    /* load adjusted parameters */
    lgw_constant_adjust();

    /* Sanity check for RX frequency */
    if (rf_rx_freq[0] == 0) {
        DEBUG_MSG("ERROR: wrong configuration, rf_rx_freq[0] is not set\n");
        return LGW_HAL_ERROR;
    }

    /* Freq-to-time-drift calculation */
    x = 4096000000 / (rf_rx_freq[0] >> 1); /* dividend: (4*2048*1000000) >> 1, rescaled to avoid 32b overflow */
    x = ( x > 63 ) ? 63 : x; /* saturation */
    lgw_reg_w(LGW_FREQ_TO_TIME_DRIFT, x); /* default 9 */

    x = 4096000000 / (rf_rx_freq[0] >> 3); /* dividend: (16*2048*1000000) >> 3, rescaled to avoid 32b overflow */
    x = ( x > 63 ) ? 63 : x; /* saturation */
    lgw_reg_w(LGW_MBWSSF_FREQ_TO_TIME_DRIFT, x); /* default 36 */

    /* configure LoRa 'multi' demodulators aka. LoRa 'sensor' channels (IF0-3) */
    radio_select = 0; /* IF mapping to radio A/B (per bit, 0=A, 1=B) */
    for(i = 0; i < LGW_MULTI_NB; ++i) {
        radio_select += (if_rf_chain[i] == 1 ? 1 << i : 0); /* transform bool array into binary word */
    }
    /*
    lgw_reg_w(LGW_RADIO_SELECT, radio_select);

    LGW_RADIO_SELECT is used for communication with the firmware, "radio_select"
    will be loaded in LGW_RADIO_SELECT at the end of start procedure.
    */

    lgw_reg_w(LGW_IF_FREQ_0, IF_HZ_TO_REG(if_freq[0])); /* default -384 */
    lgw_reg_w(LGW_IF_FREQ_1, IF_HZ_TO_REG(if_freq[1])); /* default -128 */
    lgw_reg_w(LGW_IF_FREQ_2, IF_HZ_TO_REG(if_freq[2])); /* default 128 */
    lgw_reg_w(LGW_IF_FREQ_3, IF_HZ_TO_REG(if_freq[3])); /* default 384 */
    lgw_reg_w(LGW_IF_FREQ_4, IF_HZ_TO_REG(if_freq[4])); /* default -384 */
    lgw_reg_w(LGW_IF_FREQ_5, IF_HZ_TO_REG(if_freq[5])); /* default -128 */
    lgw_reg_w(LGW_IF_FREQ_6, IF_HZ_TO_REG(if_freq[6])); /* default 128 */
    lgw_reg_w(LGW_IF_FREQ_7, IF_HZ_TO_REG(if_freq[7])); /* default 384 */

    lgw_reg_w(LGW_CORR0_DETECT_EN, (if_enable[0] == true) ? lora_multi_sfmask[0] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR1_DETECT_EN, (if_enable[1] == true) ? lora_multi_sfmask[1] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR2_DETECT_EN, (if_enable[2] == true) ? lora_multi_sfmask[2] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR3_DETECT_EN, (if_enable[3] == true) ? lora_multi_sfmask[3] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR4_DETECT_EN, (if_enable[4] == true) ? lora_multi_sfmask[4] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR5_DETECT_EN, (if_enable[5] == true) ? lora_multi_sfmask[5] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR6_DETECT_EN, (if_enable[6] == true) ? lora_multi_sfmask[6] : 0); /* default 0 */
    lgw_reg_w(LGW_CORR7_DETECT_EN, (if_enable[7] == true) ? lora_multi_sfmask[7] : 0); /* default 0 */

    lgw_reg_w(LGW_PPM_OFFSET, 0x60); /* as the threshold is 16ms, use 0x60 to enable ppm_offset for SF12 and SF11 @125kHz*/

    lgw_reg_w(LGW_CONCENTRATOR_MODEM_ENABLE, 1); /* default 0 */

    /* configure LoRa 'stand-alone' modem (IF8) */
    lgw_reg_w(LGW_IF_FREQ_8, IF_HZ_TO_REG(if_freq[8])); /* MBWSSF modem (default 0) */
    if (if_enable[8] == true) {
        lgw_reg_w(LGW_MBWSSF_RADIO_SELECT, if_rf_chain[8]);
        switch(lora_rx_bw) {
            case BW_125KHZ:
                lgw_reg_w(LGW_MBWSSF_MODEM_BW, 0);
                break;
            case BW_250KHZ:
                lgw_reg_w(LGW_MBWSSF_MODEM_BW, 1);
                break;
            case BW_500KHZ:
                lgw_reg_w(LGW_MBWSSF_MODEM_BW, 2);
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", lora_rx_bw);
                return LGW_HAL_ERROR;
        }
        switch(lora_rx_sf) {
            case DR_LORA_SF7:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 7);
                break;
            case DR_LORA_SF8:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 8);
                break;
            case DR_LORA_SF9:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 9);
                break;
            case DR_LORA_SF10:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 10);
                break;
            case DR_LORA_SF11:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 11);
                break;
            case DR_LORA_SF12:
                lgw_reg_w(LGW_MBWSSF_RATE_SF, 12);
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", lora_rx_sf);
                return LGW_HAL_ERROR;
        }
        lgw_reg_w(LGW_MBWSSF_PPM_OFFSET, lora_rx_ppm_offset); /* default 0 */
        lgw_reg_w(LGW_MBWSSF_MODEM_ENABLE, 1); /* default 0 */
    } else {
        lgw_reg_w(LGW_MBWSSF_MODEM_ENABLE, 0);
    }

    /* configure FSK modem (IF9) */
    lgw_reg_w(LGW_IF_FREQ_9, IF_HZ_TO_REG(if_freq[9])); /* FSK modem, default 0 */
    lgw_reg_w(LGW_FSK_PSIZE, fsk_sync_word_size - 1);
    lgw_reg_w(LGW_FSK_TX_PSIZE, fsk_sync_word_size - 1);
    fsk_sync_word_reg = fsk_sync_word << (8 * (8 - fsk_sync_word_size));
    lgw_reg_w(LGW_FSK_REF_PATTERN_LSB, (uint32_t)(0xFFFFFFFF & fsk_sync_word_reg));
    lgw_reg_w(LGW_FSK_REF_PATTERN_MSB, (uint32_t)(0xFFFFFFFF & (fsk_sync_word_reg >> 32)));
    if (if_enable[9] == true) {
        lgw_reg_w(LGW_FSK_RADIO_SELECT, if_rf_chain[9]);
        lgw_reg_w(LGW_FSK_BR_RATIO, LGW_XTAL_FREQU / fsk_rx_dr); /* setting the dividing ratio for datarate */
        lgw_reg_w(LGW_FSK_CH_BW_EXPO, fsk_rx_bw);
        lgw_reg_w(LGW_FSK_MODEM_ENABLE, 1); /* default 0 */
    } else {
        lgw_reg_w(LGW_FSK_MODEM_ENABLE, 0);
    }

    /* Load firmware */
    load_firmware(MCU_ARB, arb_firmware, MCU_ARB_FW_BYTE);
    load_firmware(MCU_AGC, agc_firmware, MCU_AGC_FW_BYTE);

    /* gives the AGC MCU control over radio, RF front-end and filter gain */
    lgw_reg_w(LGW_FORCE_HOST_RADIO_CTRL, 0);
    lgw_reg_w(LGW_FORCE_HOST_FE_CTRL, 0);
    lgw_reg_w(LGW_FORCE_DEC_FILTER_GAIN, 0);

    /* Get MCUs out of reset */
    lgw_reg_w(LGW_RADIO_SELECT, 0); /* MUST not be = to 1 or 2 at firmware init */
    lgw_reg_w(LGW_MCU_RST_0, 0);
    lgw_reg_w(LGW_MCU_RST_1, 0);

    /* Check firmware version */
    lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, FW_VERSION_ADDR);
    lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
    fw_version = (uint8_t)read_val;
    if (fw_version != FW_VERSION_AGC) {
        DEBUG_PRINTF("ERROR: Version of AGC firmware not expected, actual:%d expected:%d\n", fw_version, FW_VERSION_AGC);
        return LGW_HAL_ERROR;
    }
    lgw_reg_w(LGW_DBG_ARB_MCU_RAM_ADDR, FW_VERSION_ADDR);
    lgw_reg_r(LGW_DBG_ARB_MCU_RAM_DATA, &read_val);
    fw_version = (uint8_t)read_val;
    if (fw_version != FW_VERSION_ARB) {
        DEBUG_PRINTF("ERROR: Version of arbiter firmware not expected, actual:%d expected:%d\n", fw_version, FW_VERSION_ARB);
        return LGW_HAL_ERROR;
    }

    DEBUG_MSG("Info: Initialising AGC firmware...\n");
    wait_ms(10);

    lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x10) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* Update Tx gain LUT and start AGC */
    for (i = 0; i < txgain_lut.size; ++i) {
        lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT); /* start a transaction */
        wait_ms(1);
        load_val = txgain_lut.lut[i].mix_gain + (16 * txgain_lut.lut[i].dac_gain) + (64 * txgain_lut.lut[i].pa_gain);
        lgw_reg_w(LGW_RADIO_SELECT, load_val);
        wait_ms(1);
        lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
        if (read_val != (0x30 + i)) {
            DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
            return LGW_HAL_ERROR;
        }
    }
    /* As the AGC fw is waiting for 16 entries, we need to abort the transaction if we get less entries */
    if (txgain_lut.size < TX_GAIN_LUT_SIZE_MAX) {
        lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
        wait_ms(10);
        load_val = AGC_CMD_ABORT;
        lgw_reg_w(LGW_RADIO_SELECT, load_val);
        wait_ms(10);
        lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
        if (read_val != 0x30) {
            DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
            return LGW_HAL_ERROR;
        }
    }

    /* Load Tx freq MSBs (always 3 if f > 768 for SX1257 or f > 384 for SX1255 */
    lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    wait_ms(10);
    lgw_reg_w(LGW_RADIO_SELECT, 3);
    wait_ms(10);
    lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x33) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* Load chan_select firmware option */
    lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    wait_ms(10);
    lgw_reg_w(LGW_RADIO_SELECT, 0);
    wait_ms(10);
    lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x30) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* End AGC firmware init and check status */
    lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    wait_ms(10);
    lgw_reg_w(LGW_RADIO_SELECT, radio_select); /* Load intended value of RADIO_SELECT */
    wait_ms(10);
    DEBUG_MSG("Info: putting back original RADIO_SELECT value\n");
    lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x40) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* enable GPS event capture */
    lgw_reg_w(LGW_GPS_EN, 0);

    lgw_is_started = true;
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_stop(void) {
    lgw_soft_reset();
    lgw_disconnect();

    lgw_is_started = false;
    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data) {
    /* check input variables */
    if ((max_pkt == 0) || (max_pkt > LGW_PKT_FIFO_SIZE)) {
        DEBUG_PRINTF("ERROR: %d = INVALID MAX NUMBER OF PACKETS TO FETCH\n", max_pkt);
        return LGW_HAL_ERROR;
    }

    /* send packet data to concentrator MCU */
    return lgw_mcu_receive( max_pkt, pkt_data);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_send(struct lgw_pkt_tx_s* pkt_data) {
    /* check input range (segfault prevention) */
    if (pkt_data->rf_chain >= LGW_RF_CHAIN_NB) {
        DEBUG_MSG("ERROR: INVALID RF_CHAIN TO SEND PACKETS\n");
        return LGW_HAL_ERROR;
    }

    /* check input variables */
    if (rf_tx_enable[pkt_data->rf_chain] == false) {
        DEBUG_MSG("ERROR: SELECTED RF_CHAIN IS DISABLED FOR TX ON SELECTED BOARD\n");
        return LGW_HAL_ERROR;
    }
    if (rf_enable[pkt_data->rf_chain] == false) {
        DEBUG_MSG("ERROR: SELECTED RF_CHAIN IS DISABLED\n");
        return LGW_HAL_ERROR;
    }
    if (!IS_TX_MODE(pkt_data->tx_mode)) {
        DEBUG_MSG("ERROR: TX_MODE NOT SUPPORTED\n");
        return LGW_HAL_ERROR;
    }
    if (pkt_data->modulation == MOD_LORA) {
        if (!IS_LORA_BW(pkt_data->bandwidth)) {
            DEBUG_MSG("ERROR: BANDWIDTH NOT SUPPORTED BY LORA TX\n");
            return LGW_HAL_ERROR;
        }
        if (!IS_LORA_STD_DR(pkt_data->datarate)) {
            DEBUG_MSG("ERROR: DATARATE NOT SUPPORTED BY LORA TX\n");
            return LGW_HAL_ERROR;
        }
        if (!IS_LORA_CR(pkt_data->coderate)) {
            DEBUG_MSG("ERROR: CODERATE NOT SUPPORTED BY LORA TX\n");
            return LGW_HAL_ERROR;
        }
        if (pkt_data->size > 255) {
            DEBUG_MSG("ERROR: PAYLOAD LENGTH TOO BIG FOR LORA TX\n");
            return LGW_HAL_ERROR;
        }
    } else if (pkt_data->modulation == MOD_FSK) {
        if ((pkt_data->f_dev < 1) || (pkt_data->f_dev > 200)) {
            DEBUG_MSG("ERROR: TX FREQUENCY DEVIATION OUT OF ACCEPTABLE RANGE\n");
            return LGW_HAL_ERROR;
        }
        if (!IS_FSK_DR(pkt_data->datarate)) {
            DEBUG_MSG("ERROR: DATARATE NOT SUPPORTED BY FSK IF CHAIN\n");
            return LGW_HAL_ERROR;
        }
        if (pkt_data->size > 255) {
            DEBUG_MSG("ERROR: PAYLOAD LENGTH TOO BIG FOR FSK TX\n");
            return LGW_HAL_ERROR;
        }
    } else {
        DEBUG_MSG("ERROR: INVALID TX MODULATION\n");
        return LGW_HAL_ERROR;
    }

    /* send packet data to concentrator MCU */
    return lgw_mcu_send(pkt_data);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_status(uint8_t select, uint8_t *code) {
    int32_t read_value;

    /* check input variables */
    CHECK_NULL(code);

    if (select == TX_STATUS) {
        lgw_reg_r(LGW_TX_STATUS, &read_value);
        if (lgw_is_started == false) {
            *code = TX_OFF;
        } else if ((read_value & 0x10) == 0) { /* bit 4 @1: TX programmed */
            *code = TX_FREE;
        } else if ((read_value & 0x60) != 0) { /* bit 5 or 6 @1: TX sequence */
            *code = TX_EMITTING;
        } else {
            *code = TX_SCHEDULED;
        }
        return LGW_HAL_SUCCESS;

    } else if (select == RX_STATUS) {
        *code = RX_STATUS_UNKNOWN; /* todo */
        return LGW_HAL_SUCCESS;

    } else {
        DEBUG_MSG("ERROR: SELECTION INVALID, NO STATUS TO RETURN\n");
        return LGW_HAL_ERROR;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_abort_tx(void) {
    return lgw_reg_w(LGW_TX_TRIG_ALL, 0);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_trigcnt(uint32_t* trig_cnt_us) {
    return lgw_mcu_get_trigcnt(trig_cnt_us);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char* lgw_version_info() {
    return lgw_version_string;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_version_info() {
    return (int)(STM32FWVERSION);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint32_t lgw_time_on_air(struct lgw_pkt_tx_s *packet) {
    int32_t val;
    uint8_t SF, H, DE;
    uint16_t BW;
    uint32_t payloadSymbNb, Tpacket;
    double Tsym, Tpreamble, Tpayload, Tfsk;

    if (packet == NULL) {
        DEBUG_MSG("ERROR: Failed to compute time on air, wrong parameter\n");
        return 0;
    }

    if (packet->modulation == MOD_LORA) {
        /* Get bandwidth */
        val = lgw_bw_getval(packet->bandwidth);
        if (val != -1) {
            BW = (uint16_t)(val / 1E3);
        } else {
            DEBUG_PRINTF("ERROR: Cannot compute time on air for this packet, unsupported bandwidth (0x%02X)\n", packet->bandwidth);
            return 0;
        }

        /* Get datarate */
        val = lgw_sf_getval(packet->datarate);
        if (val != -1) {
            SF = (uint8_t)val;
        } else {
            DEBUG_PRINTF("ERROR: Cannot compute time on air for this packet, unsupported datarate (0x%02X)\n", packet->datarate);
            return 0;
        }

        /* Duration of 1 symbol */
        Tsym = pow(2, SF) / BW;

        /* Duration of preamble */
        Tpreamble = (8 + 4.25) * Tsym; /* 8 programmed symbols in preamble */

        /* Duration of payload */
        H = (packet->no_header == false) ? 0 : 1; /* header is always enabled, except for beacons */
        DE = (SF >= 11) ? 1 : 0; /* Low datarate optimization enabled for SF11 and SF12 */

        payloadSymbNb = 8 + (ceil((double)(8 * packet->size - 4 * SF + 28 + 16 - 20 * H) / (double)(4 * (SF - 2 * DE))) * (packet->coderate + 4)); /* Explicitely cast to double to keep precision of the division */

        Tpayload = payloadSymbNb * Tsym;

        /* Duration of packet */
        Tpacket = Tpreamble + Tpayload;
    } else if (packet->modulation == MOD_FSK) {
        /* PREAMBLE + SYNC_WORD + PKT_LEN + PKT_PAYLOAD + CRC
                PREAMBLE: default 5 bytes
                SYNC_WORD: default 3 bytes
                PKT_LEN: 1 byte (variable length mode)
                PKT_PAYLOAD: x bytes
                CRC: 0 or 2 bytes
        */
        Tfsk = (8 * (double)(packet->preamble + fsk_sync_word_size + 1 + packet->size + ((packet->no_crc == true) ? 0 : 2)) / (double)packet->datarate) * 1E3;

        /* Duration of packet */
        Tpacket = (uint32_t)Tfsk + 1; /* add margin for rounding */
    } else {
        Tpacket = 0;
        DEBUG_PRINTF("ERROR: Cannot compute time on air for this packet, unsupported modulation (0x%02X)\n", packet->modulation);
    }

    return Tpacket;
}

/* --- EOF ------------------------------------------------------------------ */
