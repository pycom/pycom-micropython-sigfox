/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
(C)2016 Semtech


Maintainer: Fabien Holin
the  HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) tontain the it rx usb management

*/
#include "CmdUSB.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "loragw_hal.h"
#include "loragw_reg.h"
#include "Registers1308.h"
#include "mbed.h"
#include "board.h"


USBMANAGER::USBMANAGER()
{
}

void USBMANAGER::init()
{
	__HAL_RCC_USB_OTG_FS_CLK_ENABLE();
	MX_USB_DEVICE_Init();

}
void USBMANAGER::initBuffromhost()
{
	int i;
	for (i = 0; i < BUFFERRXUSBMANAGER; i++)
	{
		BufFromHost[i] = 0;
		
	}
	for (i = 0; i < 64; i++)
	{
	BufFromHosttemp[i] = 0;
	}
	receivelength[0] = 0;
}
void USBMANAGER::initBuftohost()
{
	int i;
	for (i = 0; i < BUFFERTXUSBMANAGER; i++)
	{
		BufToHost[i] = 0;
	}
}

void USBMANAGER::ReceiveCmd()
{
	CDC_Receive_FSP(&BufFromHosttemp[0], &receivelength[0]);// wait interrrupt manage in HAL_PCD_DataOutStageCallback
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
/*  j           |lgw_calibration_snapshot               */
/*  l						|lgw_check_fw_version										*/
/*  m						|Reset STM32        										*/
/*  n						|GOTODFU            										*/

int USBMANAGER::DecodeCmd()
{
	int i = 0;
	int adressreg;
	int val;

	if (BufFromHost[0] == 0) { return (CMD_ERROR); }
	cmdSettings_FromHost.Cmd = BufFromHost[0];

	switch (cmdSettings_FromHost.Cmd) {

	case 'r': { // cmd Read register 
		
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];

		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}

		val = 0;
		val = Sx1308.spiRead(adressreg);
		BufToHost[0] = 'r';
		BufToHost[1] = 0;
		BufToHost[2] = 1; //Len LSB
		BufToHost[3] = val;
		return(CMD_OK);
	}
	case 's': { // cmd Read burst register first
		int i;
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];

		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		int size = (cmdSettings_FromHost.Value[0] << 8) + cmdSettings_FromHost.Value[1];

		BufToHost[0] = 's';
		BufToHost[1] = cmdSettings_FromHost.Value[0];
		BufToHost[2] = cmdSettings_FromHost.Value[1];
		Sx1308.spiReadBurstF(adressreg, &BufToHost[3 + 0], size);
		return(CMD_OK);
	}
	case 't': { // cmd Read burst register middle
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];

		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		int size = (cmdSettings_FromHost.Value[0] << 8) + cmdSettings_FromHost.Value[1];

		BufToHost[0] = 't';
		BufToHost[1] = cmdSettings_FromHost.Value[0];
		BufToHost[2] = cmdSettings_FromHost.Value[1];
		Sx1308.spiReadBurstM(adressreg, &BufToHost[3 + 0], size);
		return(CMD_OK);
	}
	case 'u': { // cmd Read burst register end
		int i;
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];

		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		int size = (cmdSettings_FromHost.Value[0] << 8) + cmdSettings_FromHost.Value[1];
		BufToHost[0] = 'u';
		BufToHost[1] = cmdSettings_FromHost.Value[0];
		BufToHost[2] = cmdSettings_FromHost.Value[1];
		Sx1308.spiReadBurstE(adressreg, &BufToHost[3 + 0], size);
		return(CMD_OK);
	}
	case 'p': { // cmd Read burst register atomic
		int i;
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		int size = (cmdSettings_FromHost.Value[0] << 8) + cmdSettings_FromHost.Value[1];
		BufToHost[0] = 'p';
		BufToHost[1] = cmdSettings_FromHost.Value[0];
		BufToHost[2] = cmdSettings_FromHost.Value[1];
		//for (i=0;i<size;i++){BufToHost[3+i]=i;}
		Sx1308.spiReadBurst(adressreg, &BufToHost[3 + 0], size);
		return(CMD_OK);
	}
	case 'e': { // cmd Read burst register atomic
		
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		int size = (cmdSettings_FromHost.Value[0] << 8) + cmdSettings_FromHost.Value[1];
		BufToHost[0] = 'e';
		BufToHost[1] = cmdSettings_FromHost.Value[0];
		BufToHost[2] = cmdSettings_FromHost.Value[1];
		Sx1308.spiReadBurst(adressreg, &BufToHost[3 + 0], size);
		return(CMD_OK);
	}
	case 'w': { // cmd write register 
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		val = cmdSettings_FromHost.Value[0];
		Sx1308.spiWrite(adressreg, val);
		BufToHost[0] = 'w';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}
	case 'x': { // cmd write burst register 
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		int size = cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8);
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		Sx1308.spiWriteBurstF(adressreg, &cmdSettings_FromHost.Value[0], size);
		BufToHost[0] = 'x';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}
	case 'y': { // cmd write burst register 
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		int size = cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8);
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		Sx1308.spiWriteBurstM(adressreg, &cmdSettings_FromHost.Value[0], size);
		BufToHost[0] = 'y';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}
	case 'z': { // cmd write burst register 
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		int size = cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8);
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}

		Sx1308.spiWriteBurstE(adressreg, &cmdSettings_FromHost.Value[0], size);
		BufToHost[0] = 'z';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}
	case 'a': { // cmd write burst atomic register 
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		int size = cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8);
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		Sx1308.spiWriteBurst(adressreg, &cmdSettings_FromHost.Value[0], size);
		BufToHost[0] = 'a';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}
	case 'b': { // it is for hal cmd lgw_receive
		static struct lgw_pkt_rx_s pkt_data[16]; //16 max packets TBU 
		uint8_t nbpacket = 0;
		int j = 0;
		int sizeatomic = sizeof(lgw_pkt_rx_s) / sizeof(uint8_t);
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];

		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
		nbpacket = lgw_receive(cmdSettings_FromHost.Value[0], &pkt_data[0]);
		BufToHost[0] = 'b';
		BufToHost[3] = nbpacket;
		int cptalc = 0;
		int pt = 0;
		for (j = 0; j < nbpacket; j++)
		{
			for (i = 0; i < (pkt_data[j].size + (sizeatomic - 256)); i++)
			{
				BufToHost[4 + i + pt] = *((uint8_t *)(&pkt_data[j]) + i);
				cptalc++;
			}
			pt = cptalc;
		}
		BufToHost[cptalc + 4] = 0x23;
		BufToHost[cptalc + 5] = 0x04;
		BufToHost[cptalc + 6] = 0x20;
		BufToHost[cptalc + 7] = 0x09;
		cptalc = cptalc + 5; // 4 for "false crc" + 1 for buftoHost
		BufToHost[1] = (cptalc >> 8);
		BufToHost[2] = cptalc - ((cptalc >> 8) << 8);
		return(CMD_OK);
	}
	case 'c': { // lgw_rxrf_setconf 
		uint8_t rf_chain;
		uint8_t  conf[30];
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		rf_chain = BufFromHost[3];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			conf[i] = BufFromHost[4 + i];
		}
		lgw_rxrf_setconf(rf_chain, *(lgw_conf_rxrf_s *)conf);
		BufToHost[0] = 'c';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}

	case 'h': { // lgw_txgain_setconf 

		uint8_t  conf[100];
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			conf[i] = BufFromHost[4 + i];
		}
		int Lutnumber;
		Lutnumber = lgw_txgain_setconf((lgw_tx_gain_lut_s *)conf);
		BufToHost[0] = 'h';
		BufToHost[1] = 0;
		BufToHost[2] = ACK_OK;
		BufToHost[3] = Lutnumber; //return the lut number 
		return(CMD_OK); //mean no ack transmission									
	}
	case 'd': { // lgw_rxif_setconf 
		uint8_t if_chain;
		uint8_t  conf[(sizeof(struct lgw_conf_rxif_s) / sizeof(uint8_t))];
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		if_chain = BufFromHost[3];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			conf[i] = BufFromHost[4 + i];
		}
		lgw_rxif_setconf(if_chain, *(lgw_conf_rxif_s *)conf);
		BufToHost[0] = 'd';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK);
	}

	case 'f': { // lgw_send 
		int32_t txcontinuous;
		uint8_t  conf[(sizeof(struct lgw_pkt_tx_s) / sizeof(uint8_t))];
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			conf[i] = BufFromHost[4 + i];
		}
	
