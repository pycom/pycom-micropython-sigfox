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
SX1301 Sx1301(PA_4, PA_7, PA_6, PA_5, PB_4,PA_3 );


#ifdef V2
DigitalOut FEM_EN(PB_0);	
DigitalOut RADIO_RST (PA_0);
DigitalOut HSCLKEN (PB_2);
#endif
#ifdef V1

DigitalOut RADIO_RST (PA_0);
DigitalOut HSCLKEN  (PB_14);
#endif
