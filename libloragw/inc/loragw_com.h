/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Host specific functions to address the LoRa concentrator registers through a
    SPI interface.
    Single-byte read/write and burst read/write.
    Does not handle pagination.
    Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


#ifndef _LORAGW_com_H
#define _LORAGW_com_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>        /* C99 types*/

#include "config.h"    /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_com_SUCCESS     0
#define LGW_com_ERROR       -1
#define LGW_BURST_CHUNK     1024

#define LGW_com_MUX_MODE0   0x0     /* No FPGA */
#define LGW_com_MUX_MODE1   0x1     /* FPGA, with spi mux header */

#define LGW_com_MUX_TARGET_SX1301   0x0
#define LGW_com_MUX_TARGET_FPGA     0x1
#define LGW_com_MUX_TARGET_EEPROM   0x2
#define LGW_com_MUX_TARGET_SX127X   0x3
#define ACK_KO   0
#define OK 1
#define KO 0

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator SPI setup (configure I/O and peripherals)
@param com_target_ptr pointer on a generic pointer to SPI target (implementation dependant)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/

int lgw_com_open(void **com_target_ptr);

/**
@brief LoRa concentrator SPI close
@param com_target generic pointer to SPI target (implementation dependant)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/

int lgw_com_close(void *com_target);

/**
@brief LoRa concentrator SPI single-byte write
@param com_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_w(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data);

/**
@brief LoRa concentrator SPI single-byte read
@param com_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_r(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa concentrator SPI burst (multiple-byte) write
@param com_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_wb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa concentrator SPI burst (multiple-byte) read
@param com_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_rb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);



/*usb picogw TBD documentation*/

#define BURSTSIZE 1024
#define BUFFERTXSIZE 4*(BURSTSIZE+2)
#define BUFFERRXSIZE 2048
#define ATOMICTX 600
#define ATOMICRX 900
typedef struct
{
    char Cmd; // w for write , r for read
    int LenMsb;
    int Len;   // size of valid adresses . Example for a simple spi write set Len to 1 for a burst of 4 spi writes set Len = 4
    int Adress;
    int Value[BURSTSIZE];
} CmdSettings_t;

typedef struct
{
    int Cmd; // w for write , r for read
    int Id;
    int Len;   // size of valid adresses . Example for a simple spi write set Len to 1 for a burst of 4 spi writes set Len = 4
    int Rxbuf[BUFFERRXSIZE];
} AnsSettings_t;
int SendCmd(CmdSettings_t CmdSettings,int file1)     ;
int SendCmdn(CmdSettings_t CmdSettings,int file1)     ;
int ReceiveAns(AnsSettings_t *Ansbuffer,int file1)     ;
int ReceiveAnsCmd(AnsSettings_t *Ansbuffer,int file1,uint8_t cmd);
void WriteBurstRegister(int file1,int adress,int *value,int size);
int set_interface_attribs (int fd, int speed, int parity);
void set_blocking (int fd, int should_block);
int lgw_receive_cmd(void *com_target, uint8_t max_packet, uint8_t *data);
int lgw_rxrf_setconfcmd(void *com_target, uint8_t rfchain, uint8_t *data,uint16_t size);
int lgw_rxif_setconfcmd(void *com_target, uint8_t ifchain, uint8_t *data,uint16_t size);
int checkcmd(uint8_t cmd);
int lgw_txgain_setconfcmd(void *com_target, uint8_t *data,uint16_t size);
int lgw_sendconfcmd(void *com_target,uint8_t *data,uint16_t size);
int lgw_trigger(void *com_target, uint8_t address, uint32_t *data);
int lgw_boardconfcmd(void * com_target,uint8_t *data,uint16_t size);
int lgw_calibration_snapshot(void * com_target);
int lgw_resetSTM32(void * com_target);
int lgw_GOTODFU(void * com_target);
int lgw_GetUniqueId(void * com_target,uint8_t * uid);
#endif

/* --- EOF ------------------------------------------------------------------ */