#ifdef V2
			HSCLKEN = 0; //switch off correlator
#else
			lgw_reg_w(LGW_CLKHS_EN, 0);
#endif
     
			calibration_save();
			Sx1308.txongoing = 1;
			Sx1308.waittxend = 1;
		  lgw_send(*(lgw_pkt_tx_s *)conf);
		  lgw_reg_r(LGW_TX_MODE, &txcontinuous); // to switch off the timeout in case of tx continuous
			if (txcontinuous==1)
			{
				BufToHost[0] = 'f';
	    	BufToHost[1] = 0;
	    	BufToHost[2] = 1;
		    BufToHost[3] = ACK_OK;
	    	return(CMD_OK); //mean no ack transmission	
			}
			while (Sx1308.waittxend) //wait for tx interrupt
			{
				// remove the timeout in case of no txdone timeout is manage in the HAL by no response from the usb cmd
			}
		
			lgw_get_trigcnt(&(Sx1308.hosttm));
			if (Sx1308.firsttx == 0)
			{
				Sx1308.offtmstpstm32ref = (Sx1308.timerstm32ref.read_us() - Sx1308.hosttm) + 60;
				Sx1308.firsttx = 1;
			}
			Sx1308.dig_reset();
#ifdef V2
			HSCLKEN = 1;
