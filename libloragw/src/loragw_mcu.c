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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_MCU == 1
#define DEBUG_MSG(str)                fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_COM_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define CHECK_NULL(a)                if(a==NULL){return LGW_COM_ERROR;}
#endif

#define UNUSED(x) (void)(x)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

extern void *lgw_com_target; /*! generic pointer to the SPI device */

pthread_mutex_t mx_usbbridgesync = PTHREAD_MUTEX_INITIALIZER; /* control access to usbbridge sync offsets */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_mcu_board_setconf(uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target;

    mystruct.Cmd = 'i';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxrf_setconf(uint8_t rfchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target;

    mystruct.Cmd = 'c';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = rfchain;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_rxif_setconf(uint8_t ifchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'd';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = ifchain;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_txgain_setconf(uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'h';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];

    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_receive(uint8_t max_packet, uint8_t *data) {
    int fd;
    int i, j;
    int pt = 0;
    int cptalc = 0;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    /* Prepare command for fetching packets */
    mystruct.Cmd = 'b';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = max_packet;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return (0); // for 0 receive packet
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    DEBUG_PRINTF("NOTE: Available packet %d %d\n", mystrctAns.Rxbuf[0], (mystrctAns.LenMsb << 8) + mystrctAns.Len);
    DEBUG_PRINTF("NOTE: read structure %d %d %d %d %d\n", mystrctAns.Rxbuf[5], mystrctAns.Rxbuf[6], mystrctAns.Rxbuf[7], mystrctAns.Rxbuf[8], mystrctAns.Rxbuf[9]);

    /* over the number of packets */
    for (i = 0; i < mystrctAns.Rxbuf[0]; i++) {
        /* for each packet */
        for (j = 0; j < mystrctAns.Rxbuf[cptalc + 43] + 44; j++) {
            pt = mystrctAns.Rxbuf[cptalc + 43] + 44;
            data[(i * 300) + j] = mystrctAns.Rxbuf[j + cptalc + 1]; /* 300 size of struct target */
        }
        cptalc = pt;
    }

    return mystrctAns.Rxbuf[0];
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_send(uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'f';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_trigcnt(uint32_t *data) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);
    CHECK_NULL(data);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'q';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    *data = (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]);
    DEBUG_PRINTF("sx1301 counter %d\n", (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]));

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_commit_radio_calibration(void) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'j';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_reset(void) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'm';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_set_dfu_mode(void) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);

    fd = *(int *)lgw_com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'n';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_mcu_get_unique_id(uint8_t *uid) {
    int fd;
    int i;
    int fwversion = STM32FWVERSION;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(lgw_com_target);

    fd = *(int *)lgw_com_target;

    mystruct.Cmd = 'l';
    mystruct.LenMsb = 0;
    mystruct.Len = 4;
    mystruct.Address = 0;
    mystruct.Value[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
    mystruct.Value[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
    mystruct.Value[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
    mystruct.Value[3] = (uint8_t)((fwversion) & (0x000000ff));

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd) == OK) {
        if (mystrctAns.Rxbuf[0] == ACK_KO) {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
        for (i = 0; i < 7; i++) {
            uid[i] = mystrctAns.Rxbuf[i + 1];
        }
    } else {
        DEBUG_MSG("ERROR: Failed to get MCU unique ID\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
