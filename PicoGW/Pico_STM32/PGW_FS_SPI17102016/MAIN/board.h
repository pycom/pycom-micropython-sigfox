/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2016 Semtech


Maintainer: Fabien Holin
*/
#ifndef BOARD_H
#define BOARD_H
#define GOTODFU 0x10
#include "mbed.h"
#include "SX1308.h"
#define DATA_EEPROM_BASE       ( ( uint32_t )0x8011000U )              /*!< DATA_EEPROM base address in the alias region */
#define DATA_EEPROM_END        ( ( uint32_t )DATA_EEPROM_BASE + 2048 ) /*!< DATA EEPROM end address in the alias region */

extern SX1308 Sx1308;
extern Serial pc;
extern DigitalOut HSCLKEN ;
extern DigitalOut RADIO_RST ;
#ifdef V2
extern DigitalOut FEM_EN;	

#endif
#endif
extern void  FLASH_Prog( uint32_t Address, uint8_t Data );