#else
			lgw_reg_w(LGW_CLKHS_EN, 1);
#endif
			lgw_start();
			
		BufToHost[0] = 'f';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK); //mean no ack transmission									
	}
	case 'q': { // cmd Read burst register first
				//		int i;
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		uint32_t timestamp;
		lgw_get_trigcnt(&timestamp);
		timestamp += Sx1308.offtmstpstm32;
		BufToHost[0] = 'q';
		BufToHost[1] = 0;
		BufToHost[2] = 4;
		BufToHost[3] = (uint8_t)(timestamp >> 24);
		BufToHost[4] = (uint8_t)((timestamp & 0x00FF0000) >> 16);
		BufToHost[5] = (uint8_t)((timestamp & 0x0000FF00) >> 8);
		BufToHost[6] = (uint8_t)((timestamp & 0x000000FF));
		return(CMD_OK);
	}

	case 'i': { // lgw_board_setconf 
		
		uint8_t  conf[(sizeof(struct lgw_conf_board_s) / sizeof(uint8_t))];
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			conf[i] = BufFromHost[4 + i];
		}

		lgw_board_setconf(*(lgw_conf_board_s *)conf);
		BufToHost[0] = 'i';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK); //mean no ack transmission									
	}
	case 'j': { // lgw_calibration_snapshot
     calibrationoffset_save();
		BufToHost[0] = 'j';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = ACK_OK;
		return(CMD_OK); //mean no ack transmission									
	}
	case 'l': { // lgw_check_fw_version	
		cmdSettings_FromHost.LenMsb = BufFromHost[1];
		cmdSettings_FromHost.Len = BufFromHost[2];
		adressreg = BufFromHost[3];
		int fwfromhost;
		for (i = 0; i < cmdSettings_FromHost.Len + (cmdSettings_FromHost.LenMsb << 8); i++)
		{
			cmdSettings_FromHost.Value[i] = BufFromHost[4 + i];
		}
    fwfromhost=(BufFromHost[4]<<24)+(BufFromHost[5]<<16)+(BufFromHost[6]<<8)+(BufFromHost[7]);
		pc.printf("fwfromhost =%x\n",fwfromhost);
		BufToHost[0] = 'l';
		BufToHost[1] = 0;
		BufToHost[2] = 9;
		if (fwfromhost==FWVERSION)
		{
			BufToHost[3] = ACK_OK;
		}
		else
		{
			BufToHost[3] = ACK_K0;
		}
		BufToHost[4] =*(uint8_t *)0x1fff7a18;    //unique STM32 register base adresse 
		BufToHost[5] =*(uint8_t *)0x1fff7a19;
		BufToHost[6] =*(uint8_t *)0x1fff7a1a;
		BufToHost[7] =*(uint8_t *)0x1fff7a1b;
		BufToHost[8] =*(uint8_t *)0x1fff7a10;
		BufToHost[9] =*(uint8_t *)0x1fff7a11;
		BufToHost[10] =*(uint8_t *)0x1fff7a12;
		BufToHost[11] =*(uint8_t *)0x1fff7a13;
		return(CMD_OK); //mean no ack transmission									
	}
	
	case 'm': { // KILL STM32 
     
		NVIC_SystemReset(); 						
	}
  
	case 'n': { // GOTODFU 
    FLASH_Prog(DATA_EEPROM_BASE, GOTODFU);	
    wait_ms(200);
		
		NVIC_SystemReset(); 						
	}

	default:BufToHost[0] = 'k';
		BufToHost[1] = 0;
		BufToHost[2] = 1;
		BufToHost[3] = 1;
		return(CMD_K0);

	}
}
int  USBMANAGER::TransmitAnswer()
{
	int TransferLen = BufToHost[2] + 3;
	while (CDC_Transmit_FS(BufToHost, TransferLen) != USBD_OK) {}
	return(1);
}
int  USBMANAGER::Convert2charsToByte(uint8_t a, uint8_t  b)
{
	if (a > 96) { a = a - 87; }
	else { a = a - 48; }
	if (b > 96) { b = b - 87; }
	else { b = b - 48; }
	return(b + (a << 4));
}
