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
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define CHECK_NULL(a)                if(a==NULL){return LGW_COM_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_mcu_board_setconf(uint8_t *data, uint16_t size) {
    int i;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    cmd.id = 'i';
    cmd.len_msb = (uint8_t)(size >> 8);
    cmd.len_lsb = (uint8_t)(size - ((size >> 8) << 8));
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxrf_setconf(uint8_t rfchain, uint8_t *data, uint16_t size) {
    int i;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    cmd.id = 'c';
    cmd.len_msb = (uint8_t)(size >> 8);
    cmd.len_lsb = (uint8_t)(size - ((size >> 8) << 8));
    cmd.address = rfchain;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxif_setconf(uint8_t ifchain, uint8_t *data, uint16_t size) {
    int i;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    cmd.id = 'd';
    cmd.len_msb = (uint8_t)(size >> 8);
    cmd.len_lsb = (uint8_t)(size - ((size >> 8) << 8));
    cmd.address = ifchain;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_txgain_setconf(uint8_t *data, uint16_t size) {
    int i;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    cmd.id = 'h';
    cmd.len_msb = (uint8_t)(size >> 8);
    cmd.len_lsb = (uint8_t)(size - ((size >> 8) << 8));
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];

    }

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_receive(uint8_t max_packet, uint8_t *data) {
    int i, j, x;
    int pt = 0;
    int cptalc = 0;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    /* Prepare command for fetching packets */
    cmd.id = 'b';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = max_packet;

    x = lgw_com_send_command(cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        DEBUG_MSG("ERROR: failed to receive packets from concentrator\n");
        return 0;
    }

    DEBUG_PRINTF("NOTE: Available packet %d %d\n", ans.ans_data[0], (ans.len_msb << 8) + ans.len_lsb);
    DEBUG_PRINTF("NOTE: read structure %d %d %d %d %d\n", ans.ans_data[5], ans.ans_data[6], ans.ans_data[7], ans.ans_data[8], ans.ans_data[9]);

    /* over the number of packets */
    for (i = 0; i < ans.ans_data[0]; i++) {
        /* for each packet */
        for (j = 0; j < ans.ans_data[cptalc + 43] + 44; j++) {
            pt = ans.ans_data[cptalc + 43] + 44;
            data[(i * 300) + j] = ans.ans_data[j + cptalc + 1]; /* 300 size of struct target */
        }
        cptalc = pt;
    }

    return ans.ans_data[0];
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_send(uint8_t *data, uint16_t size) {
    int i;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    cmd.id = 'f';
    cmd.len_msb = (uint8_t)(size >> 8);
    cmd.len_lsb = (uint8_t)(size - ((size >> 8) << 8));
    cmd.address = 0;
    for (i = 0; i < size; i++) {
        cmd.cmd_data[i] = data[i];
    }

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_trigcnt(uint32_t *data) {
    int x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    /* check input variables */
    CHECK_NULL(data);

    cmd.id = 'q';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = 0;

    x = lgw_com_send_command(cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        DEBUG_MSG("ERROR: failed to get concentrator internal counter\n");
        return LGW_COM_ERROR;
    }

    *data = (ans.ans_data[0] << 24) + (ans.ans_data[1] << 16) + (ans.ans_data[2] << 8) + (ans.ans_data[3]);
    DEBUG_PRINTF("sx1301 counter %d\n", (ans.ans_data[0] << 24) + (ans.ans_data[1] << 16) + (ans.ans_data[2] << 8) + (ans.ans_data[3]));

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_commit_radio_calibration(void) {
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    cmd.id = 'j';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = 0;

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_reset(void) {
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    cmd.id = 'm';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = 0;

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_set_dfu_mode(void) {
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    cmd.id = 'n';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = 0;
    cmd.cmd_data[0] = 0;

    return lgw_com_send_command(cmd, &ans);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_unique_id(uint8_t *uid) {
    int i, x;
    int fwversion = STM32FWVERSION;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;

    cmd.id = 'l';
    cmd.len_msb = 0;
    cmd.len_lsb = 4;
    cmd.address = 0;
    cmd.cmd_data[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
    cmd.cmd_data[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
    cmd.cmd_data[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
    cmd.cmd_data[3] = (uint8_t)((fwversion) & (0x000000ff));

    x = lgw_com_send_command(cmd, &ans);
    if (x != LGW_COM_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to get MCU unique ID\n");
        return LGW_COM_ERROR;
    }

    /* Check MCU FW version */
    if (ans.ans_data[0] == ACK_KO) {
        DEBUG_MSG("ERROR: Invalid MCU firmware version\n");
        return LGW_COM_ERROR;
    }

    /* Get MCU unique ID */
    for (i = 0; i < 7; i++) {
        uid[i] = ans.ans_data[i + 1];
    }

    return LGW_COM_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
