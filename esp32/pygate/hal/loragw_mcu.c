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
 Wrapper to call MCU's HAL functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>             /* C99 types */
#include <stdio.h>              /* printf fprintf */
#include <stdlib.h>             /* malloc free */
#include <unistd.h>             /* lseek, close */
#include <fcntl.h>              /* open */
#include <string.h>             /* memset */
#include <errno.h>              /* Error number definitions */
#include <termios.h>            /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>

#include "loragw_com.h"
#include "loragw_mcu.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#if DEBUG_MCU == 1
#define DEBUG_MSG(str)                fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_COM_ERROR;}
#else
#define DEBUG_MSG(str)                  printf(str)
#define DEBUG_PRINTF(fmt, args...)
#define CHECK_NULL(a)                if(a==NULL){return LGW_COM_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

extern void *lgw_com_target; /*! generic pointer to the COM device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_mcu_board_setconf(struct lgw_conf_board_s* conf) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    uint8_t PADDING = 0;
    uint8_t data[4];
    uint16_t size;

    /* struct to byte array */
    data[0] = conf->lorawan_public;
    data[1] = conf->clksrc;
    data[2] = PADDING;
    data[3] = PADDING;
    size = sizeof(data) / sizeof(uint8_t);

    /* prepare command */
    cmd.id = 'i';
    cmd.len_msb = (uint8_t)((size >> 8) & 0xFF);
    cmd.len_lsb = (uint8_t)((size >> 0) & 0xFF);
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        printf("ERROR: failed to configure board\n");
        return LGW_MCU_ERROR;
    }

    /* check command acknoledge */
    if (ans.status != ACK_OK) {
        printf("ERROR: failed to configure board, ACK failed\n");
        return LGW_MCU_ERROR;
    }

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxrf_setconf(uint8_t rfchain, struct lgw_conf_rxrf_s* conf) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    uint8_t PADDING = 0;
    uint8_t data[20];
    uint16_t size;

    /* struct to byte array */
    /* --- 64-bits start --- */
    data[0] = conf->enable;
    data[1] = PADDING;
    data[2] = PADDING;
    data[3] = PADDING;
    data[4] = *(((uint8_t *)(&conf->freq_hz)));
    data[5] = *(((uint8_t *)(&conf->freq_hz)) + 1);
    data[6] = *(((uint8_t *)(&conf->freq_hz)) + 2);
    data[7] = *(((uint8_t *)(&conf->freq_hz)) + 3);
    /* --- 64-bits start --- */
    data[8] = *(((uint8_t *)(&conf->rssi_offset)));
    data[9] = *(((uint8_t *)(&conf->rssi_offset)) + 1);
    data[10] = *(((uint8_t *)(&conf->rssi_offset)) + 2);
    data[11] = *(((uint8_t *)(&conf->rssi_offset)) + 3);
    data[12] = *(((uint8_t *)(&conf->type)));
    data[13] = PADDING;
    data[14] = PADDING;
    data[15] = PADDING;
    /* --- 64-bits start --- */
    data[16] = *(((uint8_t *)(&conf->tx_enable)));
    data[17] = *(((uint8_t *)(&conf->tx_enable)) + 1);
    data[18] = *(((uint8_t *)(&conf->tx_enable)) + 2);
    data[19] = *(((uint8_t *)(&conf->tx_enable)) + 3);
    size = sizeof(data) / sizeof(uint8_t);

    /* prepare command */
    cmd.id = 'c';
    cmd.len_msb = (uint8_t)((size >> 8) & 0xFF);
    cmd.len_lsb = (uint8_t)((size >> 0) & 0xFF);
    cmd.address = rfchain;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        printf("ERROR: failed to send rxrf configuration\n");
        return LGW_MCU_ERROR;
    }

    /* check command acknoledge */
    if (ans.status != ACK_OK) {
        printf("ERROR: rxrf configuration, ACK failed\n");
        return LGW_MCU_ERROR;
    }

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxif_setconf(uint8_t ifchain, struct lgw_conf_rxif_s* conf) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    uint8_t PADDING = 0;
    uint8_t data[32];
    uint16_t size;

    /* struct to byte array */
    /* --- 64-bits start --- */
    data[0] = conf->enable;
    data[1] = *(((uint8_t *)(&conf->rf_chain)));
    data[2] = PADDING;
    data[3] = PADDING;
    data[4] = *(((uint8_t *)(&conf->freq_hz)));
    data[5] = *(((uint8_t *)(&conf->freq_hz)) + 1);
    data[6] = *(((uint8_t *)(&conf->freq_hz)) + 2);
    data[7] = *(((uint8_t *)(&conf->freq_hz)) + 3);
    /* --- 64-bits start --- */
    data[8] = *(((uint8_t *)(&conf->bandwidth)));
    data[9] = PADDING;
    data[10] = PADDING;
    data[11] = PADDING;
    data[12] = *(((uint8_t *)(&conf->datarate)));
    data[13] = *(((uint8_t *)(&conf->datarate)) + 1);
    data[14] = *(((uint8_t *)(&conf->datarate)) + 2);
    data[15] = *(((uint8_t *)(&conf->datarate)) + 3);
    /* --- 64-bits start --- */
    data[16] = *(((uint8_t *)(&conf->sync_word_size)));
    data[17] = PADDING;
    data[18] = PADDING;
    data[19] = PADDING;
    data[20] = PADDING;
    data[21] = PADDING;
    data[22] = PADDING;
    data[23] = PADDING;
    /* --- 64-bits start --- */
    data[24] = *(((uint8_t *)(&conf->sync_word)));
    data[25] = *(((uint8_t *)(&conf->sync_word)) + 1);
    data[26] = *(((uint8_t *)(&conf->sync_word)) + 2);
    data[27] = *(((uint8_t *)(&conf->sync_word)) + 3);
    data[28] = *(((uint8_t *)(&conf->sync_word)) + 4);
    data[29] = *(((uint8_t *)(&conf->sync_word)) + 5);
    data[30] = *(((uint8_t *)(&conf->sync_word)) + 6);
    data[31] = *(((uint8_t *)(&conf->sync_word)) + 7);
    size = sizeof(data) / sizeof(uint8_t);

    /* prepare command */
    cmd.id = 'd';
    cmd.len_msb = (uint8_t)((size >> 8) & 0xFF);
    cmd.len_lsb = (uint8_t)((size >> 0) & 0xFF);
    cmd.address = ifchain;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        printf("ERROR: failed to send rxif configuration\n");
        return LGW_MCU_ERROR;
    }

    /* check command acknoledge */
    if (ans.status != ACK_OK) {
        printf("ERROR: rxif configuration, ACK failed\n");
        return LGW_MCU_ERROR;
    }

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_txgain_setconf(struct lgw_tx_gain_lut_s *conf) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    uint32_t u = 0;
    uint8_t data[(LGW_MULTI_NB * TX_GAIN_LUT_SIZE_MAX) + 4];
    uint16_t size;

    /* struct to byte array */
    for (u = 0; u < TX_GAIN_LUT_SIZE_MAX; u++) {
        data[0 + (5 * u)] = 0;
        data[1 + (5 * u)] = 0;
        data[2 + (5 * u)] = 0;
        data[3 + (5 * u)] = 0;
        data[4 + (5 * u)] = 0;
    }

    for (u = 0; u < conf->size; u++) {
        data[0 + (5 * u)] = conf->lut[u].dig_gain;
        data[1 + (5 * u)] = conf->lut[u].pa_gain;
        data[2 + (5 * u)] = conf->lut[u].dac_gain;
        data[3 + (5 * u)] = conf->lut[u].mix_gain;
        data[4 + (5 * u)] = conf->lut[u].rf_power;
    }
    data[(TX_GAIN_LUT_SIZE_MAX) * 5] = conf->size;
    size = ((TX_GAIN_LUT_SIZE_MAX) * 5) + 1;

    /* prepare command */
    cmd.id = 'h';
    cmd.len_msb = (uint8_t)((size >> 8) & 0xFF);
    cmd.len_lsb = (uint8_t)((size >> 0) & 0xFF);
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        printf("ERROR: failed to send tx gain configuration\n");
        return LGW_MCU_ERROR;
    }

    /* check command acknoledge */
    if (ans.status != ACK_OK) {
        printf("ERROR: tx gain configuration, ACK failed\n");
        return LGW_MCU_ERROR;
    }

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_receive(uint8_t max_pkt, struct lgw_pkt_rx_s *pkt_data) {
    int i, j, x;
    int cptalc = 0;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    int nb_packet ;
    uint8_t data[LGW_PKT_RX_STRUCT_SIZE_ALIGNED * max_pkt];
    uint16_t pkt_size;

    /* check input variables */
    CHECK_NULL(pkt_data);

    /* Prepare command for fetching packets */
    cmd.id = 'b';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = max_pkt;

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if ((x != LGW_COM_SUCCESS) || (ans.status != ACK_OK)) {
        DEBUG_MSG("ERROR: failed to receive packets from concentrator\n");
        return 0;
    }

    /* check nb_packet variables */
    nb_packet = ans.ans_data[0];
    if ((nb_packet > LGW_PKT_FIFO_SIZE) || (nb_packet < 0)) {
        DEBUG_PRINTF("ERROR: NOT A VALID NUMBER OF RECEIVED PACKET (%d)\n", nb_packet);
        return 0;
    }

    //DEBUG_PRINTF("NOTE: Available packet %d %d\n", nb_packet, (ans.len_msb << 8) + ans.len_lsb);

    /* over the number of packets */
    for (i = 0; i < nb_packet; i++) {
        /* for each packet */
        pkt_size = (uint16_t)((uint8_t)(ans.ans_data[cptalc + 42] << 8) | (uint8_t)ans.ans_data[cptalc + 43]);
        for (j = 0; j < (LGW_PKT_RX_METADATA_SIZE_ALIGNED + pkt_size); j++) {
            data[(i * LGW_PKT_RX_STRUCT_SIZE_ALIGNED) + j] = ans.ans_data[j + cptalc + 1]; /* +1 because ans.ans_data[0] is nb_packet */
        }
        cptalc += j;
    }

    /* byte array to struct - the following code is done to work both with 32 or 64 bits host */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    for (i = 0; i < nb_packet; i++) {
        /* --- 64-bits start --- */
        pkt_data[i].freq_hz = *((uint32_t*)(&data[0 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].if_chain = *((uint8_t*)(&data[4 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].status = *((uint8_t*)(&data[5 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* 1 BYTE PADDING FOR 64-bits ALIGNMENT */
        /* 1 BYTE PADDING FOR 64-bits ALIGNMENT */
        /* --- 64-bits start --- */
        pkt_data[i].count_us = *((uint32_t*)(&data[8 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].rf_chain = *((uint8_t*)(&data[12 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].modulation = *((uint8_t*)(&data[13 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].bandwidth = *((uint8_t*)(&data[14 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* 1 BYTE PADDING FOR 64-bits ALIGNMENT */
        /* --- 64-bits start --- */
        pkt_data[i].datarate = *((uint32_t*)(&data[16 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].coderate = *((uint8_t*)(&data[20 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* --- 64-bits start --- */
        pkt_data[i].rssi = *((float*)(&data[24 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].snr = *((float*)(&data[28 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* --- 64-bits start --- */
        pkt_data[i].snr_min = *((float*)(&data[32 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].snr_max = *((float*)(&data[36 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* --- 64-bits start --- */
        pkt_data[i].crc = *((uint16_t*)(&data[40 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        pkt_data[i].size = *((uint16_t*)(&data[42 + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        /* NO PADDING NEEDED HERE, END OF ARRAY */
        for (j = 0; j < 256; j++) {
            (pkt_data[i].payload[j]) = *((uint8_t*)(&data[LGW_PKT_RX_METADATA_SIZE_ALIGNED + j + LGW_PKT_RX_STRUCT_SIZE_ALIGNED * i]));
        }
    }
#pragma GCC diagnostic pop

    return nb_packet;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_send(struct lgw_pkt_tx_s* pkt_data) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    uint8_t PADDING = 0;
    uint8_t data[LGW_PKT_TX_STRUCT_SIZE_ALIGNED];
    uint16_t size;

    /* struct to byte array */
    /* --- 64-bits start --- */
    data[0] = *(((uint8_t *)(&pkt_data->freq_hz)));
    data[1] = *(((uint8_t *)(&pkt_data->freq_hz)) + 1);
    data[2] = *(((uint8_t *)(&pkt_data->freq_hz)) + 2);
    data[3] = *(((uint8_t *)(&pkt_data->freq_hz)) + 3);
    data[4] = *(((uint8_t *)(&pkt_data->tx_mode)));
    data[5] = PADDING;
    data[6] = PADDING;
    data[7] = PADDING;
    /* --- 64-bits start --- */
    data[8] = *(((uint8_t *)(&pkt_data->count_us)));
    data[9] = *(((uint8_t *)(&pkt_data->count_us)) + 1);
    data[10] = *(((uint8_t *)(&pkt_data->count_us)) + 2);
    data[11] = *(((uint8_t *)(&pkt_data->count_us)) + 3);
    data[12] = *(((uint8_t *)(&pkt_data->rf_chain)));
    data[13] = *(((uint8_t *)(&pkt_data->rf_power)));
    data[14] = *(((uint8_t *)(&pkt_data->modulation)));
    data[15] = *(((uint8_t *)(&pkt_data->bandwidth)));
    /* --- 64-bits start --- */
    data[16] = *(((uint8_t *)(&pkt_data->datarate)));
    data[17] = *(((uint8_t *)(&pkt_data->datarate)) + 1);
    data[18] = *(((uint8_t *)(&pkt_data->datarate)) + 2);
    data[19] = *(((uint8_t *)(&pkt_data->datarate)) + 3);
    data[20] = *(((uint8_t *)(&pkt_data->coderate)));
    data[21] = *(((uint8_t *)(&pkt_data->invert_pol)));
    data[22] = *(((uint8_t *)(&pkt_data->f_dev)));
    data[23] = PADDING;
    /* --- 64-bits start --- */
    data[24] = *(((uint8_t *)(&pkt_data->preamble)));
    data[25] = *(((uint8_t *)(&pkt_data->preamble)) + 1);
    data[26] = *(((uint8_t *)(&pkt_data->no_crc)));
    data[27] = *(((uint8_t *)(&pkt_data->no_header)));
    data[28] = *(((uint8_t *)(&pkt_data->size)));
    data[29] = *(((uint8_t *)(&pkt_data->size)) + 1);
    /* NO PADDING NEEDED HERE, END OF ARRAY */
    for (i = 0; i < 256; i++) {
        data[i + LGW_PKT_TX_METADATA_SIZE_ALIGNED] = *(((uint8_t *)(&pkt_data->payload)) + i);
    }
    size = sizeof(data) / sizeof(uint8_t);

    /* prepare command */
    cmd.id = 'f';
    cmd.len_msb = (uint8_t)((size >> 8) & 0xFF);
    cmd.len_lsb = (uint8_t)((size >> 0) & 0xFF);
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        printf("ERROR: failed to send packet\n");
        return LGW_MCU_ERROR;
    }

    /* check command acknoledge */
    if (ans.status != ACK_OK) {
        printf("ERROR: failed to send packet, ACK failed\n");
        return LGW_MCU_ERROR;
    }

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_trigcnt(uint32_t *data) {
    int x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    /* prepare command */
    cmd.id = 'q';
    cmd.len_msb = 0;
    cmd.len_lsb = 0;
    cmd.address = 0;

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if ((x != LGW_COM_SUCCESS) || (ans.status != ACK_OK)) {
        DEBUG_MSG("ERROR: failed to get concentrator internal counter\n");
        return LGW_MCU_ERROR;
    }

    *data = (ans.ans_data[0] << 24) + (ans.ans_data[1] << 16) + (ans.ans_data[2] << 8) + (ans.ans_data[3]);
    DEBUG_PRINTF("Note: sx1301 counter %u\n", *data);

    return LGW_MCU_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_commit_radio_calibration(uint8_t idx_start, uint8_t idx_nb) {
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* prepare command */
    cmd.id = 'j';
    cmd.len_msb = 0;
    cmd.len_lsb = 2;
    cmd.address = 0;
    cmd.cmd_data[0] = idx_start;
    cmd.cmd_data[1] = idx_nb;

    /* send command to MCU */
    return lgw_com_send_command(lgw_com_target, cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_reset(void) {
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* prepare command */
    cmd.id = 'm';
    cmd.len_msb = 0;
    cmd.len_lsb = 0;
    cmd.address = 0;

    /* send command to MCU */
    return lgw_com_send_command(lgw_com_target, cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_unique_id(uint8_t *uid) {
    int i, x;
    int fwversion = STM32FWVERSION;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* prepare command */
    cmd.id = 'l';
    cmd.len_msb = 0;
    cmd.len_lsb = 4;
    cmd.address = 0;
    cmd.cmd_data[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
    cmd.cmd_data[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
    cmd.cmd_data[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
    cmd.cmd_data[3] = (uint8_t)((fwversion) & (0x000000ff));

    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to get MCU unique ID\n");
        return LGW_MCU_ERROR;
    }

    /* Check MCU FW version */
    if (ans.status == ACK_KO) {
        DEBUG_MSG("ERROR: Invalid MCU firmware version\n");
        return LGW_MCU_ERROR;
    }

    /* Get MCU unique ID */
    for (i = 0; i <= 7; i++) {
        uid[i] = ans.ans_data[i];
    }

    return LGW_MCU_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
