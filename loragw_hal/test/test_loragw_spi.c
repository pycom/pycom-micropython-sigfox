/*
 / _____)             _              | |    
( (____  _____ ____ _| |_ _____  ____| |__  
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
    Minimum test program for the loragw_spi 'library'
    Use logic analyser to check the results.
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>
#include <stdio.h>

#include "loragw_spi.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
    int spi_device;
    uint8_t data;
    uint8_t dataout[33] = "01234567012345670123456701234567";
    uint8_t datain[33] = "+-*$&!?;+-*$&!?;+-*$&!?;+-*$&!?;"; /* garbage data, should be overwritten */
    
    printf("Beginning of test for loragw_spi.c\n");
    lgw_spi_open(&spi_device);
    
    /* normal R/W test */
    lgw_spi_w(spi_device, 0xAA, 0x96);
    lgw_spi_r(spi_device, 0x55, &data);
    
    /* burst R/W test */
    lgw_spi_wb(spi_device, 0x55, dataout, ARRAY_SIZE(dataout)-1);
    lgw_spi_rb(spi_device, 0x55, datain, ARRAY_SIZE(datain)-1);
    
    /* display results */
    printf("data received: %d\n",data);
    printf("burst received: %s\n", datain);
    
    lgw_spi_close(spi_device);
    printf("End of test for loragw_spi.c\n");
    
    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
