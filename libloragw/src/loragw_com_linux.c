
/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
(C)2017 Semtech-Cycleo

Description:
this file contains the USB cmd to configure and communicate with
the Sx1308 LoRA concentrator.
An USB CDC drivers is required to establish the connection with the picogateway board.

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>        /* C99 types */
#include <stdio.h>        /* printf fprintf */
#include <stdlib.h>        /* malloc free */
#include <unistd.h>        /* lseek, close */
#include <fcntl.h>        /* open */
#include <string.h>        /* memset */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <pthread.h>
#include "loragw_com_linux.h"
#include "loragw_com.h"
#include "loragw_hal.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include <time.h>

#include <sys/select.h>
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

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

pthread_mutex_t mx_usbbridgesync = PTHREAD_MUTEX_INITIALIZER; /* control access to usbbridge sync offsets */




int
set_interface_attribs_linux(int fd, int speed, int parity)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        DEBUG_PRINTF("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN] = 0;            // read doesn't block
    tty.c_cc[VTIME] = 50;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    //  tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        DEBUG_PRINTF("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}
/* configure TTYACM0 read blocking or not*/
void set_blocking_linux(int fd, int should_block)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        DEBUG_PRINTF("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 1;            // 0.5 seconds read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        DEBUG_PRINTF("error %d setting term attributes", errno);
    }
}




