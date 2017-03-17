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
#include "board.h"
#include "mbed.h"

Serial pc(PB_6,PB_7);
#ifdef V2
SX1308 Sx1308(PA_4, PA_7, PA_6, PA_5, PB_4,PA_3);  
#endif
#ifdef V1
SX1308 Sx1308(PA_4, PA_7, PA_6, PA_5, PB_1,PA_3);  
#endif
/* SPI config 
PA4 : SPI NSS
PA7 : SPI MOSI
PA6 : SPI MISO
PA5 : SPI SCK
*/


/* interrupt 
PB4 : TX ON interrupt
*/

/* Reset sx1308
PA_3 : reset
*/

#ifdef V2
DigitalOut FEM_EN(PB_0);	    //enable ldo 2V for PA
DigitalOut RADIO_RST (PA_0); // reset sx1257 but sx1257 deliver HSE clk for stm32  so use HSI clk before to reset sx1257 
DigitalOut HSCLKEN (PB_2); // clk to switch off the correlators
#endif
#ifdef V1
DigitalOut RADIO_RST (PA_0);
DigitalOut HSCLKEN  (PB_14);
#endif


void  FLASH_Prog( uint32_t Address, uint8_t Data )
{ 
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
	FLASH_Erase_Sector(FLASH_SECTOR_4, VOLTAGE_RANGE_3); //sector4 0x8010000 0x801FFFF
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, Address, Data);
	HAL_FLASH_Lock();
}



