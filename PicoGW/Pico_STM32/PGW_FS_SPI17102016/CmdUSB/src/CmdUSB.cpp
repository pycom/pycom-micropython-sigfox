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
#include "loragw_hal.h"
#include "loragw_reg.h"
#include "Registers1301.h"
#include "mbed.h"
#include "board.h"


// end V2
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
return true;
}
int USBMANAGER::ReceiveCmd()
{ 
		CDC_Receive_FSP(&BufFromRasptemp[0], &receivelength[0]);// wait interrrupt manage in HAL_PCD_DataOutStageCallback
	return(1);
}

/********************************************************/
/*   cmd name   |      description                      */
/*------------------------------------------------------*/
/*  r						|Read register													*/	
/*	s						|Read long burst First packet						*/
/*	t						|Read long burst Middle packet					*/
/*	u						|Read long burst End packet							*/
/*	p,e					|Read long Atomic packet								*/
/*	w						|Write register													*/
/*	x						|Write long burst First packet	   			*/	
/*	y						|Write long burst First packet					*/
/*	z						|Write long burst First packet					*/
/*	a						|Write long burst First packet					*/
/*------------------------------------------------------*/
/*	b						|lgw_receive cmd												*/
/*	c						|lgw_rxrf_setconf cmd										*/
/*  d						|int lgw_rxif_setconf_cmd								*/ 
/*  f						|int lgw_send cmd												*/
/*  h           |lgw_txgain_setconf                     */
/*  q						|lgw_trigger                            */ 
/*  i           |lgw_board_setconf                      */   

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
							}
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
								
									BufToRasp[0]='s';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurstF(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							
							}
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
								
									BufToRasp[0]='t';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
								//	for (i=0;i<size;i++){BufToRasp[3+i]=8;}
									Sx1301.spiReadBurstM(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							}
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
								
									BufToRasp[0]='u';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=8;}
									Sx1301.spiReadBurstE(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							}
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
								
									BufToRasp[0]='p';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurst(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							}
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
								
									BufToRasp[0]='e';
									BufToRasp[1]=cmdSettings_FromRasp.Value[0]; 
									BufToRasp[2]=cmdSettings_FromRasp.Value[1];
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
									Sx1301.spiReadBurst(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							}
				
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
							}
			
						
							
							
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
									BufToRasp[0]='x';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;									
									return(1); //mean no ack transmission									
							}
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
									BufToRasp[0]='y';
									BufToRasp[1]=0; 
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
						}
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
							}
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
						}
       case 'b' :{ // it is for hal cmd lgw_receive
				           /* debug for powermeter*/ 
				         
				          static struct lgw_pkt_rx_s pkt_data[16]; //16 max packets TBU 
				          uint8_t nbpacket =0;
				          int sizeresp=0;
									int j=0;
				
				          int sizeatomic=sizeof(lgw_pkt_rx_s)/sizeof(uint8_t);
				        //  pc.printf("size atomic =%d %d\n",sizeatomic,sizeof(lgw_pkt_rx_s));
								//	uint8_t temp[sizeatomic+2];
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				          //int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{
											cmdSettings_FromRasp.Value[i] =BufFromRasp[4+i];	
                  	//Sx1301.spiWrite(adressreg,cmdSettings_FromRasp.Value[i]);
									}	
                  nbpacket=lgw_receive(cmdSettings_FromRasp.Value[0], &pkt_data[0]);
                /* if (nbpacket>0)		// use for powermeter
								 {
                   if (debugsleep>20)
									{
				            deepsleep();
									}
									debugsleep++;
								}*/
                 sizeresp=(nbpacket*sizeatomic)+1;
