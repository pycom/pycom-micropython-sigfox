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
#include "esp32_mphal.h"
#include "loragw_hal_esp.h"

#include "loragw_radio_esp.h"
#include "loragw_reg_esp.h"
#include "sx1308.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_HAL == 1
#define DEBUG_MSG(str)                printf(str)
#define DEBUG_PRINTF(fmt, args...)    printf("%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define DEBUG_ARRAY(a,b,c)           for(a=0;a!=0;){}
#define CHECK_NULL(a)                if(a==NULL){return LGW_HAL_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define DEBUG_ARRAY(a,b,c)            for(a=0;a!=0;){}
#define CHECK_NULL(a)                 if(a==NULL){return LGW_HAL_ERROR;}
#endif
#define IF_HZ_TO_REG(f)     (f << 5)/15625
#define SET_PPM_ON(bw,dr)   (((bw == BW_125KHZ) && ((dr == DR_LORA_SF11) || (dr == DR_LORA_SF12))) || ((bw == BW_250KHZ) && (dr == DR_LORA_SF12)))
#define TRACE()

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
const uint8_t esp_ifmod_config[LGW_IF_CHAIN_NB] = LGW_IFMODEM_CONFIG;


/* Version string, used to identify the library version/options once compiled */
const char esp_lgw_version_string[] = "Version: 0.1";// LIBLORAGW_VERSION ";";

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static int32_t iqrxtab[4];

