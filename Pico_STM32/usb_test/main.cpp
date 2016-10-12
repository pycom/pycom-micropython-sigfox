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

USBMANAGER Usbmanager;
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);

void Error_Handler(void)
{
    pc.printf("error\n")  ;
}



int main(void)
{
	pc.baud(115200);
int i;
int j=0;
pc.printf ("start done \n");
Sx1301.init();
Sx1301.SelectPage(0);
Usbmanager.init(); 
Usbmanager.initBuf();
for (i=0;i<128;i++)
{
	 pc.printf ("Reg[%d]=0x%.2x   Page 0 \n",i,Sx1301.spiRead(i));
}
for (i=0;i<128;i++)
{
Sx1301.spiWrite(LGW_IF_FREQ_0,i);
 pc.printf ("Reg[%d]=0x%.2x   Page 0 \n",LGW_IF_FREQ_0,Sx1301.spiRead(LGW_IF_FREQ_0));
}

Sx1301.spiWrite(LGW_IF_FREQ_0,0xAA);
 pc.printf ("Reg[%d]=0x%.2x   Page 0 \n",LGW_IF_FREQ_0,Sx1301.spiRead(LGW_IF_FREQ_0));
	Timer t;
int tread=0;
int tread1=0;
uint8_t BufFromRasptemp[BUFFERRXUSBMANAGER];
uint32_t receivelength[5] ;

while(Usbmanager.ReceiveCmd()){
		t.start();
		tread=t.read_us();
		if (Usbmanager.BufFromRasptemp[0]>0){// Receive cmd from Host 
				//pc.printf("msg %s \n",Usbmanager.BufFromRasptemp);
		 	if (Usbmanager.DecodeCmd()){	// decode cmd from Host
			   while (CDC_Transmit_FS(Usbmanager.BufToRasp, Usbmanager.BufToRasp[2]+3)!=USBD_OK) // transmit answer to Host
					{
					//pc.printf("usb error \n");
					}
			}
			Usbmanager.initBuf(); // clean buffer
			t.stop();
		}
}
pc.printf("fin\n");
wait_ms(1);
}



	/** System Clock Configuration
	*/
