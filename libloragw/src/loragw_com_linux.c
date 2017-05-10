/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
this file contains the USB commands to configure and communicate with the SX1308
LoRA concentrator.
A USB CDC drivers is required to establish the connection with the PicoCell
board.

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* printf fprintf */
#include <stdlib.h>     /* malloc free */
#include <unistd.h>     /* lseek, close */
#include <fcntl.h>      /* open */
#include <string.h>     /* memset */
#include <errno.h>      /* Error number definitions */
#include <termios.h>    /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include "loragw_com_linux.h"
#include "loragw_hal.h"
#include "loragw_aux.h"
#include "loragw_reg.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_COM == 1
#define DEBUG_MSG(str)                fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_COM_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define CHECK_NULL(a)                if(a==NULL){return LGW_COM_ERROR;}
#endif

#define UNUSED(x) (void)(x)

#define OK 1
#define KO 0
#define ACK_KO   0

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

static pthread_mutex_t mx_usbbridgesync = PTHREAD_MUTEX_INITIALIZER; /* control access to usbbridge sync offsets */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

int set_interface_attribs_linux(int fd, int speed) {
    struct termios tty;

    memset(&tty, 0, sizeof tty);

    /* Get current attributes */
    if (tcgetattr(fd, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcgetattr failed with %d - %s", errno, strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    /* Control Modes */
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; /* set 8-bit characters */
    tty.c_cflag |= CLOCAL;                      /* local connection, no modem control */
    tty.c_cflag |= CREAD;                       /* enable receiving characters */
    tty.c_cflag &= ~PARENB;                     /* no parity */
    tty.c_cflag &= ~CSTOPB;                     /* one stop bit */
    /* Input Modes */
    tty.c_iflag &= ~IGNBRK;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    /* Output Modes */
    tty.c_oflag = 0;
    /* Local Modes */
    tty.c_lflag = 0;
    /* Settings for non-canonical mode */
    tty.c_cc[VMIN] = 0;                         /* non-blocking mode */
    tty.c_cc[VTIME] = 50;                       /* wait for (n * 0.1) seconds before returning */

    /* Set attributes */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcsetattr failed with %d - %s", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* configure TTYACM0 read blocking or not*/
int set_blocking_linux(int fd, bool blocking) {
    struct termios tty;

    memset(&tty, 0, sizeof tty);

    /* Get current attributes */
    if (tcgetattr(fd, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcgetattr failed with %d - %s", errno, strerror(errno));
        return -1;
    }

    tty.c_cc[VMIN] = (blocking == true) ? 1 : 0;    /* set blocking or non-blocking mode */
    tty.c_cc[VTIME] = 1;                            /* wait for (n * 0.1) seconds before returning */

    /* Set attributes */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcsetattr failed with %d - %s", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int checkcmd_linux(uint8_t cmd) {
    switch (cmd) {
        case 'r': /* read register */
        case 's':
        case 't':
        case 'u':
        case 'p':
        case 'e':
        case 'w': /* write register */
        case 'x':
        case 'y':
        case 'z':
        case 'a':
        case 'b': /* lgw_receive */
        case 'c': /* lgw_rxrf_setconf */
        case 'd': /* lgw_rxif_setconf */
        case 'f': /* lgw_send */
        case 'h': /* lgw_txgain_setconf */
        case 'q': /* lgw_trigger */
        case 'i': /* lgw_board_setconf */
        case 'j': /* lgw_calibration_snapshot */
        case 'l': /* lgw_check_fw_version */
        case 'm': /* reset STM32 */
        case 'n': /* Go to DFU */
            return(0);
        default:
            return(OK);
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int SendCmd_linux(CmdSettings_t CmdSettings, int fd) {
    uint8_t buffertx[CMD_HEADER_TX_SIZE + CMD_DATA_TX_SIZE];
    uint16_t Clen = CmdSettings.Len + (CmdSettings.LenMsb << 8);
    uint16_t Tlen = CMD_HEADER_TX_SIZE + Clen;
    int i;
    ssize_t lencheck;

    /* Initialize buffer */
    memset(buffertx, 0, sizeof buffertx);

    /* Prepare command */
    buffertx[0] = (uint8_t)CmdSettings.Cmd;
    buffertx[1] = CmdSettings.LenMsb;
    buffertx[2] = CmdSettings.Len;
    buffertx[3] = CmdSettings.Address;
    for (i = 0; i < Clen; i++) {
        buffertx[i + 4] = CmdSettings.Value[i];
    }

    /* Send command */
    lencheck = write(fd, buffertx, Tlen);
    if (lencheck < 0) {
        DEBUG_PRINTF("ERROR: failed to write cmd (%d - %s)\n", errno, strerror(errno));
        return(KO);
    }
    if (lencheck != Tlen) {
        DEBUG_PRINTF("WARNING: incomplete cmd written (%d)\n", (int)lencheck);
    }

    DEBUG_PRINTF("Note: sent cmd \'%c\', length=%d\n", CmdSettings.Cmd, Clen);

    return(OK);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int ReceiveAns_linux(AnsSettings_t *Ansbuffer, int fd) {
    uint8_t bufferrx[CMD_HEADER_RX_SIZE + CMD_DATA_RX_SIZE];
    int i;
    int cpttimer = 0;
    size_t cmd_size;
    ssize_t buf_size = 0;
    ssize_t lencheck;

    /* Initialize variables */
    memset(bufferrx, 0, sizeof bufferrx);
    cpttimer = 0;

    /* Wait for cmd answer header */
    while (checkcmd_linux(bufferrx[0])) {
        lencheck = read(fd, bufferrx, CMD_HEADER_RX_SIZE);
        if (lencheck < 0) {
            DEBUG_PRINTF("WARNING: failed to read from communication bridge (%d - %s), retry...\n", errno, strerror(errno));
        } else if (lencheck == 0) {
            DEBUG_MSG("WARNING: no data read yet, retry...\n");
        } else if ((lencheck > 0) && (lencheck < CMD_HEADER_RX_SIZE)) { /* TODO: improve mechanism to try to get complete hearder? */
            DEBUG_MSG("ERROR: read incomplete cmd answer, aborting.\n");
            return(KO);
        }
        /* Exit after several unsuccessful read */
        cpttimer++;
        if (cpttimer > 15) {
            DEBUG_PRINTF("ERROR: failed to receive answer, aborting.");
            return(KO);
        }
    }
    cmd_size = (bufferrx[1] << 8) + bufferrx[2];

    /* Wait for more data */
    wait_ns((cmd_size + 1) * 6000); /* TODO: refine this tempo */

    /* Read the answer */
    buf_size = cmd_size + CMD_HEADER_RX_SIZE;
    if ((buf_size % 64) == 0) {
        buf_size = cmd_size + 1; /* one padding byte is added by USB driver, we need to read it */
    } else {
        buf_size = cmd_size;
    }
    lencheck = read(fd, &bufferrx[CMD_HEADER_RX_SIZE], buf_size);
    if (lencheck < buf_size) {
        DEBUG_PRINTF("ERROR: failed to read cmd answer (%d - %s)\n", errno, strerror(errno));
        return(KO);
    }
    Ansbuffer->Cmd = (char)bufferrx[0];
    Ansbuffer->LenMsb = bufferrx[1];
    Ansbuffer->Len = bufferrx[2];
    for (i = 0; i < (bufferrx[1] << 8) + bufferrx[2]; i++) {
        Ansbuffer->Rxbuf[i] = bufferrx[CMD_HEADER_RX_SIZE + i];
    }

    DEBUG_PRINTF("Note: received answer for cmd \'%c\', length=%d\n", bufferrx[0], (bufferrx[1] << 8) + bufferrx[2]);

    return(OK);
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* open USB port */
int lgw_com_open_linux(void **com_target_ptr) {

    int *usb_device = NULL;
    char portname[50];
    int i, x;
    int fd;
    int fwversion = STM32FWVERSION;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /*check input variables*/
    CHECK_NULL(com_target_ptr);

    usb_device = malloc(sizeof(int));
    if (usb_device == NULL) {
        DEBUG_MSG("ERROR : MALLOC FAIL\n");
        return LGW_COM_ERROR;
    }

    /* try to open one of the 10 port ttyACM */
    for (i = 0; i < 10; i++) {
        sprintf(portname, "/dev/ttyACM%d", i);
        fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
            DEBUG_PRINTF("ERROR: failed to open USB port %s - %s\n", portname, strerror(errno));
        } else {
            x = set_interface_attribs_linux(fd, B921600);
            x |= set_blocking_linux(fd, false);
            if (x != 0) {
                DEBUG_PRINTF("ERROR: failed to configure USB port %s\n", portname);
                return LGW_COM_ERROR;
            }

            *usb_device = fd;
            *com_target_ptr = (void*)usb_device;

            /* Check that MCU firmware version is correct */
            mystruct.Cmd = 'l';
            mystruct.LenMsb = 0;
            mystruct.Len = 4;
            mystruct.Address = 0;
            mystruct.Value[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
            mystruct.Value[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
            mystruct.Value[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
            mystruct.Value[3] = (uint8_t)((fwversion) & (0x000000ff));

            pthread_mutex_lock(&mx_usbbridgesync);
            SendCmd_linux(mystruct, fd);
            if (ReceiveAns_linux(&mystrctAns, fd)) {
                if (mystrctAns.Rxbuf[0] == ACK_KO) {
                    pthread_mutex_unlock(&mx_usbbridgesync);
                    DEBUG_MSG("ERROR: Wrong MCU firmware version\n");
                    return LGW_COM_ERROR;
                }
            } else {
                pthread_mutex_unlock(&mx_usbbridgesync);
                DEBUG_MSG("ERROR: failed to get MCU firmware version\n");
                return LGW_COM_ERROR;
            }
            pthread_mutex_unlock(&mx_usbbridgesync);
            DEBUG_PRINTF("Note: MCU firmware version checked: 0x%X\n", STM32FWVERSION);
            return LGW_COM_SUCCESS;
        }
    }

    return LGW_COM_ERROR;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* COM release */
int lgw_com_close_linux(void *com_target) {
    int usb_device;
    int a;

    /*check input variables*/
    CHECK_NULL(com_target);

    usb_device = *(int*)com_target;

    a = close(usb_device);
    if (a < 0) {
        DEBUG_PRINTF("ERROR: failed to close USB port - %s\n", strerror(errno));
        return LGW_COM_ERROR;
    } else {
        DEBUG_MSG("Note : USB port closed \n");
        return LGW_COM_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
int lgw_com_w_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /*check input variables*/
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'w';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = address;
    mystruct.Value[0] = data;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
int lgw_com_r_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'r';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = address;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    *data = mystrctAns.Rxbuf[0];

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_wb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    int sizei = size;
    int cptalc = 0;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    /* prepare command byte */
    pthread_mutex_lock(&mx_usbbridgesync);
    while (sizei > ATOMICTX) {
        if (sizei == size) {
            mystruct.Cmd = 'x'; //first part of big packet
        } else {
            mystruct.Cmd = 'y';   //middle part of big packet
        }

        mystruct.LenMsb = (uint8_t)(ATOMICTX >> 8);
        mystruct.Len = (uint8_t)(ATOMICTX - ((ATOMICTX >> 8) << 8));
        mystruct.Address = address;

        for (i = 0; i < ATOMICTX; i++) {
            mystruct.Value[i] = data[i + cptalc];
        }

        SendCmd_linux(mystruct, fd);
        ReceiveAns_linux(&mystrctAns, fd);
        sizei = sizei - ATOMICTX;
        cptalc = cptalc + ATOMICTX;
    }

    /*end of the transfer*/
    if (sizei > 0) {
        if (size <= ATOMICTX) {
            mystruct.Cmd = 'a'; //  end part of big packet
        } else {
            mystruct.Cmd = 'z';   // case short packet
        }
        mystruct.LenMsb = (uint8_t)(sizei >> 8);
        mystruct.Len = (uint8_t)(sizei - ((sizei >> 8) << 8));
        mystruct.Address = address;
        for (i = 0; i < ((mystruct.LenMsb << 8) + mystruct.Len); i++) {
            mystruct.Value[i] = data[i + cptalc];
        }

        SendCmd_linux(mystruct, fd);
        if (ReceiveAns_linux(&mystrctAns, fd)) {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_SUCCESS;
        } else {
            DEBUG_MSG("ERROR: USB READ FAILURE\n");
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
    }
    DEBUG_MSG("ERROR: USB READ FAILURE\n");
    return LGW_COM_ERROR; //never reach
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) read */
int lgw_com_rb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    int sizei = size;
    int cptalc = 0;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    pthread_mutex_lock(&mx_usbbridgesync);
    while (sizei > ATOMICRX) {
        if (sizei == size) {
            mystruct.Cmd = 's';
        } else {
            mystruct.Cmd = 't';
        }
        mystruct.LenMsb = 0;
        mystruct.Len = 2;
        mystruct.Value[0] = (uint8_t)(ATOMICRX >> 8);
        mystruct.Value[1] = (uint8_t)(ATOMICRX - ((ATOMICRX >> 8) << 8));
        mystruct.Address = address;
        SendCmd_linux(mystruct, fd);
        if (ReceiveAns_linux(&mystrctAns, fd)) {
            for (i = 0; i < ATOMICRX; i++) {
                data[i + cptalc] = mystrctAns.Rxbuf[i];
            }
        } else {

            for (i = 0; i < ATOMICRX; i++) {
                data[i + cptalc] = 0xFF;
            }
        }

        sizei = sizei - ATOMICRX;
        cptalc = cptalc + ATOMICRX;
    }
    if (sizei > 0) {
        if (size <= ATOMICRX) {
            mystruct.Cmd = 'p';
        } else {
            mystruct.Cmd = 'u';
        }
        mystruct.LenMsb = 0;
        mystruct.Len = 2;
        mystruct.Value[0] = (uint8_t)(sizei >> 8);
        mystruct.Value[1] = (uint8_t)(sizei - ((sizei >> 8) << 8));
        mystruct.Address = address;

        DEBUG_MSG("Note: usb send cmd readburst success\n");
        SendCmd_linux(mystruct, fd);

        if (ReceiveAns_linux(&mystrctAns, fd)) {
            DEBUG_PRINTF("mystrctAns = %x et %x \n", mystrctAns.Len, mystrctAns.LenMsb);
            for (i = 0; i < sizei; i++) {
                data[i + cptalc] = mystrctAns.Rxbuf[i];
            }
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_SUCCESS;
        } else {
            DEBUG_MSG("ERROR: Cannot readburst stole  \n");
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
    } else {
        return LGW_COM_ERROR;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_receive_cmd_linux(void *com_target, uint8_t max_packet, uint8_t *data) {
    int fd;
    int i, j;
    int pt = 0;
    int cptalc = 0;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    /* Prepare command for fetching packets */
    mystruct.Cmd = 'b';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = max_packet;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
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

int lgw_rxrf_setconfcmd_linux(void *com_target, uint8_t rfchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'c';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = rfchain;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_boardconfcmd_linux(void * com_target, uint8_t *data, uint16_t size)
{
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'i';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_rxif_setconfcmd_linux(void *com_target, uint8_t ifchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'd';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = ifchain;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_txgain_setconfcmd_linux(void *com_target, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'h';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];

    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_sendconfcmd_linux(void *com_target, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'f';
    mystruct.LenMsb = (uint8_t)(size >> 8);
    mystruct.Len = (uint8_t)(size - ((size >> 8) << 8));
    mystruct.Address = 0;
    for (i = 0; i < size; i++) {
        mystruct.Value[i] = data[i];
    }

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_trigger_linux(void *com_target, uint8_t address, uint32_t *data) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'q';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = address;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    *data = (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]);
    DEBUG_PRINTF("sx1301 counter %d\n", (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]));

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_calibration_snapshot_linux(void *com_target)
{
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'j';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_resetSTM32_linux(void *com_target) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'm';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_GOTODFU_linux(void *com_target) {
    int fd;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input variables */
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    mystruct.Cmd = 'n';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Address = 0;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd) == KO) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_GetUniqueId_linux(void * com_target, uint8_t * uid) {
    int fd;
    int i;
    int fwversion = STM32FWVERSION;

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    mystruct.Cmd = 'l';
    mystruct.LenMsb = 0;
    mystruct.Len = 4;
    mystruct.Address = 0;
    mystruct.Value[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
    mystruct.Value[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
    mystruct.Value[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
    mystruct.Value[3] = (uint8_t)((fwversion) & (0x000000ff));

    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmd_linux(mystruct, fd);
    if (ReceiveAns_linux(&mystrctAns, fd)) {
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