/* open USB port */
int lgw_com_open_linux(void **com_target_ptr) {

    int *usb_device = NULL;
    char portname[50];
    int i;
    int fd;
    int fwversion = STM32FWVERSION;
    /*check input variables*/
    CHECK_NULL(com_target_ptr);
    usb_device = malloc(sizeof(int));
    if (usb_device == NULL) {
        DEBUG_MSG("ERROR : MALLOC FAIL\n");
        return LGW_COM_ERROR;
    }

    for (i = 0; i < 10; i++) // try to open one of the 10 port ttyACM
    {
        sprintf(portname, "/dev/ttyACM%d", i);
        fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);

        if (fd < 0)
        {
            DEBUG_PRINTF("ERROR: failed to open bridge USB  %s \n", portname);
        }
        else
        {
            set_interface_attribs(fd, B921600, 0);  // set speed to 115,200 bps, 8n1 (no parity)
            set_blocking(fd, 0);                // set  non blocking
            *usb_device = fd;
            *com_target_ptr = (void*)usb_device;
            CmdSettings_t mystruct;
            AnsSettings_t mystrctAns;

            mystruct.Cmd = 'l';
            mystruct.LenMsb = 0;
            mystruct.Len = 4;
            mystruct.Adress = 0;
            mystruct.Value[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
            mystruct.Value[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
            mystruct.Value[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
            mystruct.Value[3] = (uint8_t)((fwversion) & (0x000000ff));

            DEBUG_MSG("Note: USB write success\n");
            pthread_mutex_lock(&mx_usbbridgesync);
            SendCmdn(mystruct, fd);
            if (ReceiveAns(&mystrctAns, fd))
            {
                if (mystrctAns.Rxbuf[0] == ACK_KO) {
                    return LGW_COM_ERROR;
                }
                DEBUG_PRINTF("check fw version %d \n", mystrctAns.Rxbuf[0]);
                DEBUG_MSG("Note: USB read config success\n");
                pthread_mutex_unlock(&mx_usbbridgesync);
                return LGW_COM_SUCCESS;
            }
            else
            {
                DEBUG_MSG("ERROR: USB read config FAILED\n");
                pthread_mutex_unlock(&mx_usbbridgesync);
                return LGW_COM_ERROR;
            }
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
        DEBUG_MSG("ERROR : USB PORT FAILED TO CLOSE\n");
        return LGW_COM_ERROR;
    }
    else
    {
        DEBUG_MSG("Note : USB port closed \n");
        return LGW_COM_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
int lgw_com_w_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data) {
    int fd;

    int temp4WARNING;
    temp4WARNING = com_mux_mode;
    temp4WARNING = com_mux_target;
    temp4WARNING++;
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'w';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = address;
    mystruct.Value[0] = data;
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: usb read success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: usb READ FAILURE\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
int lgw_com_r_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data) {
    int fd;

    int temp4WARNING;
    temp4WARNING = com_mux_mode;
    temp4WARNING = com_mux_target;
    temp4WARNING++;
    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    mystruct.Cmd = 'r';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = address;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    DEBUG_MSG("Note: usb send cmd read success\n");
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: usb read success\n");
        *data = mystrctAns.Rxbuf[0];
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB READ FAILURE\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}


int lgw_com_wb_linux(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    int sizei = size;
    int cptalc = 0;
    int temp4WARNING;
    temp4WARNING = com_mux_mode;
    temp4WARNING = com_mux_target;
    temp4WARNING++;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    /* check input parameters */
    CHECK_NULL(com_target);
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    /* prepare command byte */
    pthread_mutex_lock(&mx_usbbridgesync);
    while (sizei > ATOMICTX)
    {
        if (sizei == size)
        {
            mystruct.Cmd = 'x'; //first part of big packet
        }
        else
        {
            mystruct.Cmd = 'y';   //middle part of big packet
        }

        mystruct.LenMsb = (ATOMICTX >> 8);
        mystruct.Len = ATOMICTX - ((ATOMICTX >> 8) << 8);
        mystruct.Adress = address;

        for (i = 0; i < ATOMICTX; i++)
        {
            mystruct.Value[i] = data[i + cptalc];
        }

        SendCmdn(mystruct, fd);
        ReceiveAns(&mystrctAns, fd);
        sizei = sizei - ATOMICTX;
        cptalc = cptalc + ATOMICTX;
    }
    /*end of the transfer*/
    if (sizei > 0)
    {
        if (size <= ATOMICTX)
        {
            mystruct.Cmd = 'a'; //  end part of big packet
        }
        else
        {
            mystruct.Cmd = 'z';   // case short packet
        }
        mystruct.LenMsb = (sizei >> 8);
        mystruct.Len = sizei - ((sizei >> 8) << 8);
        mystruct.Adress = address;
        for (i = 0; i < ((mystruct.LenMsb << 8) + mystruct.Len); i++)
        {

            mystruct.Value[i] = data[i + cptalc];
        }

        SendCmdn(mystruct, fd);
        if (ReceiveAns(&mystrctAns, fd))
        {
            DEBUG_MSG("Note: usb read success\n");
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_SUCCESS;
        }
        else
        {
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
    int temp4WARNING;
    temp4WARNING = com_mux_mode;
    temp4WARNING = com_mux_target;
    temp4WARNING++;
    /* check input parameters */
    CHECK_NULL(com_target);
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    int i;
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    int sizei = size;
    int cptalc = 0;
    pthread_mutex_lock(&mx_usbbridgesync);
    while (sizei > ATOMICRX)
    {
        if (sizei == size)
        {
            mystruct.Cmd = 's';
        }
        else
        {
            mystruct.Cmd = 't';
        }
        mystruct.LenMsb = 0;
        mystruct.Len = 2;
        mystruct.Value[0] = ATOMICRX >> 8;
        mystruct.Value[1] = ATOMICRX - ((ATOMICRX >> 8) << 8);
        mystruct.Adress = address;
        SendCmdn(mystruct, fd);
        if (ReceiveAns(&mystrctAns, fd))
        {
            for (i = 0; i < ATOMICRX; i++)
            {
                data[i + cptalc] = mystrctAns.Rxbuf[i];
            }
        }
        else
        {

            for (i = 0; i < ATOMICRX; i++)
            {
                data[i + cptalc] = 0xFF;
            }
        }

        sizei = sizei - ATOMICRX;
        cptalc = cptalc + ATOMICRX;

    }
    if (sizei > 0) {
        if (size <= ATOMICRX)
        {
            mystruct.Cmd = 'p';
        }
        else
        {
            mystruct.Cmd = 'u';
        }
        mystruct.LenMsb = 0;
        mystruct.Len = 2;
        mystruct.Value[0] = sizei >> 8;
        mystruct.Value[1] = sizei - ((sizei >> 8) << 8);
        mystruct.Adress = address;

        DEBUG_MSG("Note: usb send cmd readburst success\n");
        SendCmdn(mystruct, fd);

        if (ReceiveAns(&mystrctAns, fd))
        {
            DEBUG_PRINTF("mystrctAns = %x et %x \n", mystrctAns.Len, mystrctAns.Id);
            for (i = 0; i < sizei; i++)
            {
                data[i + cptalc] = mystrctAns.Rxbuf[i];
            }
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_SUCCESS;
        }
        else
        {
            DEBUG_MSG("ERROR: Cannot readburst stole  \n");
            pthread_mutex_unlock(&mx_usbbridgesync);
            return LGW_COM_ERROR;
        }
    }
    else
    {
        return LGW_COM_ERROR;
    }

}

int SendCmdn_linux(CmdSettings_t CmdSettings, int fd)
{
    char buffertx[BUFFERTXSIZE];
    int Clen = CmdSettings.Len + (CmdSettings.LenMsb << 8);
    int Tlen = 1 + 2 + 1 + Clen; // cmd  Length +adress
    int i;
    ssize_t lencheck;
    for (i = 0; i < BUFFERTXSIZE; i++)
    {
        buffertx[i] = 0;
    }
    buffertx[0] = CmdSettings.Cmd;
    buffertx[1] = CmdSettings.LenMsb;
    buffertx[2] = CmdSettings.Len;
    buffertx[3] = CmdSettings.Adress;
    for (i = 0; i < Clen; i++)
    {
        buffertx[i + 4] = CmdSettings.Value[i];

    }
    lencheck = write(fd, buffertx, Tlen);
    if (lencheck != Tlen)
    {
        DEBUG_PRINTF("WARNING : write cmd failed (%d)\n", (int) lencheck);
    }
    DEBUG_PRINTF("send burst done size %d\n", Tlen);
    return(OK);
}


int ReceiveAns_linux(AnsSettings_t *Ansbuffer, int fd )
{
    uint8_t bufferrx[BUFFERRXSIZE];
    int i;
    int cpttimer = 0;
    int sizet = 0;
    ssize_t lencheck;
    for (i = 0; i < BUFFERRXSIZE; i++)
    {
        bufferrx[i] = 0;
    }
    cpttimer = 0;

    while (checkcmd(bufferrx[0]))
    {
        lencheck = read(fd, bufferrx, 3);
        cpttimer++;
        if (lencheck != 3)
        {
            DEBUG_PRINTF("WARNING : write  read  failed (%d) time buffer 0 = %d\n", (int) cpttimer, bufferrx[0]);
        }
        if (cpttimer > 15) // wait read error the read function isn't block but timeout of 0.1s
        {
            DEBUG_MSG("WARNING : deadlock usb");
            return(OK); // deadlock
        }
    }
    wait_ns(((bufferrx[1] << 8) + bufferrx[2] + 1) * 6000);
    DEBUG_PRINTF("cmd = %d readburst size %d\n", bufferrx[0], (bufferrx[1] << 8) + bufferrx[2]);
    sizet = (bufferrx[1] << 8) + bufferrx[2] + 3;
    if ((sizet % 64) == 0) {
        sizet = sizet - 2;
    }
    else {
        sizet = sizet - 3;
    }
//lencheck = read(file1, &bufferrx[3], (bufferrx[1] << 8) + bufferrx[2]);
    lencheck = read(fd, &bufferrx[3], sizet);
    if (lencheck != (sizet))
    {
        DEBUG_PRINTF("WARNING : write  read  failed %d\n", lencheck);
    }
    Ansbuffer->Cmd = bufferrx[0];
    Ansbuffer->Id = bufferrx[1];
    Ansbuffer->Len = bufferrx[2];
    for (i = 0; i < (bufferrx[1] << 8) + bufferrx[2]; i++)
    {
        Ansbuffer->Rxbuf[i] = bufferrx[3 + i];
    }
    return(OK);
}


/*Embedded HAL into STM32 part */

int lgw_receive_cmd_linux(void *com_target, uint8_t max_packet, uint8_t *data) {
    int fd;
    int i;
    int j;
    int pt = 0;
    int resp = 0;
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */

    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    mystruct.Cmd = 'b';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = 0;
    mystruct.Value[0] = max_packet;
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);

    resp = ReceiveAns(&mystrctAns, fd);
    DEBUG_MSG("Note: usb write success\n");
    DEBUG_PRINTF("NOTE : Available packet %d  %d\n", mystrctAns.Rxbuf[0], (mystrctAns.Id << 8) + mystrctAns.Len);
    DEBUG_PRINTF("NOTE : read structure %d %d %d %d %d\n", mystrctAns.Rxbuf[5], mystrctAns.Rxbuf[6], mystrctAns.Rxbuf[7], mystrctAns.Rxbuf[8], mystrctAns.Rxbuf[9]);

    int cptalc = 0;
    if (resp == KO)
    {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return (0); // for 0 receive packet
    }

    for (i = 0; i < mystrctAns.Rxbuf[0]; i++) // over the number of packets
    {
        for (j = 0; j < mystrctAns.Rxbuf[cptalc + 43] + 44; j++) // for each packet
        {
            pt = mystrctAns.Rxbuf[cptalc + 43] + 44;
            data[(i * 300) + j] = mystrctAns.Rxbuf[j + cptalc + 1];//300 size of struct target
        }
        cptalc = pt;

    }
    pthread_mutex_unlock(&mx_usbbridgesync);
    return mystrctAns.Rxbuf[0];
}



/*Embedded HAL into STM32 part */

int lgw_rxrf_setconfcmd_linux(void *com_target, uint8_t rfchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'c';
    mystruct.LenMsb = (size >> 8);
    mystruct.Len = size - ((size >> 8) << 8);
    mystruct.Adress = rfchain;
    DEBUG_PRINTF("Note: USB write success size = %d\n", size);
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = data[i];
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
}

int lgw_boardconfcmd_linux(void * com_target, uint8_t *data, uint16_t size)

{
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'i';
    mystruct.LenMsb = (size >> 8);
    mystruct.Len = size - ((size >> 8) << 8);
    mystruct.Adress = 0;
    DEBUG_PRINTF("Note: USB write success size = %d\n", size);
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = data[i];
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}
int lgw_rxif_setconfcmd_linux(void *com_target, uint8_t ifchain, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'd';
    mystruct.LenMsb = (size >> 8);
    mystruct.Len = size - ((size >> 8) << 8);
    mystruct.Adress = ifchain;
    DEBUG_PRINTF("Note: USB write success size = %d\n", size);
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = data[i];
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
}

int lgw_txgain_setconfcmd_linux(void *com_target, uint8_t *data, uint16_t size)
{
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'h';
    mystruct.LenMsb = (size >> 8);
    mystruct.Len = size - ((size >> 8) << 8);
    mystruct.Adress = 0;
    DEBUG_PRINTF("Note: USB write success size = %d\n", size);
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = data[i];

    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
}



int lgw_sendconfcmd_linux(void *com_target, uint8_t *data, uint16_t size) {
    int fd;
    int i;
    DEBUG_MSG("Note SEND A PACKET: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;

    mystruct.Cmd = 'f';
    mystruct.LenMsb = (size >> 8);
    mystruct.Len = size - ((size >> 8) << 8);
    mystruct.Adress = 0;
    DEBUG_PRINTF("Note: USB write success size = %d\n", size);
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = data[i];
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
}



int lgw_trigger_linux(void *com_target, uint8_t address, uint32_t *data) {
    int fd;


    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    mystruct.Cmd = 'q';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = address;
    mystruct.Value[0] = 0;

    pthread_mutex_lock(&mx_usbbridgesync);
    DEBUG_MSG("Note: usb send cmd read success\n");
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: usb read success\n");
        *data = (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]);
        DEBUG_PRINTF("timestampreceive %d\n", (mystrctAns.Rxbuf[0] << 24) + (mystrctAns.Rxbuf[1] << 16) + (mystrctAns.Rxbuf[2] << 8) + (mystrctAns.Rxbuf[3]));
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB READ FAILURE\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}

int lgw_calibration_snapshot_linux(void * com_target)

{
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    int size = 1;
    mystruct.Cmd = 'j';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = 0;
    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = 0;
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}

int lgw_resetSTM32_linux(void * com_target)
{
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    int size = 1;
    mystruct.Cmd = 'm';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = 0;

    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = 0;
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}

int lgw_GOTODFU_linux(void * com_target)
{
    int fd;
    int i;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    /*build the write cmd*/
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    int size = 1;
    mystruct.Cmd = 'n';
    mystruct.LenMsb = 0;
    mystruct.Len = 1;
    mystruct.Adress = 0;

    DEBUG_MSG("Note: USB write success\n");
    for (i = 0; i < size; i++)
    {
        mystruct.Value[i] = 0;
    }
    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }

}

int lgw_GetUniqueId_linux(void * com_target, uint8_t * uid)
{
    int fd;
    int i;
    int fwversion = STM32FWVERSION;
    DEBUG_MSG("Note: USB write success\n");
    fd = *(int *)com_target; /* must check that com_target is not null beforehand */
    DEBUG_PRINTF("Note: USB write success %d\n", fd);
    CmdSettings_t mystruct;
    AnsSettings_t mystrctAns;
    mystruct.Cmd = 'l';
    mystruct.LenMsb = 0;
    mystruct.Len = 4;
    mystruct.Adress = 0;
    mystruct.Value[0] = (uint8_t)((fwversion >> 24) & (0x000000ff));
    mystruct.Value[1] = (uint8_t)((fwversion >> 16) & (0x000000ff));
    mystruct.Value[2] = (uint8_t)((fwversion >> 8) & (0x000000ff));
    mystruct.Value[3] = (uint8_t)((fwversion) & (0x000000ff));

    DEBUG_MSG("Note: USB write success\n");
    pthread_mutex_lock(&mx_usbbridgesync);
    SendCmdn(mystruct, fd);
    if (ReceiveAns(&mystrctAns, fd))
    {
        if (mystrctAns.Rxbuf[0] == ACK_KO) {
            return LGW_COM_ERROR;
        }
        for (i = 0; i < 7; i++)
        {
            uid[i] = mystrctAns.Rxbuf[i + 1];
        }
        DEBUG_MSG("Note: USB read config success\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_SUCCESS;
    }
    else
    {
        DEBUG_MSG("ERROR: USB read config FAILED\n");
        pthread_mutex_unlock(&mx_usbbridgesync);
        return LGW_COM_ERROR;
    }
    return LGW_COM_SUCCESS;
}


/****************************/
int checkcmd_linux(uint8_t cmd)
{
    switch (cmd)
    {
        case 'r': {
                return(0);
                break;
            }
        case 's': {
                return(0);
                break;
            }
        case 't': {
                return(0);
                break;
            }
        case 'u': {
                return(0);
                break;
            }
        case 'p': {
                return(0);
                break;
            }
        case 'e': {
                return(0);
                break;
            }
        case 'w': {
                return(0);
                break;
            }
        case 'x': {
                return(0);
                break;
            }
        case 'y': {
                return(0);
                break;
            }
        case 'z': {
                return(0);
                break;
            }
        case 'a': {
                return(0);
                break;
            }
        case 'b': {
                return(0);
                break;
            }
        case 'c': {
                return(0);
                break;
            }
        case 'd': {
                return(0);
                break;
            }
        case 'f': {
                return(0);
                break;
            }
        case 'h': {
                return(0);
                break;
            }
        case 'q': {
                return(0);
                break;
            }
        case 'i': {
                return(0);
                break;
            }
        case 'j': {
                return(0);
                break;
            }
        case 'l': {
                return(0);
                break;
            }
        case 'm': {
                return(0);
                break;
            }
        case 'n': {
                return(0);
                break;
            }
        //case 97 : return (1);

        default:
            return(OK);
    }
    return(OK);
}


/* --- EOF ------------------------------------------------------------------ */
