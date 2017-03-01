/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
(C)2016 Semtech


Maintainer: Fabien Holin
*/

#include "mbed.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "Registers1301.h"
#include "string.h"
#include "CmdUSB.h"
#include "board.h"



USBMANAGER Usbmanager;

void Error_Handler(void);


#if DEBUG_MAIN == 0
#define DEBUG_MSG(str)                pc.printf(str)
#define DEBUG_PRINTF(fmt, args...)    pc.printf("%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define DEBUG_ARRAY(a,b,c)           for(a=0;a!=0;){}
#define CHECK_NULL(a)                if(a==NULL){return LGW_HAL_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define DEBUG_ARRAY(a,b,c)            for(a=0;a!=0;){}
#define CHECK_NULL(a)                 if(a==NULL){return LGW_HAL_ERROR;}
#endif

void Error_Handler(void)
{
	DEBUG_MSG("error\n");
}



int main(void)
{
	pc.baud(115200);
	int i;
	Timer t;
	uint16_t size;

	//init 	
	Sx1301.init();
	Sx1301.SelectPage(0);
	Usbmanager.init();
	Usbmanager.initBuf();
	wait_ms(1000);



	Usbmanager.count = 1; // wait for an 64 bytes transfer
	Usbmanager.ReceiveCmd();
	while (1) {
		while (Usbmanager.count > 0) {// wait until it usbcmd rx 
		}
		Usbmanager.count = 1;
		if (Usbmanager.DecodeCmd()) {	// decode cmd from Host
			size = (Usbmanager.BufToRasp[1] << 8) + Usbmanager.BufToRasp[2] + 3;
			for (i = 0; i < size; i++)
			{
				Usbmanager.BufToRasptemp[i] = Usbmanager.BufToRasp[i];
			}
			while (CDC_Transmit_FS(Usbmanager.BufToRasptemp, (uint16_t)((Usbmanager.BufToRasptemp[1] << 8) + Usbmanager.BufToRasptemp[2] + 3)) != USBD_OK) // transmit answer to Host
			{
			}
		}
		Usbmanager.initBuf(); // clean buffer
		Usbmanager.ReceiveCmd();
	}

	DEBUG_MSG("End never reach\n");
	wait_ms(1);
}




