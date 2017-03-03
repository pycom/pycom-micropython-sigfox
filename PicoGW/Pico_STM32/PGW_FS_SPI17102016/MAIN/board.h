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
#include "mbed.h"
#include "SX1308.h"
extern SX1308 Sx1308;
extern Serial pc;
extern DigitalOut HSCLKEN ;

#ifdef V2
extern DigitalOut FEM_EN;	
extern DigitalOut RADIO_RST ;
#endif
#endif
