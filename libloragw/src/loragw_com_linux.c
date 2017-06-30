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

#include "loragw_com.h"
#include "loragw_com_linux.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_mcu.h"

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

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

extern pthread_mutex_t mx_usbbridgesync;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

int set_interface_attribs_linux(int fd, int speed) {
    struct termios tty;

    memset(&tty, 0, sizeof tty);

    /* Get current attributes */
    if (tcgetattr(fd, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcgetattr failed with %d - %s", errno, strerror(errno));
        return LGW_COM_ERROR;
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
    tty.c_oflag &= ~IGNBRK;
    tty.c_oflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    /* Local Modes */
    tty.c_lflag = 0;
    /* Settings for non-canonical mode */
    tty.c_cc[VMIN] = 0;                         /* non-blocking mode */
    tty.c_cc[VTIME] = 50;                       /* wait for (n * 0.1) seconds before returning */

    /* Set attributes */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcsetattr failed with %d - %s", errno, strerror(errno));
        return LGW_COM_ERROR;
    }

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* configure TTYACM0 read blocking or not*/
int set_blocking_linux(int fd, bool blocking) {
    struct termios tty;

    memset(&tty, 0, sizeof tty);

    /* Get current attributes */
    if (tcgetattr(fd, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcgetattr failed with %d - %s", errno, strerror(errno));
        return LGW_COM_ERROR;
    }

    tty.c_cc[VMIN] = (blocking == true) ? 1 : 0;    /* set blocking or non-blocking mode */
    tty.c_cc[VTIME] = 1;                            /* wait for (n * 0.1) seconds before returning */

    /* Set attributes */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        DEBUG_PRINTF("ERROR: tcsetattr failed with %d - %s", errno, strerror(errno));
        return LGW_COM_ERROR;
    }

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool checkcmd_linux(uint8_t cmd) {
    switch (cmd) {
        case 'r': /* read register */
        case 's': /* read burst - first chunk */
        case 't': /* read burst - middle chunk */
        case 'u': /* read burst - end chunk */
        case 'p': /* read burst - atomic */
        case 'w': /* write register */
        case 'x': /* write burst - first chunk */
        case 'y': /* write burst - middle chunk */
        case 'z': /* write burst - end chunk */
        case 'a': /* write burst - atomic */
        case 'b': /* lgw_receive */
        case 'c': /* lgw_rxrf_setconf */
        case 'd': /* lgw_rxif_setconf */
        case 'f': /* lgw_send */
        case 'h': /* lgw_txgain_setconf */
        case 'q': /* lgw_get_trigcnt */
        case 'i': /* lgw_board_setconf */
        case 'j': /* lgw_calibration_snapshot */
        case 'l': /* lgw_check_fw_version */
        case 'm': /* reset STM32 */
        case 'n': /* Go to bootloader */
            return true;
        default:
            return false;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_send_cmd_linux(lgw_com_cmd_t cmd, lgw_handle_t handle) {
    int i;
    uint8_t buffertx[CMD_HEADER_TX_SIZE + CMD_DATA_TX_SIZE];
    uint16_t Clen = cmd.len_lsb + (cmd.len_msb << 8);
    uint16_t Tlen = CMD_HEADER_TX_SIZE + Clen;
    ssize_t lencheck;

    /* Initialize buffer */
    memset(buffertx, 0, sizeof buffertx);

    /* Prepare command */
    buffertx[0] = (uint8_t)cmd.id;
    buffertx[1] = cmd.len_msb;
    buffertx[2] = cmd.len_lsb;
    buffertx[3] = cmd.address;
    for (i = 0; i < Clen; i++) {
        buffertx[i + 4] = cmd.cmd_data[i];
    }

    /* Send command */
    lencheck = write(handle, buffertx, Tlen);
    if (lencheck < 0) {
        DEBUG_PRINTF("ERROR: failed to write cmd (%d - %s)\n", errno, strerror(errno));
        return LGW_COM_ERROR;
    }
    if (lencheck != Tlen) {
        DEBUG_PRINTF("WARNING: incomplete cmd written (%d)\n", (int)lencheck);
    }

    DEBUG_PRINTF("Note: sent cmd \'%c\', addr 0x%02X, length=%d\n", cmd.id, cmd.address, Clen);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_receive_ans_linux(lgw_com_ans_t *ans, lgw_handle_t handle) {
    int i;
    uint8_t bufferrx[CMD_HEADER_RX_SIZE + CMD_DATA_RX_SIZE];
    unsigned int buffer_idx;
    size_t cmd_size;
    ssize_t buf_size = 0;
    ssize_t lencheck;

    /* Initialize variables */
    memset(bufferrx, 0, sizeof bufferrx);

    /* Wait for cmd answer header */
    buffer_idx = 0;
    while ((checkcmd_linux(bufferrx[0]) != true) || (buffer_idx < CMD_HEADER_RX_SIZE)) {
        lencheck = read(handle, &bufferrx[buffer_idx], CMD_HEADER_RX_SIZE - buffer_idx);
        if (lencheck < 0) {
            DEBUG_PRINTF("WARNING: failed to read from communication bridge (%d - %s), retry...\n", errno, strerror(errno));
            return LGW_COM_ERROR;
        }
        buffer_idx += lencheck;
    }

    cmd_size = (bufferrx[1] << 8) + bufferrx[2];

    DEBUG_PRINTF("Note: received answer header for cmd \'%c\', length=%zd, ack=%u\n", bufferrx[0], cmd_size, bufferrx[3]);

    /* Read answer Data */
    if (cmd_size > 0) {
        /* Determine how much we need to read */
        buf_size = cmd_size + CMD_HEADER_RX_SIZE;
        if ((buf_size % 64) == 0) {
            cmd_size = cmd_size + 1; /* one padding byte is added by USB driver, we need to read it */
        }
        /* Check that data size does not exceed buffer size */
        if (cmd_size > CMD_DATA_RX_SIZE) {
            DEBUG_PRINTF("ERROR: exceed read buffer size, abort. (%zd)\n", cmd_size);
            return LGW_COM_ERROR;
        }
        /* Read the answer */
        buffer_idx = 0;
        while (buffer_idx < cmd_size) {
            lencheck = read(handle, &bufferrx[CMD_HEADER_RX_SIZE + buffer_idx], cmd_size - buffer_idx);
            if (lencheck < 0) {
                DEBUG_PRINTF("ERROR: failed to read cmd answer (%d - %s)\n", errno, strerror(errno));
                return LGW_COM_ERROR;
            }
            buffer_idx += lencheck;
        }
    }
    ans->id = (char)bufferrx[0];
    ans->len_msb = bufferrx[1];
    ans->len_lsb = bufferrx[2];
    ans->status = bufferrx[3];
    for (i = 0; i < (int)cmd_size; i++) {
        ans->ans_data[i] = bufferrx[CMD_HEADER_RX_SIZE + i];
    }

    DEBUG_PRINTF("Note: received answer for cmd \'%c\', length=%zd\n", bufferrx[0], cmd_size);

    return LGW_COM_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_com_open_linux(void **com_target_ptr, const char *com_path) {

    int *usb_device = NULL;
    char portname[50];
    int x;
    int fd;

    /*check input variables*/
    CHECK_NULL(com_target_ptr);

    usb_device = malloc(sizeof(int));
    if (usb_device == NULL) {
        DEBUG_MSG("ERROR : MALLOC FAIL\n");
        return LGW_COM_ERROR;
    }

    /* open tty port */
    sprintf(portname, "%s", com_path);
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("ERROR: failed to open COM port %s - %s\n", portname, strerror(errno));
    } else {
        x = set_interface_attribs_linux(fd, B115200);
        x |= set_blocking_linux(fd, true);
        if (x != 0) {
            printf("ERROR: failed to configure COM port %s\n", portname);
            free(usb_device);
            return LGW_COM_ERROR;
        }

        *usb_device = fd;
        *com_target_ptr = (void*)usb_device;

        return LGW_COM_SUCCESS;
    }

    free(usb_device);
    return LGW_COM_ERROR;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_close_linux(void *com_target) {
    int usb_device;
    int a;

    /*check input variables*/
    CHECK_NULL(com_target);

    /* close file & deallocate file descriptor */
    usb_device = *(int*)com_target;
    a = close(usb_device);
    free(com_target);

    /* determine return code */
    if (a < 0) {
        printf("ERROR: failed to close COM port - %s\n", strerror(errno));
        return LGW_COM_ERROR;
    } else {
        DEBUG_MSG("Note : USB port closed\n");
        return LGW_COM_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_w_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data) {
    int fd;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /*check input variables*/
    CHECK_NULL(com_target);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    cmd.id = 'w';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = address;
    cmd.cmd_data[0] = data;

    pthread_mutex_lock(&mx_usbbridgesync);
    lgw_com_send_cmd_linux(cmd, fd);
    if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_ERROR) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_r_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data) {
    int fd;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    cmd.id = 'r';
    cmd.len_msb = 0;
    cmd.len_lsb = 1;
    cmd.address = address;
    cmd.cmd_data[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    lgw_com_send_cmd_linux(cmd, fd);
    if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_ERROR) {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    pthread_mutex_unlock(&mx_usbbridgesync);

    *data = ans.ans_data[0];

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_wb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    uint16_t chunk_size = size;
    int cptalc = 0;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target;

    /* lock for complete burst */
    pthread_mutex_lock(&mx_usbbridgesync);

    /* Split burst in multiple chunks if necessary */
    while (chunk_size > ATOMICTX) {
        /* Prepare command */
        if (chunk_size == size) {
            cmd.id = 'x'; /* write burst - first */
        } else {
            cmd.id = 'y'; /* write burst - middle */
        }
        cmd.len_msb = (uint8_t)((ATOMICTX >> 8) & 0xFF);
        cmd.len_lsb = (uint8_t)((ATOMICTX >> 0) & 0xFF);
        cmd.address = address;
        for (i = 0; i < ATOMICTX; i++) {
            cmd.cmd_data[i] = data[i + cptalc];
        }

        /* Send command */
        lgw_com_send_cmd_linux(cmd, fd);
        if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_ERROR) {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }

        chunk_size = chunk_size - ATOMICTX;
        cptalc = cptalc + ATOMICTX;
    }

    /* Complete multiple-chunk transfer, or send atomic one */
    if (chunk_size > 0) {
        /* Prepare command */
        if (size <= ATOMICTX) {
            cmd.id = 'a'; /* write burst - atomic */
        } else {
            cmd.id = 'z'; /* write burst - end */
        }
        cmd.len_msb = (uint8_t)((chunk_size >> 8) & 0xFF);
        cmd.len_lsb = (uint8_t)((chunk_size >> 0) & 0xFF);
        cmd.address = address;
        for (i = 0; i < ((cmd.len_msb << 8) + cmd.len_lsb); i++) {
            cmd.cmd_data[i] = data[i + cptalc];
        }

        /* Send command */
        lgw_com_send_cmd_linux(cmd, fd);
        if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_ERROR) {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
    } else {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

    /* unlock burst */
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_rb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    uint16_t chunk_size = size;
    int cptalc = 0;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    UNUSED(com_mux_mode);
    UNUSED(com_mux_target);

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target;

    /* lock for complete burst */
    pthread_mutex_lock(&mx_usbbridgesync);

    /* Split burst in multiple chunks if necessary */
    while (chunk_size > ATOMICRX) {
        /* Prepare command */
        if (chunk_size == size) {
            cmd.id = 's'; /* read burst - first */
        } else {
            cmd.id = 't'; /* read burst - middle */
        }
        cmd.len_msb = 0;
        cmd.len_lsb = 2;
        cmd.cmd_data[0] = (uint8_t)((ATOMICRX >> 8) & 0xFF);
        cmd.cmd_data[1] = (uint8_t)((ATOMICRX >> 0) & 0xFF);
        cmd.address = address;

        /* Send command */
        lgw_com_send_cmd_linux(cmd, fd);
        if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_SUCCESS) {
            for (i = 0; i < ATOMICRX; i++) {
                data[i + cptalc] = ans.ans_data[i];
            }
        } else {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }

        chunk_size = chunk_size - ATOMICRX;
        cptalc = cptalc + ATOMICRX;
    }

    /* Complete multiple-chunk transfer, or send atomic one */
    if (chunk_size > 0) {
        /* Prepare command */
        if (size <= ATOMICRX) {
            cmd.id = 'p'; /* read burst - atomic */
        } else {
            cmd.id = 'u'; /* read burst - end */
        }
        cmd.len_msb = 0;
        cmd.len_lsb = 2;
        cmd.cmd_data[0] = (uint8_t)((chunk_size >> 8) & 0xFF);
        cmd.cmd_data[1] = (uint8_t)((chunk_size >> 0) & 0xFF);
        cmd.address = address;

        /* Send command */
        lgw_com_send_cmd_linux(cmd, fd);
        if (lgw_com_receive_ans_linux(&ans, fd) == LGW_COM_SUCCESS) {
            for (i = 0; i < chunk_size; i++) {
                data[i + cptalc] = ans.ans_data[i];
            }
        } else {
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
    } else {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

    /* unlock burst */
    pthread_mutex_unlock(&mx_usbbridgesync);

    return LGW_COM_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
