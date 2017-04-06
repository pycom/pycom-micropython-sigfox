/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2016 Semtech


Maintainer: Fabien Holin
*/
#ifndef USBMANAGER_H
#define USBMANAGER_H
#include "mbed.h"
#include "SX1308.h"

#define BUFFERRXUSBMANAGER 2200
#define BUFFERTXUSBMANAGER 2200
#define CMDLENGTH          800 
#define CMD_ERROR 				 0
#define CMD_OK 						 1
#define CMD_K0						 0
#define ACK_OK 						 1
#define ACK_K0 						 0
#define FWVERSION          0x23040900
typedef struct
{
	char Cmd;
	int LenMsb;
	int Len;
	int Adress;
	int Value[CMDLENGTH];
}CmdSettings_t;

class USBMANAGER //
{
public:
	USBMANAGER();
	virtual void   init();
	virtual void  initBuffromhost();
  virtual void  initBuftohost();
	void ReceiveCmd();
	int DecodeCmd();
	int TransmitAnswer();
	uint8_t BufFromHost[BUFFERRXUSBMANAGER];
	uint8_t BufFromHosttemp[64];
	uint8_t BufToHost[BUFFERRXUSBMANAGER];
	//uint8_t BufToHosttemp[BUFFERRXUSBMANAGER];
	uint32_t receivelength[5];
	uint32_t count;
	CmdSettings_t cmdSettings_FromHost;

private:
	int Convert2charsToByte(uint8_t a, uint8_t b);

};
extern USBMANAGER Usbmanager;
#endif