//pc.printf("wbesize = %d\n",cmdSettings_FromRasp.Len);											
									BufToRasp[0]='b';
								//BufToRasp[1]=(sizeresp>>8); 
								//	BufToRasp[2]=sizeresp-((sizeresp>>8)<<8);
									BufToRasp[3]=nbpacket;
									int cptalc=0;
									int pt=0;
								//		pc.printf("data send to rburst nb packet %d %d atomic size = %d freq=%d\n",nbpacket,sizeresp,sizeatomic,pkt_data[0].freq_hz);
									for (j=0;j<nbpacket;j++)
									{
									for (i=0;i<(pkt_data[j].size+(sizeatomic-256));i++)
									{
									//	BufToRasp[4+i+(j*sizeatomic)]=*((uint8_t *)(&pkt_data[0])+i+(j*sizeatomic));
										BufToRasp[4+i+pt]=*((uint8_t *)(&pkt_data[j])+i);
										cptalc++;
									//	pc.printf("%d\n",BufToRasp[4+i+(j*sizeatomic)]);
									}
									pt=cptalc;
								}
									BufToRasp[cptalc+4]=0x23;
								  BufToRasp[cptalc+5]=0x04;
								  BufToRasp[cptalc+6]=0x20;
								  BufToRasp[cptalc+7]=0x09;
								  cptalc=cptalc+5; // 4 for "crc" + 1 for buftorasp
									BufToRasp[1]=(cptalc>>8); 
									BufToRasp[2]=cptalc-((cptalc>>8)<<8);
							
									
									return(1); //mean no ack transmission	
							}
			  case 'c' :{ // lgw_rxrf_setconf 
					        uint8_t rf_chain;
					        uint8_t  conf[30];
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          rf_chain =BufFromRasp[3];
								//	int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
					//pc.printf("receive cmd rx config %d\n",sizeof(lgw_conf_rxrf_s));
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										conf[i] =BufFromRasp[4+i];
//pc.printf("data[%d]=%d  \n",i,conf[i]);										
									}
									lgw_rxrf_setconf( rf_chain,  *(lgw_conf_rxrf_s *)conf); 
                 
									BufToRasp[0]='c';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							}
				
							 case 'h' :{ // lgw_txgain_setconf 
					        
					        uint8_t  conf[100];
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          
								//	int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
	//				pc.printf("receive cmd rx config\n");
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										conf[i] =BufFromRasp[4+i];
         									
									}
							  int tmp ;
                 tmp=lgw_txgain_setconf((lgw_tx_gain_lut_s *)conf);
									BufToRasp[0]='h';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=tmp;
									return(1); //mean no ack transmission									
							}
				  case 'd' :{ // lgw_rxif_setconf 
					        uint8_t if_chain;
					        uint8_t  conf[(sizeof(struct lgw_conf_rxif_s)/sizeof(uint8_t))];
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          if_chain =BufFromRasp[3];
								//	int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
	              //pc.printf("size rxif struct%d  \n",size);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										conf[i] =BufFromRasp[4+i];
							  //  pc.printf("%d  \n",conf[i]);	
									}
									
                  lgw_rxif_setconf( if_chain, *(lgw_conf_rxif_s *) conf); 
									BufToRasp[0]='d';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
						}
					
							 case 'f' :{ // lgw_send 
					        int32_t txcontinuous;
								  int temp;
					        uint8_t  conf[(sizeof(struct lgw_pkt_tx_s)/sizeof(uint8_t))];
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				        
								//	int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
	              //pc.printf("size rxif struct%d  \n",size);
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										conf[i] =BufFromRasp[4+i];
							  //  pc.printf("datato send [%d] = %d  \n",i,conf[i]);	
									}
								 	//tbc for V2 
								/*  lgw_reg_w(LGW_CLKHS_EN, 0);
								 // HSCLKEN=0;*/
									//lgw_reg_w(LGW_CLKHS_EN, 0);
								  //HSCLKEN=0;
										//end for V2 
									lgw_reg_r(LGW_TX_MODE, &txcontinuous); // to switch off the timeout in case of tx continuous
									 if (txcontinuous!=1)
									 {
									#ifdef V2
									    HSCLKEN=0;
									#else
									lgw_reg_w(LGW_CLKHS_EN, 0);
									#endif
									calibration_save();
									Sx1301.txongoing=1;
									Sx1301.waittxend=1;
									 
										 temp=lgw_send(*(lgw_pkt_tx_s *)conf);
										Timer t;
										t.reset();
										t.start();
									  
									while(Sx1301.waittxend) //wait for tx inte
									//{if (Sx1301.firsttx==0)
									{
						
                    if (t.read()>3500){break;} // 3500s max frame size
									}
									 
								//}
								  //wait_ms(10);
									 Sx1301.timerstm32.stop();
                  Sx1301.timerstm32.reset();
									Sx1301.timerstm32.start();	
									lgw_get_trigcnt(&(Sx1301.hosttm));
									if (Sx1301.firsttx==0)
									{
									Sx1301.offtmstpstm32ref=(Sx1301.timerstm32ref.read_us()-Sx1301.hosttm)+60;
                	Sx1301.firsttx=1;
									}
										//pc.printf("il est 8heureeees = %d \n",Sx1301.hosttm);								
									
									
									Sx1301.dig_reset();
										
								//	page_switch(Sx1301.lgw_currentpage);
									//lgw_soft_reset();
									
										//tbc for V2 
							   	/* 
									lgw_reg_w(LGW_CLKHS_EN, 1);
									//HSCLKEN=1;*/
									//lgw_reg_w(LGW_CLKHS_EN, 1);
									//HSCLKEN=1;
										//end for V2 
									#ifdef V2
									    HSCLKEN=1;
									#else
									lgw_reg_w(LGW_CLKHS_EN, 1);
									#endif
									lgw_start();
									}
									 else
									 {
										  lgw_send(*(lgw_pkt_tx_s *)conf);
									 }
                 
									BufToRasp[0]='f';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=temp;
									return(1); //mean no ack transmission									
							}
							 	case 'q' :{ // cmd Read burst register first
								//		int i;
									cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				          adressreg =BufFromRasp[3];
				
									uint32_t timestamp;
									//pc.printf("rbasize = %d\n",size);
								 //	Sx1301.SelectPage(page);
								  lgw_get_trigcnt(&timestamp);
									
									timestamp+=Sx1301.offtmstpstm32;
									BufToRasp[0]='q';
									BufToRasp[1]=0; 
									BufToRasp[2]=4;
									BufToRasp[3]=(uint8_t)(timestamp>>24);
									BufToRasp[4]=(uint8_t)((timestamp&0x00FF0000)>>16);
									BufToRasp[5]=(uint8_t)((timestamp&0x0000FF00)>>8);
									BufToRasp[6]=(uint8_t)((timestamp&0x000000FF));
								//		pc.printf("timestampread = %d et %d \n",timestamp,(BufToRasp[3]<<24)+(BufToRasp[4]<<16)+(BufToRasp[5]<<8)+BufToRasp[6]);
									//for (i=0;i<size;i++){BufToRasp[3+i]=i;}
								//	Sx1301.spiReadBurstF(adressreg,&BufToRasp[3+0],size);
								
              return(1);									
							
							}
						
             case 'i' :{ // lgw_board_setconf 
					      //  uint8_t if_chain;
					        uint8_t  conf[(sizeof(struct lgw_conf_board_s)/sizeof(uint8_t))];
				        	cmdSettings_FromRasp.Id  =BufFromRasp[1];
									cmdSettings_FromRasp.Len =BufFromRasp[2];
				        
									//int size=cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);
	             
									for (i=0;i< cmdSettings_FromRasp.Len+(cmdSettings_FromRasp.Id <<8);i++)
									{		
										conf[i] =BufFromRasp[4+i];
							  //  pc.printf("%d  \n",conf[i]);	
									}
									
                  lgw_board_setconf( *(lgw_conf_board_s *) conf); 
									BufToRasp[0]='i';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
						}	
							case 'l' :{ // lgw_board_reset 
					       
				        
	             
								
                  // tbd done reset radio sx1257
							  //  RADIO_RST=1; 
								 // Sx1301.dig_reset();
							    //wait_ms(1);
							    //RADIO_RST=0;
								//Sx1301.resetsx1257();
								//wait(5);
								 //NVIC_SystemReset();

									BufToRasp[0]='l';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;
									return(1); //mean no ack transmission									
							}																			 
								
							default :BufToRasp[0]='k';
									BufToRasp[1]=0 ;
									BufToRasp[2]=1;
									BufToRasp[3]=1;	
							return(0);
			
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
 