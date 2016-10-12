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
	//	__HAL_RCC_USB_OTG_FS_CLK_ENABLE();
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
	}
	receivelength[0]=0;
}
int USBMANAGER::ReceiveCmd()
{ int i;
	//if (CDC_Receive_FSP(&BufFromRasptemp[0], &receivelength[0])==USBD_OK)
	CDC_Receive_FSP(&BufFromRasp[0], &receivelength[0]);
	while(BufFromRasp[0]==0)
	{
		CDC_Receive_FSP(&BufFromRasp[0], &receivelength[0]);
	}
	//else{return(0);}
	for (i=0;i<64;i++)
	{
		BufFromRasptemp[i]=BufFromRasp[i];
	}
	
	return(1);
}
int USBMANAGER::DecodeCmd()
{int i=0;
	    for(i=0;i<BUFFERRXUSBMANAGER;i++)
			{BufFromRasp[i]=BufFromRasptemp[i];
			}	
	    if (BufFromRasp[0]==0){return (0);}
	    cmdSettings_FromRasp.Cmd = BufFromRasp[0]; 
	   
		switch (cmdSettings_FromRasp.Cmd) {
			
			case 'r' :{ // cmd Read register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[4*i+5],BufFromRasp[4*i+6]);	
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[4*i+7],BufFromRasp[4*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int val=0;
								 //	Sx1301.SelectPage(page);
									val= Sx1301.spiRead(adressreg);
									BufToRasp[0]='r';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=val;																	
							break;}
			case 's' :{ // cmd Read burst register first
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
			          	cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);	
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
							
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbsize = %d\n",size);
								 //	Sx1301.SelectPage(page);
									
									BufToRasp[0]='s';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									Sx1301.spiReadBurstF(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 't' :{ // cmd Read burst register middle
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
			          	cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);	
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
							
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbsize = %d\n",size);
								 //	Sx1301.SelectPage(page);
									
									BufToRasp[0]='s';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									Sx1301.spiReadBurstM(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'u' :{ // cmd Read burst register end
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
			          	cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);	
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
							
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbsize = %d\n",size);
								 //	Sx1301.SelectPage(page);
									
									BufToRasp[0]='s';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									Sx1301.spiReadBurstE(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'p' :{ // cmd Read burst register atomic
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
			          	cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);	
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
							
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int size=(cmdSettings_FromRasp.Value[0]<<8)+cmdSettings_FromRasp.Value[1];
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
									
									BufToRasp[0]='s';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									Sx1301.spiReadBurst(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							break;}
			case 'w' :{ // cmd write register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Adress=Convert2charsToByte(BufFromRasp[4*i+5],BufFromRasp[4*i+6]);	
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[4*i+7],BufFromRasp[4*i+8]);	
									}			
									
									int adressreg=cmdSettings_FromRasp.Adress;
									int val=cmdSettings_FromRasp.Value[0];
									
									//Sx1301.SelectPage(page); unused
								  Sx1301.spiWrite(adressreg,val);
									BufToRasp[0]='w';
									BufToRasp[1]=cmdSettings_FromRasp.Id; 
									BufToRasp[2]=1;
									BufToRasp[3]=val;
									return(0); //mean no ack transmission									
							break;}
			 case 'x' :{ // cmd write burst register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
				          int adressreg=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);
				            
				          /*TBD have to implement a check of the adress*/
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstF(adressreg, &cmdSettings_FromRasp.Value[0], cmdSettings_FromRasp.Len);		
                 // pc.printf("wbfsize = %d\n",cmdSettings_FromRasp.Len);									
									BufToRasp[0]='x';
									BufToRasp[1]=cmdSettings_FromRasp.Id; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(0); //mean no ack transmission									
							break;}
			 case 'y' :{ // cmd write burst register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
				          int adressreg=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);
				            
				          /*TBD have to implement a check of the adress*/
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstM(adressreg, &cmdSettings_FromRasp.Value[0], cmdSettings_FromRasp.Len);	
//pc.printf("wbsize = %d\n",cmdSettings_FromRasp.Len);											
									BufToRasp[0]='x';
									BufToRasp[1]=cmdSettings_FromRasp.Id; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(0); //mean no ack transmission									
							break;}
			 case 'z' :{ // cmd write burst register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
				          int adressreg=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);
				            
				          /*TBD have to implement a check of the adress*/
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurstE(adressreg, &cmdSettings_FromRasp.Value[0], cmdSettings_FromRasp.Len);	
//pc.printf("wbesize = %d\n",cmdSettings_FromRasp.Len);											
									BufToRasp[0]='x';
									BufToRasp[1]=cmdSettings_FromRasp.Id; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(0); //mean no ack transmission									
							break;}
			 case 'a' :{ // cmd write burst atomic register 
									cmdSettings_FromRasp.Id  =Convert2charsToByte(BufFromRasp[1],BufFromRasp[2]);
									cmdSettings_FromRasp.Len =Convert2charsToByte(BufFromRasp[3],BufFromRasp[4]);
				          int adressreg=Convert2charsToByte(BufFromRasp[5],BufFromRasp[6]);
				            
				          /*TBD have to implement a check of the adress*/
									for (i=0;i< cmdSettings_FromRasp.Len;i++)
									{
										cmdSettings_FromRasp.Value[i] =Convert2charsToByte(BufFromRasp[2*i+7],BufFromRasp[2*i+8]);
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	

                  Sx1301.spiWriteBurst(adressreg, &cmdSettings_FromRasp.Value[0], cmdSettings_FromRasp.Len);
//pc.printf("wasize = %d\n",cmdSettings_FromRasp.Len);											
									BufToRasp[0]='x';
									BufToRasp[1]=cmdSettings_FromRasp.Id; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(0); //mean no ack transmission									
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