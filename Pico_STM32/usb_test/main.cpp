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
// DigitalOut DEBUGPIN(PB_6);

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
    DEBUG_MSG("error\n")  ;
}



int main(void)
{
	
	pc.baud(115200);
int i;
int j=0;
DEBUG_MSG ("start done \n");
//Sx1301.spiWrite( 0, 0x80); /* 1 -> SOFT_RESET bit */
//wait_ms(1);
Sx1301.init();
Sx1301.SelectPage(0);
Usbmanager.init(); 
Usbmanager.initBuf();
	
//DEBUGPIN=0;	
for (i=0;i<128;i++)
{
	Sx1301.spiRead(i);
	//wait_ms(1);
	DEBUG_PRINTF ("Register[%d]=0x%.2x   Page 0 \n",i,Sx1301.spiRead(i));
}
for (i=0;i<128;i++)
{
Sx1301.spiWrite(LGW_IF_FREQ_0,i);
//	wait_ms(1);
 DEBUG_PRINTF ("Reg[%d]=0x%.2x   Page 0 \n",LGW_IF_FREQ_0,Sx1301.spiRead(LGW_IF_FREQ_0));
}

Sx1301.spiWrite(LGW_IF_FREQ_0,0xAA);
//wait_ms(1);	
DEBUG_PRINTF("Reg[%d]=0x%.2x   Page 0 \n",LGW_IF_FREQ_0,Sx1301.spiRead(LGW_IF_FREQ_0));


Timer t;
int tread=0;
int tread1=0;
uint8_t BufFromRasptemp[BUFFERRXUSBMANAGER];
uint32_t receivelength[5] ;
 uint16_t size;
j=0;
/*while(1)
{
	for (i=0;i<864;i++)
				{
					Usbmanager.BufToRasptemp[i]=i+54*j;
				}
				size=154;
				Usbmanager.BufToRasptemp[1]=size>>8;
				Usbmanager.BufToRasptemp[2]=size-((size>>8)<<8);
  while (CDC_Transmit_FS(Usbmanager.BufToRasptemp, size+3)!=USBD_OK) // transmit answer to Host
 
	wait(10);
		
}*/
Usbmanager.count=1; // wait for an 64 bytes transfer
Usbmanager.ReceiveCmd();
	while(1){
			     while(Usbmanager.count>0){//pc.printf("wait it \n");wait(0.5);
						 }
					 
					 Usbmanager.count=1;
						/*for(i=0;i<64;i++)
						{
						pc.printf(" %d",Usbmanager.BufFromRasptemp[i]);
						}
						pc.printf("\n");*/
						if (Usbmanager.DecodeCmd()){	// decode cmd from Host
							size =(Usbmanager.BufToRasp[1]<<8)+Usbmanager.BufToRasp[2]+3;
							for (i=0;i<size;i++)
							{
								Usbmanager.BufToRasptemp[i]=Usbmanager.BufToRasp[i];
							}
			

				while (CDC_Transmit_FS(Usbmanager.BufToRasptemp, (uint16_t)((Usbmanager.BufToRasptemp[1]<<8)+Usbmanager.BufToRasptemp[2]+3))!=USBD_OK) // transmit answer to Host
 // while (CDC_Transmit_FS(Usbmanager.BufToRasptemp, 24+3)!=USBD_OK) // transmit answer to Host
				
				 {
					//pc.printf("usb error \n");
					}
			}
			Usbmanager.initBuf(); // clean buffer
		   Usbmanager.ReceiveCmd();		
		}

DEBUG_MSG("fin\n");
wait_ms(1);
}



	/** System Clock Configuration
	*/
