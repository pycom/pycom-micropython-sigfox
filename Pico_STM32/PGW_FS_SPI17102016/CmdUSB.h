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
#include "SX1301.h"

#define BUFFERRXUSBMANAGER 8200
#define BUFFERTXUSBMANAGER 8200
#define CMDLENGTH          800 

typedef struct
{
	char Cmd;
	int Id;
	int Len;
	int Adress;
	int Value[CMDLENGTH];
}CmdSettings_t;

class USBMANAGER //
{
public:
    
   USBMANAGER();
   
    virtual bool   init();
    virtual bool  initBuf();
    int ReceiveCmd();
    int DecodeCmd();
    int TransmitAnswer();
     uint8_t BufFromRasp[BUFFERRXUSBMANAGER];
     uint8_t BufFromRasptemp[BUFFERRXUSBMANAGER];
     uint8_t BufToRasp[BUFFERRXUSBMANAGER];
     uint8_t BufToRasptemp[BUFFERRXUSBMANAGER];
    uint32_t receivelength[5] ;
     uint32_t count;
    CmdSettings_t cmdSettings_FromRasp;
  
private:
     int Convert2charsToByte(uint8_t a,uint8_t b); 
     
};
extern USBMANAGER Usbmanager;
extern SX1301 Sx1301;
extern Serial pc;



#endif
