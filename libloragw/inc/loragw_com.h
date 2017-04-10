/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
Host specific functions to address the LoRa concentrator registers through a
USB interface. USB CDC driver is required to establish the connection with the
Picocell Gateway.
Single-byte read/write and burst read/write.

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _LORAGW_com_H
#define _LORAGW_com_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types*/

#include "config.h"     /* library configuration options (dynamically generated) */

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
#define BURSTSIZE 1024
#define BUFFERTXSIZE 4*(BURSTSIZE+2)
#define BUFFERRXSIZE 2048
#define ATOMICTX 600
#define ATOMICRX 900

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

typedef struct {
    char Cmd; // w for write , r for read
    int LenMsb;
    int Len;   // size of valid adresses . Example for a simple spi write set Len to 1 for a burst of 4 spi writes set Len = 4
    int Adress;
    int Value[BURSTSIZE];
} CmdSettings_t;

typedef struct {
    int Cmd; // w for write , r for read
    int Id;
    int Len;   // size of valid adresses . Example for a simple spi write set Len to 1 for a burst of 4 spi writes set Len = 4
    int Rxbuf[BUFFERRXSIZE];
} AnsSettings_t;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator USB setup 
@param com_target_ptr pointer on a generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/

int lgw_com_open(void **com_target_ptr);

/**
@brief LoRa concentrator USB close
@param com_target generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/

int lgw_com_close(void *com_target);

/**
@brief LoRa usb bridge to spi sx1308 concentrator single-byte write
@param com_target generic pointer to USB target 
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/


int lgw_com_w(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data);

/**
@brief  LoRa usb bridge to spi sx1308 concentrator single-byte read
@param com_target generic pointer to USB target 
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_r(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa usb bridge to spi sx1308 concentrator  burst (multiple-byte) write
@param com_target generic pointer to USB target 
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_wb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa usb bridge to spi sx1308 concentrator burst (multiple-byte) read
@param com_target generic pointer to USB target 
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_com_rb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size);

/**
@brief LoRa usb receive packet cmd for sx1308 concentrator 
@param com_target generic pointer to USB target 
@param max_packet : nbr of max receive packets
@param data pointer to byte array that will be written by the LoRa concentrator
@return nbr of current receive packets
*/
int lgw_receive_cmd(void *com_target, uint8_t max_packet, uint8_t *data);
/**
@brief LoRa usb cmd to configure rf chain for LoRa concentrator 
@param com_target generic pointer to USB target 
@param rfchain : rfchain id 
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_rxrf_setconfcmd(void *com_target, uint8_t rfchain, uint8_t *data,uint16_t size);
/**
@brief LoRa usb cmd to configure if chain for LoRa concentrator 
@param com_target generic pointer to USB target 
@param ifchain : rfchain id 
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_rxif_setconfcmd(void *com_target, uint8_t ifchain, uint8_t *data,uint16_t size);
/**
@brief LoRa usb cmd to configure tx gain for LoRa concentrator 
@param com_target generic pointer to USB target 
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_txgain_setconfcmd(void *com_target, uint8_t *data,uint16_t size);
/**
@brief LoRa usb cmd to configure  LoRa concentrator 
@param com_target generic pointer to USB target 
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_sendconfcmd(void *com_target,uint8_t *data,uint16_t size);
/**
@brief LoRa usb cmd to configure trigger of the LoRa concentrator 
@param com_target generic pointer to USB target 
@param data pointer to byte array that will be read from the LoRa concentrator
@param address 7-bit register address
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_trigger(void *com_target, uint8_t address, uint32_t *data);
/**
@brief LoRa usb cmd to configure  the board of the LoRa concentrator 
@param com_target generic pointer to USB target 
@param data pointer to byte array that will be written to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_boardconfcmd(void * com_target,uint8_t *data,uint16_t size);
/**
@brief LoRa usb cmd to store calibration parameters to the LoRa concentrator 
@param com_target generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_calibration_snapshot(void * com_target);
/**
@brief LoRa usb cmd to reset STM32 of the LoRa concentrator 
@param com_target generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_resetSTM32(void * com_target);
/**
@brief LoRa usb cmd to set STM32 in DFU mode 
@param com_target generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_GOTODFU(void * com_target);
/**
@brief LoRa usb cmd to get the unique id of STM32 
@param com_target generic pointer to USB target 
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int lgw_GetUniqueId(void * com_target,uint8_t * uid);

/**
@brief LoRa usb cmd to check the cmd response coming from the stm32
@param cmd : cmd id
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int checkcmd(uint8_t cmd);
/**
@brief LoRa usb cmd to send a cmd to the stm32
@param CmdSettings_t : cmd structure 
param com_target : USB target
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int SendCmdn(CmdSettings_t CmdSettings,int fd);
/**
@brief LoRa usb cmd to receive a cmd response from the stm32
@param CmdSettings_t : cmd structure 
@param fd : USB target
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int ReceiveAns(AnsSettings_t *Ansbuffer,int fd);
/**
@brief configure the usb com 
@param fd : USB target
@param speed : speed com port configuration
@param parity : parity com port configuration
@return status of register operation (LGW_com_SUCCESS/LGW_com_ERROR)
*/
int set_interface_attribs (int fd, int speed, int parity);
/**
@brief configure the usb com 
@param fd : USB target
@param should_block : blocking wainting a usb com response
@param parity : parity com port configuration
@return 
*/
void set_blocking (int fd, int should_block);
#endif

/* --- EOF ------------------------------------------------------------------ */
