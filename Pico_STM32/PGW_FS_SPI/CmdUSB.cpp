/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2016 Semtech


Maintainer: Fabien Holin
*/
#include "CmdUSB.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
Serial pc(PB_6,PB_7);
SX1301 Sx1301(PA_4, PA_7, PA_6, PA_5, PB_1,PA_3 );

USBMANAGER::USBMANAGER()
   
{		
}

bool USBMANAGER::init()
	{ 	
		__HAL_RCC_USB_OTG_FS_CLK_ENABLE();
		MX_USB_DEVICE_Init();
		return true;
	}	
bool USBMANAGER::initBuf()
{int i;
	for (i=0;i<BUFFERRXUSBMANAGER;i++)
	{
		 BufFromRasp[i]=0;
		 BufFromRasptemp[i]=0;
	}
	for (i=0;i<BUFFERTXUSBMANAGER;i++)
	{
		 BufToRasp[i]=0;
		// BufToRasptemp[i]=0;
	}
	receivelength[0]=0;

}
int USBMANAGER::ReceiveCmd()
{ 
		CDC_Receive_FSP(&BufFromRasptemp[0], &receivelength[0]);// wait interrrupt manage in HAL_PCD_DataOutStageCallback
	return(1);
}
int USBMANAGER::DecodeCmd()
{int i=0;
	 int adressreg;
	int val; 
	    /*for(i=0;i<BUFFERRXUSBMANAGER;i++)
			{BufFromRasp[i]=BufFromRasptemp[i];
			}	*/
	    if (BufFromRasp[0]==0){return (0);}
	    cmdSettings_FromRasp.Cmd = BufFromRasp[0]; 
	   
		switch (cmdSettings_FromRasp.Cmd) {
			
			case 'r' :{ // cmd Read register 
		          		cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
								
									
									
									 val=0;
								 //	Sx1301.SelectPage(page);
									val= Sx1301.spiRead(adressreg);
									BufToRasp[0]='r';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=val;	
                  return(1);											
							break;}
			case 's' :{ // cmd Read burst register first
										int i;
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								
									BufToRasp[0]=12;
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurstF(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							
							break;}
			case 't' :{ // cmd Read burst register middle
										cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
								
									
									
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								
									BufToRasp[0]=12;
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
								//	for (i=0;i<size;i++){BufToRasp[3+i]=8;}
									Sx1301.spiReadBurstM(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'u' :{ // cmd Read burst register end
									int i;
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
								
									
									
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								
									BufToRasp[0]=12;
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=8;}
									Sx1301.spiReadBurstE(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'p' :{ // cmd Read burst register atomic
										int i;
										cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
								
									
									
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								
									BufToRasp[0]=12;
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurst(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'e' :{ // cmd Read burst register atomic
				int i;
										cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			
								
									
									
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								
									BufToRasp[0]=12;
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurst(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
				
			case 'w' :{ // cmd write register 
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}			

									 val=cmdSettings_FromRasp.Value[0];
								//	pc.printf("write register adress %d value %d \n",adressreg,val);
									//Sx1301.SelectPage(page); unused
								  Sx1301.spiWrite(adressreg,val);
								 BufToRasp[0]='w';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							break;}
			
						
							
							
			 case 'x' :{ // cmd write burst register 
										cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				          int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{
											cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstF(adressreg, &cmdSettings_FromRasp.Value[0], size);
									BufToRasp[0]='z';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;									
									return(1); //mean no ack transmission									
							break;}
			 case 'y' :{ // cmd write burst register 
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				          int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{
											cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstM(adressreg, &cmdSettings_FromRasp.Value[0], size);	
									BufToRasp[0]='z';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							break;}
			 case 'z' :{ // cmd write burst register 
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				          int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{
											cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstE(adressreg, &cmdSettings_FromRasp.Value[0], size);	
//pc.printf("wbesize = %d\n",cmdSettings_FromRasp.Len);											
									BufToRasp[0]='z';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							break;}
			 case 'a' :{ // cmd write burst atomic register 
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
									}
                  Sx1301.spiWriteBurst(adressreg, &cmdSettings_FromRasp.Value[0],size);
     				
									BufToRasp[0]='a';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							break;}
			default : break;

	}
return (1);		
}	
int  USBMANAGER::TransmitAnswer()
{
	int TransferLen=BufToRasp[2]+3;
	while (CDC_Transmit_FS(BufToRasp, TransferLen)!=USBD_OK) {}
return(1);
}
 int  USBMANAGER::Convert2charsToByte(uint8_t a,uint8_t  b)
 {
  if (a>96){a=a-87;}else{a=a-48;}
  if(b>96){b=b-87;}else{b=b-48;}
  return(b+(a<<4));								 						  
 }                                    