/*
The following static variables are the configuration set that the user can
modify using rxrf_setconf, rxif_setconf and txgain_setconf functions.
The functions _start and _send then use that set to configure the hardware.

Parameters validity and coherency is verified by the _setconf functions and
the _start and _send functions assume they are valid.
*/

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
static bool lorawan_public = true;

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
static int8_t cal_offset_a_i[16]; /* TX I offset for radio A */
static int8_t cal_offset_a_q[16]; /* TX Q offset for radio A */
static int8_t cal_offset_b_i[16]; /* TX I offset for radio B */
static int8_t cal_offset_b_q[16]; /* TX Q offset for radio B */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void lgw_constant_adjust(void);
static int32_t lgw_sf_getval(int x);
static int32_t lgw_bw_getval(int x);
static int reset_firmware(uint8_t target);
static void calibration_save(void);
static void calibration_reload(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

int reset_firmware(uint8_t target) {
    int reg_rst;
    int reg_sel;

    /* check parameters */
    if (target == MCU_ARB) {
        reg_rst = LGW_MCU_RST_0;
        reg_sel = LGW_MCU_SELECT_MUX_0;
    } else if (target == MCU_AGC) {
        reg_rst = LGW_MCU_RST_1;
        reg_sel = LGW_MCU_SELECT_MUX_1;
    } else {
        DEBUG_MSG("ERROR: NOT A VALID TARGET FOR RESETTING FIRMWARE\n");
        return -1;
    }

    /* reset the targeted MCU */
    esp_lgw_reg_w(reg_rst, 1);

    /* set mux to access MCU program RAM and set address to 0 */
    esp_lgw_reg_w(reg_sel, 0);
    esp_lgw_reg_w(LGW_MCU_PROM_ADDR, 0);

    /* give back control of the MCU program ram to the MCU */
    esp_lgw_reg_w(reg_sel, 1);

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
    esp_lgw_reg_w(LGW_RSSI_BB_FILTER_ALPHA, 6); /* default 7 */
    esp_lgw_reg_w(LGW_RSSI_DEC_FILTER_ALPHA, 7); /* default 5 */
    esp_lgw_reg_w(LGW_RSSI_CHANN_FILTER_ALPHA, 7); /* default 8 */
    esp_lgw_reg_w(LGW_RSSI_BB_DEFAULT_VALUE, 23); /* default 32 */
    esp_lgw_reg_w(LGW_RSSI_CHANN_DEFAULT_VALUE, 85); /* default 100 */
    esp_lgw_reg_w(LGW_RSSI_DEC_DEFAULT_VALUE, 66); /* default 100 */
    esp_lgw_reg_w(LGW_DEC_GAIN_OFFSET, 7); /* default 8 */
    esp_lgw_reg_w(LGW_CHAN_GAIN_OFFSET, 6); /* default 7 */

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
    esp_lgw_reg_w(LGW_SNR_AVG_CST, 3); /* default 2 */
    if (lorawan_public) { /* LoRa network */
        esp_lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        esp_lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else { /* private network */
        esp_lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        esp_lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
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
        esp_lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        esp_lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else {
        esp_lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        esp_lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
    }
    // lgw_reg_w(LGW_MBWSSF_ONLY_CRC_EN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_PAYLOAD_FINE_TIMING_GAIN,2); /* default 2 */
    // lgw_reg_w(LGW_MBWSSF_PREAMBLE_FINE_TIMING_GAIN,1); /* default 1 */
    // lgw_reg_w(LGW_MBWSSF_TRACKING_INTEGRAL,0); /* default 0 */
    // lgw_reg_w(LGW_MBWSSF_AGC_FREEZE_ON_DETECT,1); /* default 1 */
    esp_lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_RDX4, 1); /* default 0 */
    esp_lgw_reg_w(LGW_ADJUST_MODEM_START_OFFSET_SF12_RDX4, 4094); /* default 4092 */
    esp_lgw_reg_w(LGW_CORR_MAC_GAIN, 7); /* default 5 */



    /* FSK datapath setup */
    esp_lgw_reg_w(LGW_FSK_RX_INVERT, 1); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_MODEM_INVERT_IQ, 1); /* default 0 */

    /* FSK demodulator setup */
    esp_lgw_reg_w(LGW_FSK_RSSI_LENGTH, 4); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_PKT_MODE, 1); /* variable length, default 0 */
    esp_lgw_reg_w(LGW_FSK_CRC_EN, 1); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_DCFREE_ENC, 2); /* default 0 */
    // lgw_reg_w(LGW_FSK_CRC_IBM,0); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_ERROR_OSR_TOL, 10); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_PKT_LENGTH, 255); /* max packet length in variable length mode */
    // lgw_reg_w(LGW_FSK_NODE_ADRS,0); /* default 0 */
    // lgw_reg_w(LGW_FSK_BROADCAST,0); /* default 0 */
    // lgw_reg_w(LGW_FSK_AUTO_AFC_ON,0); /* default 0 */
    esp_lgw_reg_w(LGW_FSK_PATTERN_TIMEOUT_CFG, 128); /* sync timeout (allow 8 bytes preamble + 8 bytes sync word, default 0 */

    /* TX general parameters */
    esp_lgw_reg_w(LGW_TX_START_DELAY, TX_START_DELAY); /* default 0 */

    /* TX LoRa */
    // lgw_reg_w(LGW_TX_MODE,0); /* default 0 */
    esp_lgw_reg_w(LGW_TX_SWAP_IQ, 1); /* "normal" polarity; default 0 */
    if (lorawan_public) { /* LoRa network */
        esp_lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK1_POS, 3); /* default 1 */
        esp_lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK2_POS, 4); /* default 2 */
    } else { /* Private network */
        esp_lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK1_POS, 1); /* default 1 */
        esp_lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK2_POS, 2); /* default 2 */
    }

    /* TX FSK */
    // lgw_reg_w(LGW_FSK_TX_GAUSSIAN_EN,1); /* default 1 */
    esp_lgw_reg_w(LGW_FSK_TX_GAUSSIAN_SELECT_BT, 2); /* Gaussian filter always on TX, default 0 */
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
        case BW_7K8HZ:
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

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int esp_lgw_board_setconf(struct lgw_conf_board_s *conf) {

    /* set internal config according to parameters */
    lorawan_public = conf->lorawan_public;

    DEBUG_PRINTF("Note: board configuration; lorawan_public:%d\n", lorawan_public);

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_rxrf_setconf(uint8_t rf_chain, struct lgw_conf_rxrf_s *conf) {

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

    // DEBUG_PRINTF("Note: rf_chain %d configuration; en:%d freq:%d rssi_offset:%f radio_type:%d tx_enable:%d tx_notch_freq:%u\n", rf_chain, rf_enable[rf_chain], rf_rx_freq[rf_chain], rf_rssi_offset[rf_chain], rf_radio_type[rf_chain], rf_tx_enable[rf_chain]);

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_rxif_setconf(uint8_t if_chain, struct lgw_conf_rxif_s *conf) {
    int32_t bw_hz;
    uint32_t rf_rx_bandwidth;

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
    if (esp_ifmod_config[if_chain] == IF_UNDEFINED) {
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
    switch (esp_ifmod_config[if_chain]) {
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
            if (!IS_FSK_BW(conf->bandwidth)) {
                DEBUG_MSG("ERROR: BANDWIDTH NOT SUPPORTED BY FSK IF CHAIN\n");
                return LGW_HAL_ERROR;
            }
            if (!IS_FSK_DR(conf->datarate)) {
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
            DEBUG_PRINTF("Note: FSK if_chain %d configuration; en:%d freq:%d bw:%d dr:%d (%d real dr) sync:0x%0*llX\n", if_chain, if_enable[if_chain], if_freq[if_chain], fsk_rx_bw, fsk_rx_dr, LGW_XTAL_FREQU / (LGW_XTAL_FREQU / fsk_rx_dr), 2 * fsk_sync_word_size, fsk_sync_word);
            break;

        default:
            DEBUG_PRINTF("ERROR: IF CHAIN %d TYPE NOT SUPPORTED\n", if_chain);
            return LGW_HAL_ERROR;
    }

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_txgain_setconf(struct lgw_tx_gain_lut_s *conf) {
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
            DEBUG_MSG("ERROR: TX gain LUT: SX1308 digital gain must be between 0 and 3\n");
            return LGW_HAL_ERROR;
        }
        if (conf->lut[i].dac_gain != 3) {
            DEBUG_MSG("ERROR: TX gain LUT: SX1257 DAC gains != 3 are not supported\n");
            return LGW_HAL_ERROR;
        }
        if (conf->lut[i].mix_gain > 15) {
            DEBUG_MSG("WARNING: TX gain LUT: SX1257 mixer gain must not exceed 15\n");
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
        txgain_lut.lut[i].pa_gain = conf->lut[i].pa_gain;
        txgain_lut.lut[i].rf_power = conf->lut[i].rf_power;
    }

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_start(void) {
    int i;
    uint32_t x;
    uint8_t radio_select;
    int32_t read_val;
    uint8_t load_val;
    uint8_t fw_version;
    uint64_t fsk_sync_word_reg;
    int DELAYSTART = 1;

    /* Enable clocks */
    esp_lgw_reg_w(LGW_GLOBAL_EN, 1);
    esp_lgw_reg_w(LGW_CLK32M_EN, 1);

    /* Compute counter offset to be applied to SX1308 internal counter on receive and send */
    SX1308.offtmstp = sx1308_timer_read_us() - SX1308.offtmstpref;

    /* Set all GPIOs as output RXON/TXON*/
    esp_lgw_reg_w(LGW_GPIO_MODE, 31);
    esp_lgw_reg_w(LGW_GPIO_SELECT_OUTPUT, 0);

    /* select calibration command */
    calibration_reload();

    /* load adjusted parameters */
    lgw_constant_adjust();

    /* Sanity check for RX frequency */
    if (rf_rx_freq[0] == 0) {
        DEBUG_MSG("ERROR: wrong configuration, rf_rx_freq[0] is not set\n");
        return LGW_HAL_ERROR;
    }

    /* Freq-to-time-drift calculation */
    //float ftemp=(409600 / (rf_rx_freq[0] >> 1))*10000;
    float ftemp = (4096 * 2) / (rf_rx_freq[0] / 1000000);
    x = (uint32_t)ftemp; /* dividend: (4*2048*1000000) >> 1, rescaled to avoid 32b overflow */
    x = (x > 63) ? 63 : x; /* saturation */
    esp_lgw_reg_w(LGW_FREQ_TO_TIME_DRIFT, x); /* default 9 */

    //ftemp=(409600 / (rf_rx_freq[0] >> 3))*10000; /* dividend: (16*2048*1000000) >> 3, rescaled to avoid 32b overflow */
    ftemp = (4096 * 8) / (rf_rx_freq[0] / 1000000);
    x = (uint32_t)ftemp;
    x = (x > 63) ? 63 : x; /* saturation */
    esp_lgw_reg_w(LGW_MBWSSF_FREQ_TO_TIME_DRIFT, x); /* default 36 */

    /* configure LoRa 'multi' demodulators aka. LoRa 'sensor' channels (IF0-3) */
    radio_select = 0; /* IF mapping to radio A/B (per bit, 0=A, 1=B) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        radio_select += (if_rf_chain[i] == 1 ? 1 << i : 0); /* transform bool array into binary word */
    }
    /*
    lgw_reg_w(LGW_RADIO_SELECT, radio_select);
    LGW_RADIO_SELECT is used for communication with the firmware, "radio_select"
    will be loaded in LGW_RADIO_SELECT at the end of start procedure.
    */

    esp_lgw_reg_w(LGW_IF_FREQ_0, IF_HZ_TO_REG(if_freq[0])); /* default -384 */
    esp_lgw_reg_w(LGW_IF_FREQ_1, IF_HZ_TO_REG(if_freq[1])); /* default -128 */
    esp_lgw_reg_w(LGW_IF_FREQ_2, IF_HZ_TO_REG(if_freq[2])); /* default 128 */
    esp_lgw_reg_w(LGW_IF_FREQ_3, IF_HZ_TO_REG(if_freq[3])); /* default 384 */
    esp_lgw_reg_w(LGW_IF_FREQ_4, IF_HZ_TO_REG(if_freq[4])); /* default -384 */
    esp_lgw_reg_w(LGW_IF_FREQ_5, IF_HZ_TO_REG(if_freq[5])); /* default -128 */
    esp_lgw_reg_w(LGW_IF_FREQ_6, IF_HZ_TO_REG(if_freq[6])); /* default 128 */
    esp_lgw_reg_w(LGW_IF_FREQ_7, IF_HZ_TO_REG(if_freq[7])); /* default 384 */
    esp_lgw_reg_w(LGW_CORR0_DETECT_EN, (if_enable[0] == true) ? lora_multi_sfmask[0] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR1_DETECT_EN, (if_enable[1] == true) ? lora_multi_sfmask[1] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR2_DETECT_EN, (if_enable[2] == true) ? lora_multi_sfmask[2] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR3_DETECT_EN, (if_enable[3] == true) ? lora_multi_sfmask[3] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR4_DETECT_EN, (if_enable[4] == true) ? lora_multi_sfmask[4] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR5_DETECT_EN, (if_enable[5] == true) ? lora_multi_sfmask[5] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR6_DETECT_EN, (if_enable[6] == true) ? lora_multi_sfmask[6] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_CORR7_DETECT_EN, (if_enable[7] == true) ? lora_multi_sfmask[7] : 0); /* default 0 */
    esp_lgw_reg_w(LGW_PPM_OFFSET, 0x60); /* as the threshold is 16ms, use 0x60 to enable ppm_offset for SF12 and SF11 @125kHz*/
    esp_lgw_reg_w(LGW_CONCENTRATOR_MODEM_ENABLE, 1); /* default 0 */

    /* configure LoRa 'stand-alone' modem (IF8) */
    esp_lgw_reg_w(LGW_IF_FREQ_8, IF_HZ_TO_REG(if_freq[8])); /* MBWSSF modem (default 0) */
    if (if_enable[8] == true) {
        esp_lgw_reg_w(LGW_MBWSSF_RADIO_SELECT, if_rf_chain[8]);
        switch (lora_rx_bw) {
            case BW_125KHZ:
                esp_lgw_reg_w(LGW_MBWSSF_MODEM_BW, 0);
                break;
            case BW_250KHZ:
                esp_lgw_reg_w(LGW_MBWSSF_MODEM_BW, 1);
                break;
            case BW_500KHZ:
                esp_lgw_reg_w(LGW_MBWSSF_MODEM_BW, 2);
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", lora_rx_bw);
                return LGW_HAL_ERROR;
        }
        switch (lora_rx_sf) {
            case DR_LORA_SF7:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 7);
                break;
            case DR_LORA_SF8:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 8);
                break;
            case DR_LORA_SF9:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 9);
                break;
            case DR_LORA_SF10:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 10);
                break;
            case DR_LORA_SF11:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 11);
                break;
            case DR_LORA_SF12:
                esp_lgw_reg_w(LGW_MBWSSF_RATE_SF, 12);
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", lora_rx_sf);
                return LGW_HAL_ERROR;
        }
        esp_lgw_reg_w(LGW_MBWSSF_PPM_OFFSET, lora_rx_ppm_offset); /* default 0 */
        esp_lgw_reg_w(LGW_MBWSSF_MODEM_ENABLE, 1); /* default 0 */
    } else {
        esp_lgw_reg_w(LGW_MBWSSF_MODEM_ENABLE, 0);
    }

    /* configure FSK modem (IF9) */
    esp_lgw_reg_w(LGW_IF_FREQ_9, IF_HZ_TO_REG(if_freq[9])); /* FSK modem, default 0 */
    esp_lgw_reg_w(LGW_FSK_PSIZE, fsk_sync_word_size - 1);
    esp_lgw_reg_w(LGW_FSK_TX_PSIZE, fsk_sync_word_size - 1);
    fsk_sync_word_reg = fsk_sync_word << (8 * (8 - fsk_sync_word_size));
    esp_lgw_reg_w(LGW_FSK_REF_PATTERN_LSB, (uint32_t)(0xFFFFFFFF & fsk_sync_word_reg));
    esp_lgw_reg_w(LGW_FSK_REF_PATTERN_MSB, (uint32_t)(0xFFFFFFFF & (fsk_sync_word_reg >> 32)));
    if (if_enable[9] == true) {
        esp_lgw_reg_w(LGW_FSK_RADIO_SELECT, if_rf_chain[9]);
        esp_lgw_reg_w(LGW_FSK_BR_RATIO, LGW_XTAL_FREQU / fsk_rx_dr); /* setting the dividing ratio for datarate */
        esp_lgw_reg_w(LGW_FSK_CH_BW_EXPO, fsk_rx_bw);
        esp_lgw_reg_w(LGW_FSK_MODEM_ENABLE, 1); /* default 0 */
    } else {
        esp_lgw_reg_w(LGW_FSK_MODEM_ENABLE, 0);
    }

    /* Load firmware */
    reset_firmware(MCU_ARB);
    reset_firmware(MCU_AGC);
    esp_lgw_reg_w(LGW_MCU_RST_0, 1);
    esp_lgw_reg_w(LGW_MCU_RST_1, 1);

    /* gives the AGC MCU control over radio, RF front-end and filter gain */
    esp_lgw_reg_w(LGW_FORCE_HOST_RADIO_CTRL, 0);
    esp_lgw_reg_w(LGW_FORCE_HOST_FE_CTRL, 0);
    esp_lgw_reg_w(LGW_FORCE_DEC_FILTER_GAIN, 0);

    /* Get MCUs out of reset */
    esp_lgw_reg_w(LGW_RADIO_SELECT, 0); /* MUST not be = to 1 or 2 at firmware init */
    esp_lgw_reg_w(LGW_MCU_RST_0, 0);
    esp_lgw_reg_w(LGW_MCU_RST_1, 0);

    /* Check firmware version */
    esp_lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, FW_VERSION_ADDR);
    esp_lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
    fw_version = (uint8_t)read_val;
    if (fw_version != FW_VERSION_AGC) {
        DEBUG_PRINTF("ERROR: Version of AGC firmware not expected, actual:%d expected:%d\n", fw_version, FW_VERSION_AGC);
        return LGW_HAL_ERROR;
    }
    esp_lgw_reg_w(LGW_DBG_ARB_MCU_RAM_ADDR, FW_VERSION_ADDR);
    esp_lgw_reg_r(LGW_DBG_ARB_MCU_RAM_DATA, &read_val);
    fw_version = (uint8_t)read_val;
    if (fw_version != FW_VERSION_ARB) {
        DEBUG_PRINTF("ERROR: Version of arbiter firmware not expected, actual:%d expected:%d\n", fw_version, FW_VERSION_ARB);
        return LGW_HAL_ERROR;
    }

    DEBUG_MSG("Info: Initialising AGC firmware...\n");
    ets_delay_us(100); /* Need to wait for long enough here */
    esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x10) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS1 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* Update Tx gain LUT and start AGC */
    for (i = 0; i < txgain_lut.size; ++i) {
        esp_lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT); /* start a transaction */
        ets_delay_us(DELAYSTART);
        load_val = txgain_lut.lut[i].mix_gain + (16 * txgain_lut.lut[i].dac_gain) + (64 * txgain_lut.lut[i].pa_gain);
        esp_lgw_reg_w(LGW_RADIO_SELECT, load_val);
        ets_delay_us(DELAYSTART);
        esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
        if (read_val != (0x30 + i)) {
            DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS2 0x%02X\n", (uint8_t)read_val);
            return LGW_HAL_ERROR;
        }
    }

    /* As the AGC fw is waiting for 16 entries, we need to abort the transaction if we get less entries */
    if (txgain_lut.size < TX_GAIN_LUT_SIZE_MAX) {
        esp_lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
        ets_delay_us(DELAYSTART);
        load_val = AGC_CMD_ABORT;
        esp_lgw_reg_w(LGW_RADIO_SELECT, load_val);
        ets_delay_us(DELAYSTART);
        esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
        if (read_val != 0x30) {
            DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS3 0x%02X\n", (uint8_t)read_val);
            return LGW_HAL_ERROR;
        }
    }

    /* Load Tx freq MSBs (always 3 if f > 768 for SX1257 or f > 384 for SX1255 */
    esp_lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_w(LGW_RADIO_SELECT, 3);
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x33) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS4 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* Load chan_select firmware option */
    esp_lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_w(LGW_RADIO_SELECT, 0);
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x30) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS5 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* End AGC firmware init and check status */
    esp_lgw_reg_w(LGW_RADIO_SELECT, AGC_CMD_WAIT);
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_w(LGW_RADIO_SELECT, radio_select); /* Load intended value of RADIO_SELECT */
    ets_delay_us(DELAYSTART);
    esp_lgw_reg_r(LGW_MCU_AGC_STATUS, &read_val);
    if (read_val != 0x40) {
        DEBUG_PRINTF("ERROR: AGC FIRMWARE INITIALIZATION FAILURE, STATUS 0x%02X\n", (uint8_t)read_val);
        return LGW_HAL_ERROR;
    }

    /* do not enable GPS event capture */
    esp_lgw_reg_w(LGW_GPS_EN, 0);

    DEBUG_MSG("Info: concentrator restarted...\n");

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data) {
    int nb_pkt_fetch; /* loop variable and return value */
    struct lgw_pkt_rx_s *p; /* pointer to the current structure in the struct array */
    uint8_t buff[255 + RX_METADATA_NB]; /* buffer to store the result of SPI read bursts */
    unsigned sz; /* size of the payload, uses to address metadata */
    int ifmod; /* type of if_chain/modem a packet was received by */
    int stat_fifo; /* the packet status as indicated in the FIFO */
    uint32_t raw_timestamp; /* timestamp when internal 'RX finished' was triggered */
    uint32_t delay_x, delay_y, delay_z; /* temporary variable for timestamp offset calculation */
    uint32_t timestamp_correction; /* correction to account for processing delay */
    uint32_t sf, cr, bw_pow, crc_en, ppm; /* used to calculate timestamp correction */

    /* check input variables */
    if ((max_pkt == 0) || (max_pkt > LGW_PKT_FIFO_SIZE)) {
        DEBUG_PRINTF("ERROR: %d = INVALID MAX NUMBER OF PACKETS TO FETCH\n", max_pkt);
        return LGW_HAL_ERROR;
    }
    CHECK_NULL(pkt_data);

    /* Initialize buffer */
    memset(buff, 0, sizeof buff);

    /* iterate max_pkt times at most */
    for (nb_pkt_fetch = 0; nb_pkt_fetch < max_pkt; ++nb_pkt_fetch) {

        /* point to the proper struct in the struct array */
        p = &pkt_data[nb_pkt_fetch];

        /* fetch all the RX FIFO data */
        esp_lgw_reg_rb(LGW_RX_PACKET_DATA_FIFO_NUM_STORED, buff, 5);
        /* 0:   number of packets available in RX data buffer */
        /* 1,2: start address of the current packet in RX data buffer */
        /* 3:   CRC status of the current packet */
        /* 4:   size of the current packet payload in byte */

        /* how many packets are in the RX buffer ? Break if zero */
        if (buff[0] == 0) {
            break; /* no more packets to fetch, exit out of FOR loop */
        }

        /* sanity check */
        if (buff[0] > LGW_PKT_FIFO_SIZE) {
            DEBUG_PRINTF("WARNING: %u = INVALID NUMBER OF PACKETS TO FETCH, ABORTING\n", buff[0]);
            break;
        }

        DEBUG_PRINTF("FIFO content: %x %x %x %x %x\n", buff[0], buff[1], buff[2], buff[3], buff[4]);

        p->size = buff[4];
        sz = p->size;
        stat_fifo = buff[3]; /* will be used later, need to save it before overwriting buff */

        /* get payload + metadata */
        esp_lgw_reg_rb(LGW_RX_DATA_BUF_DATA, buff, sz + RX_METADATA_NB);

        /* copy payload to result struct */
        memcpy((void *)p->payload, (void *)buff, sz);

        /* process metadata */
        p->if_chain = buff[sz + 0];
        if (p->if_chain >= LGW_IF_CHAIN_NB) {
            DEBUG_PRINTF("WARNING: %u NOT A VALID IF_CHAIN NUMBER, ABORTING\n", p->if_chain);
            break;
        }
        ifmod = esp_ifmod_config[p->if_chain];
        DEBUG_PRINTF("[%d %d]\n", p->if_chain, ifmod);

        p->rf_chain = (uint8_t)if_rf_chain[p->if_chain];
        p->freq_hz = (uint32_t)((int32_t)rf_rx_freq[p->rf_chain] + if_freq[p->if_chain]);
        p->rssi = (float)buff[sz + 5] + rf_rssi_offset[p->rf_chain];

        if ((ifmod == IF_LORA_MULTI) || (ifmod == IF_LORA_STD)) {
            DEBUG_MSG("Note: LoRa packet\n");
            switch (stat_fifo & 0x07) {
                case 5:
                    p->status = STAT_CRC_OK;
                    crc_en = 1;
                    break;
                case 7:
                    p->status = STAT_CRC_BAD;
                    crc_en = 1;
                    break;
                case 1:
                    p->status = STAT_NO_CRC;
                    crc_en = 0;
                    break;
                default:
                    p->status = STAT_UNDEFINED;
                    crc_en = 0;
            }
            p->modulation = MOD_LORA;
            p->snr = ((float)((int8_t)buff[sz + 2])) / 4;
            p->snr_min = ((float)((int8_t)buff[sz + 3])) / 4;
            p->snr_max = ((float)((int8_t)buff[sz + 4])) / 4;
            if (ifmod == IF_LORA_MULTI) {
                p->bandwidth = BW_125KHZ; /* fixed in hardware */
            } else {
                p->bandwidth = lora_rx_bw; /* get the parameter from the config variable */
            }
            sf = (buff[sz + 1] >> 4) & 0x0F;

            switch (sf) {
                case 7:
                    p->datarate = DR_LORA_SF7;
                    break;
                case 8:
                    p->datarate = DR_LORA_SF8;
                    break;
                case 9:
                    p->datarate = DR_LORA_SF9;
                    break;
                case 10:
                    p->datarate = DR_LORA_SF10;
                    break;
                case 11:
                    p->datarate = DR_LORA_SF11;
                    break;
                case 12:
                    p->datarate = DR_LORA_SF12;
                    break;
                default:
                    p->datarate = DR_UNDEFINED;
            }
            cr = (buff[sz + 1] >> 1) & 0x07;
            switch (cr) {
                case 1:
                    p->coderate = CR_LORA_4_5;
                    break;
                case 2:
                    p->coderate = CR_LORA_4_6;
                    break;
                case 3:
                    p->coderate = CR_LORA_4_7;
                    break;
                case 4:
                    p->coderate = CR_LORA_4_8;
                    break;
                default:
                    p->coderate = CR_UNDEFINED;
            }

            /* determine if 'PPM mode' is on, needed for timestamp correction */
            if (SET_PPM_ON(p->bandwidth, p->datarate)) {
                ppm = 1;
            } else {
                ppm = 0;
            }

            /* timestamp correction code, base delay */
            if (ifmod == IF_LORA_STD) { /* if packet was received on the stand-alone LoRa modem */
                switch (lora_rx_bw) {
                    case BW_125KHZ:
                        delay_x = 64;
                        bw_pow = 1;
                        break;
                    case BW_250KHZ:
                        delay_x = 32;
                        bw_pow = 2;
                        break;
                    case BW_500KHZ:
                        delay_x = 16;
                        bw_pow = 4;
                        break;
                    default:
                        DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", p->bandwidth);
                        delay_x = 0;
                        bw_pow = 0;
                }
            } else { /* packet was received on one of the sensor channels = 125kHz */
                delay_x = 114;
                bw_pow = 1;
            }

            /* timestamp correction code, variable delay */
            if ((sf >= 6) && (sf <= 12) && (bw_pow > 0)) {
                if ((2 * (sz + 2 * crc_en) - (sf - 7)) <= 0) { /* payload fits entirely in first 8 symbols */
                    delay_y = (((1 << (sf - 1)) * (sf + 1)) + (3 * (1 << (sf - 4)))) / bw_pow;
                    delay_z = 32 * (2 * (sz + 2 * crc_en) + 5) / bw_pow;
                } else {
                    delay_y = (((1 << (sf - 1)) * (sf + 1)) + ((4 - ppm) * (1 << (sf - 4)))) / bw_pow;
                    delay_z = (16 + 4 * cr) * (((2 * (sz + 2 * crc_en) - sf + 6) % (sf - 2 * ppm)) + 1) / bw_pow;
                }
                timestamp_correction = delay_x + delay_y + delay_z;
            } else {
                timestamp_correction = 0;
                DEBUG_MSG("WARNING: invalid packet, no timestamp correction\n");
            }

            /* RSSI correction */
            if (ifmod == IF_LORA_MULTI) {
                p->rssi -= RSSI_MULTI_BIAS;
            }

        } else if (ifmod == IF_FSK_STD) {
            DEBUG_MSG("Note: FSK packet\n");
            switch (stat_fifo & 0x07) {
                case 5:
                    p->status = STAT_CRC_OK;
                    break;
                case 7:
                    p->status = STAT_CRC_BAD;
                    break;
                case 1:
                    p->status = STAT_NO_CRC;
                    break;
                default:
                    p->status = STAT_UNDEFINED;
                    break;
            }
            p->modulation = MOD_FSK;
            p->snr = -128.0;
            p->snr_min = -128.0;
            p->snr_max = -128.0;
            p->bandwidth = fsk_rx_bw;
            p->datarate = fsk_rx_dr;
            p->coderate = CR_UNDEFINED;
            timestamp_correction = ((uint32_t)680000 / fsk_rx_dr) - 20;

            /* RSSI correction */
            p->rssi = (RSSI_FSK_POLY_0) + ((float)(RSSI_FSK_POLY_1) * p->rssi) + ((float)(RSSI_FSK_POLY_2) * (p->rssi) * (p->rssi));
        } else {
            DEBUG_MSG("ERROR: UNEXPECTED PACKET ORIGIN\n");
            p->status = STAT_UNDEFINED;
            p->modulation = MOD_UNDEFINED;
            p->rssi = -128.0;
            p->snr = -128.0;
            p->snr_min = -128.0;
            p->snr_max = -128.0;
            p->bandwidth = BW_UNDEFINED;
            p->datarate = DR_UNDEFINED;
            p->coderate = CR_UNDEFINED;
            timestamp_correction = 0;
        }

        raw_timestamp = (uint32_t)buff[sz + 6] + ((uint32_t)buff[sz + 7] << 8) + ((uint32_t)buff[sz + 8] << 16) + ((uint32_t)buff[sz + 9] << 24);
        p->count_us = raw_timestamp - timestamp_correction + SX1308.offtmstp; /* corrected with PicoCell offset */
        p->crc = (uint16_t)buff[sz + 10] + ((uint16_t)buff[sz + 11] << 8);

        /* advance packet FIFO */
        esp_lgw_reg_w(LGW_RX_PACKET_DATA_FIFO_NUM_STORED, 0);
    }

    return nb_pkt_fetch;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_send(struct lgw_pkt_tx_s *pkt_data) {
    int i;
    uint8_t buff[256 + TX_METADATA_NB]; /* buffer to prepare the packet to send + metadata before SPI write burst */
    uint32_t part_int = 0; /* integer part for PLL register value calculation */
    uint32_t part_frac = 0; /* fractional part for PLL register value calculation */
    uint16_t fsk_dr_div; /* divider to configure for target datarate */
    int transfer_size = 0; /* data to transfer from host to TX databuffer */
    int payload_offset = 0; /* start of the payload content in the databuffer */
    uint8_t pow_index = 0; /* 4-bit value to set the firmware TX power */
    uint8_t target_mix_gain = 0; /* used to select the proper I/Q offset correction */
    uint32_t count_trig = 0; /* timestamp value in trigger mode corrected for TX start delay */

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

    /* interpretation of TX power */
    for (pow_index = txgain_lut.size - 1; pow_index > 0; pow_index--) {
        if (txgain_lut.lut[pow_index].rf_power <= pkt_data->rf_power) {
            break;
        }
    }

    /* Save radio calibration for next restart */
    calibration_save();
    
    /* loading TX imbalance correction */
    target_mix_gain = txgain_lut.lut[pow_index].mix_gain;
    if (pkt_data->rf_chain == 0) { /* use radio A calibration table */
        esp_lgw_reg_w(LGW_TX_OFFSET_I, cal_offset_a_i[target_mix_gain ]);
        esp_lgw_reg_w(LGW_TX_OFFSET_Q, cal_offset_a_q[target_mix_gain ]);
    } else { /* use radio B calibration table */
        esp_lgw_reg_w(LGW_TX_OFFSET_I, cal_offset_b_i[target_mix_gain ]);
        esp_lgw_reg_w(LGW_TX_OFFSET_Q, cal_offset_b_q[target_mix_gain ]);
    }

    /* Set digital gain from LUT */
    esp_lgw_reg_w(LGW_TX_GAIN, txgain_lut.lut[pow_index].dig_gain);

    /* fixed metadata, useful payload and misc metadata compositing */
    transfer_size = TX_METADATA_NB + pkt_data->size; /*  */
    payload_offset = TX_METADATA_NB; /* start the payload just after the metadata */

    /* metadata 0 to 2, TX PLL frequency */
    switch (rf_radio_type[0]) { /* we assume that there is only one radio type on the board */
        case LGW_RADIO_TYPE_SX1255:
            part_int = pkt_data->freq_hz / (SX125x_32MHz_FRAC << 7); /* integer part, gives the MSB */
            part_frac = ((pkt_data->freq_hz % (SX125x_32MHz_FRAC << 7)) << 9) / SX125x_32MHz_FRAC; /* fractional part, gives middle part and LSB */
            break;
        case LGW_RADIO_TYPE_SX1257:
            part_int = pkt_data->freq_hz / (SX125x_32MHz_FRAC << 8); /* integer part, gives the MSB */
            part_frac = ((pkt_data->freq_hz % (SX125x_32MHz_FRAC << 8)) << 8) / SX125x_32MHz_FRAC; /* fractional part, gives middle part and LSB */
            break;
        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d FOR RADIO TYPE\n", rf_radio_type[0]);
            break;
    }

    buff[0] = 0xFF & part_int; /* Most Significant Byte */
    buff[1] = 0xFF & (part_frac >> 8); /* middle byte */
    buff[2] = 0xFF & part_frac; /* Least Significant Byte */

    /* metadata 3 to 6, timestamp trigger value */
    /* TX state machine must be triggered at T0 - TX_START_DELAY for packet to start being emitted at T0 */
    if (pkt_data->tx_mode == TIMESTAMPED) {
        count_trig = pkt_data->count_us - TX_START_DELAY - SX1308.offtmstp; /* Corrected with PicoCell offset */
        buff[3] = 0xFF & (count_trig >> 24);
        buff[4] = 0xFF & (count_trig >> 16);
        buff[5] = 0xFF & (count_trig >> 8);
        buff[6] = 0xFF & count_trig;
    }

    /* parameters depending on modulation  */
    if (pkt_data->modulation == MOD_LORA) {
        /* metadata 7, modulation type, radio chain selection and TX power */
        buff[7] = (0x20 & (pkt_data->rf_chain << 5)) | (0x0F & pow_index); /* bit 4 is 0 -> LoRa modulation */

        buff[8] = 0; /* metadata 8, not used */

        /* metadata 9, CRC, LoRa CR & SF */
        switch (pkt_data->datarate) {
            case DR_LORA_SF7:
                buff[9] = 7;
                break;
            case DR_LORA_SF8:
                buff[9] = 8;
                break;
            case DR_LORA_SF9:
                buff[9] = 9;
                break;
            case DR_LORA_SF10:
                buff[9] = 10;
                break;
            case DR_LORA_SF11:
                buff[9] = 11;
                break;
            case DR_LORA_SF12:
                buff[9] = 12;
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", pkt_data->datarate);
        }
        switch (pkt_data->coderate) {
            case CR_LORA_4_5:
                buff[9] |= 1 << 4;
                break;
            case CR_LORA_4_6:
                buff[9] |= 2 << 4;
                break;
            case CR_LORA_4_7:
                buff[9] |= 3 << 4;
                break;
            case CR_LORA_4_8:
                buff[9] |= 4 << 4;
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", pkt_data->coderate);
        }
        if (pkt_data->no_crc == false) {
            buff[9] |= 0x80; /* set 'CRC enable' bit */
        } else {
            DEBUG_MSG("Info: packet will be sent without CRC\n");
        }

        /* metadata 10, payload size */
        buff[10] = pkt_data->size;

        /* metadata 11, implicit header, modulation bandwidth, PPM offset & polarity */
        switch (pkt_data->bandwidth) {
            case BW_125KHZ:
                buff[11] = 0;
                break;
            case BW_250KHZ:
                buff[11] = 1;
                break;
            case BW_500KHZ:
                buff[11] = 2;
                break;
            default:
                DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", pkt_data->bandwidth);
        }
        if (pkt_data->no_header == true) {
            buff[11] |= 0x04; /* set 'implicit header' bit */
        }
        if (SET_PPM_ON(pkt_data->bandwidth, pkt_data->datarate)) {
            buff[11] |= 0x08; /* set 'PPM offset' bit at 1 */
        }
        if (pkt_data->invert_pol == true) {
            buff[11] |= 0x10; /* set 'TX polarity' bit at 1 */
        }

        /* metadata 12 & 13, LoRa preamble size */
        if (pkt_data->preamble == 0) { /* if not explicit, use recommended LoRa preamble size */
            pkt_data->preamble = STD_LORA_PREAMBLE;
        } else if (pkt_data->preamble < MIN_LORA_PREAMBLE) { /* enforce minimum preamble size */
            pkt_data->preamble = MIN_LORA_PREAMBLE;
            DEBUG_MSG("Note: preamble length adjusted to respect minimum LoRa preamble size\n");
        }
        buff[12] = 0xFF & (pkt_data->preamble >> 8);
        buff[13] = 0xFF & pkt_data->preamble;

        /* metadata 14 & 15, not used */
        buff[14] = 0;
        buff[15] = 0;

        /* MSB of RF frequency is now used in AGC firmware to implement large/narrow filtering in SX1257/55 */
        buff[0] &= 0x3F; /* Unset 2 MSBs of frequency code */
        if (pkt_data->bandwidth == BW_500KHZ) {
            buff[0] |= 0x80; /* Set MSB bit to enlarge analog filter for 500kHz BW */
        } else if (pkt_data->bandwidth == BW_125KHZ) {
            buff[0] |= 0x40; /* Set MSB-1 bit to enable digital filter for 125kHz BW */
        }

    } else if (pkt_data->modulation == MOD_FSK) {
        /* metadata 7, modulation type, radio chain selection and TX power */
        buff[7] = (0x20 & (pkt_data->rf_chain << 5)) | 0x10 | (0x0F & pow_index); /* bit 4 is 1 -> FSK modulation */

        buff[8] = 0; /* metadata 8, not used */

        /* metadata 9, frequency deviation */
        buff[9] = pkt_data->f_dev;

        /* metadata 10, payload size */
        buff[10] = pkt_data->size;
        /* TODO: how to handle 255 bytes packets ?!? */

        /* metadata 11, packet mode, CRC, encoding */
        buff[11] = 0x01 | (pkt_data->no_crc ? 0 : 0x02) | (0x02 << 2); /* always in variable length packet mode, whitening, and CCITT CRC if CRC is not disabled  */

        /* metadata 12 & 13, FSK preamble size */
        if (pkt_data->preamble == 0) { /* if not explicit, use LoRa MAC preamble size */
            pkt_data->preamble = STD_FSK_PREAMBLE;
        } else if (pkt_data->preamble < MIN_FSK_PREAMBLE) { /* enforce minimum preamble size */
            pkt_data->preamble = MIN_FSK_PREAMBLE;
            DEBUG_MSG("Note: preamble length adjusted to respect minimum FSK preamble size\n");
        }
        buff[12] = 0xFF & (pkt_data->preamble >> 8);
        buff[13] = 0xFF & pkt_data->preamble;

        /* metadata 14 & 15, FSK baudrate */
        fsk_dr_div = (uint16_t)((uint32_t)LGW_XTAL_FREQU / pkt_data->datarate); /* Ok for datarate between 500bps and 250kbps */
        buff[14] = 0xFF & (fsk_dr_div >> 8);
        buff[15] = 0xFF & fsk_dr_div;

        /* insert payload size in the packet for variable mode */
        buff[16] = pkt_data->size;
        ++transfer_size; /* one more byte to transfer to the TX modem */
        ++payload_offset; /* start the payload with one more byte of offset */

        /* MSB of RF frequency is now used in AGC firmware to implement large/narrow filtering in SX1257/55 */
        buff[0] &= 0x7F; /* Always use narrow band for FSK (force MSB to 0) */

    } else {
        DEBUG_MSG("ERROR: INVALID TX MODULATION..\n");
        return LGW_HAL_ERROR;
    }

    /* copy payload from user struct to buffer containing metadata */
    memcpy((void *)(buff + payload_offset), (void *)(pkt_data->payload), pkt_data->size);

    /* reset TX command flags */
    esp_lgw_abort_tx();

    /* put metadata + payload in the TX data buffer */
    esp_lgw_reg_w(LGW_TX_DATA_BUF_ADDR, 0);
    esp_lgw_reg_wb(LGW_TX_DATA_BUF_DATA, buff, transfer_size);
    DEBUG_ARRAY(i, transfer_size, buff);

    switch (pkt_data->tx_mode) {
        case IMMEDIATE:
            esp_lgw_reg_w(LGW_TX_TRIG_IMMEDIATE, 1);
            break;

        case TIMESTAMPED:
            esp_lgw_reg_w(LGW_TX_TRIG_DELAYED, 1);
            break;

        case ON_GPS:
            esp_lgw_reg_w(LGW_TX_TRIG_GPS, 1);
            break;

        default:
            DEBUG_PRINTF("ERROR: UNEXPECTED VALUE %d IN SWITCH STATEMENT\n", pkt_data->tx_mode);
            return LGW_HAL_ERROR;
    }

    DEBUG_MSG("Note: lgw_send() done.\n");

    return LGW_HAL_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_status(uint8_t select, uint8_t *code) {
    int32_t read_value;

    /* check input variables */
    CHECK_NULL(code);

    if (select == TX_STATUS) {
        esp_lgw_reg_r(LGW_TX_STATUS, &read_value);
        if ((read_value & 0x10) == 0) { /* bit 4 @1: TX programmed */
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

int esp_lgw_abort_tx(void) {
    int i;

    i = esp_lgw_reg_w(LGW_TX_TRIG_ALL, 0);

    if (i == LGW_REG_SUCCESS) {
        return LGW_HAL_SUCCESS;
    } else {
        return LGW_HAL_ERROR;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int esp_lgw_get_trigcnt(uint32_t* trig_cnt_us) {
    int i;
    int32_t val;

    i = esp_lgw_reg_r(LGW_TIMESTAMP, &val);
    if (i == LGW_REG_SUCCESS) {
        *trig_cnt_us = (uint32_t)val;
        return LGW_HAL_SUCCESS;
    } else {
        return LGW_HAL_ERROR;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char* esp_lgw_version_info() {
    return esp_lgw_version_string;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint32_t esp_lgw_time_on_air(struct lgw_pkt_tx_s *packet) {
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
        Tsym = pow(2.0, (int)SF) / BW;

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

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void esp_lgw_calibration_offset_transfer(uint8_t idx_start, uint8_t idx_nb) {
    int i;
    int read_val;

    DEBUG_PRINTF("start calibration for index [%u-%u]\n", idx_start, idx_start + idx_nb);

    /* Get 'idx_nb' calibration offsets from AGC FW, and put it in the local 
    calibration offsets array, at 'idx_start' index position */
    for(i = 0; i < idx_nb; ++i) {
        esp_lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xA0 + i);
        ets_delay_us(1000);
        esp_lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        ets_delay_us(1000);
        cal_offset_a_i[i + idx_start] = (int8_t)read_val;
        esp_lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xA8 + i);
        ets_delay_us(1000);
        esp_lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        ets_delay_us(1000);
        cal_offset_a_q[i + idx_start] = (int8_t)read_val;
        esp_lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xB0 + i);
        ets_delay_us(1000);
        esp_lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        ets_delay_us(1000);
        cal_offset_b_i[i + idx_start] = (int8_t)read_val;
        esp_lgw_reg_w(LGW_DBG_AGC_MCU_RAM_ADDR, 0xB8 + i);
        ets_delay_us(1000);
        esp_lgw_reg_r(LGW_DBG_AGC_MCU_RAM_DATA, &read_val);
        ets_delay_us(1000);
        cal_offset_b_q[i + idx_start] = (int8_t)read_val;
    }

    /* Fill the first 5 offsets [0-4] with the value of index 5 */
    for(i = 0; i < 5; i++) {
        cal_offset_a_i[i] = cal_offset_a_i[5];
        cal_offset_a_q[i] = cal_offset_a_q[5];
        cal_offset_b_i[i] = cal_offset_b_i[5];
        cal_offset_b_q[i] = cal_offset_b_q[5];
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void calibration_save(void) {
    esp_lgw_reg_r(LGW_IQ_MISMATCH_A_AMP_COEFF, &iqrxtab[0]);
    esp_lgw_reg_r(LGW_IQ_MISMATCH_A_PHI_COEFF, &iqrxtab[1]);
    esp_lgw_reg_r(LGW_IQ_MISMATCH_B_AMP_COEFF, &iqrxtab[2]);
    esp_lgw_reg_r(LGW_IQ_MISMATCH_B_PHI_COEFF, &iqrxtab[3]);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void calibration_reload(void) {
    esp_lgw_reg_w(LGW_IQ_MISMATCH_A_AMP_COEFF, iqrxtab[0]);
    esp_lgw_reg_w(LGW_IQ_MISMATCH_A_PHI_COEFF, iqrxtab[1]);
    esp_lgw_reg_w(LGW_IQ_MISMATCH_B_AMP_COEFF, iqrxtab[2]);
    esp_lgw_reg_w(LGW_IQ_MISMATCH_B_PHI_COEFF, iqrxtab[3]);
}

/* --- EOF ------------------------------------------------------------------ */
