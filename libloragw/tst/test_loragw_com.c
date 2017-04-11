/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
    Minimum test program for the loragw_spi 'library'
    Use logic analyser to check the results.

License: Revised BSD License, see LICENSE.TXT file include in the project

*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>
#include <stdio.h>

#include "loragw_com.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define BURST_TEST_SIZE 2500 /* >> LGW_BURST_CHUNK */
#define TIMING_REPEAT   1    /* repeat transactions multiple times for timing characterisation */

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main()
{
    int i;
    void *com_target = NULL;
    uint8_t data = 0;
    uint8_t dataout[BURST_TEST_SIZE];
    uint8_t datain[BURST_TEST_SIZE];
    uint8_t com_mux_mode = LGW_COM_MUX_MODE0;

    for (i = 0; i < BURST_TEST_SIZE; ++i) {
        dataout[i] = 0x30 + (i % 10); /* ASCCI code for 0 -> 9 */
        datain[i] = 0x23; /* garbage data, to be overwritten by received data */
    }

    printf("Beginning of test for loragw_spi.c\n");
    lgw_com_open(&com_target);

    /* normal R/W test */
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_w(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0xAA, 0x96);
    }
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_r(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x55, &data);
    }

    /* burst R/W test, small bursts << LGW_BURST_CHUNK */
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_wb(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x55, dataout, 16);
    }
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_rb(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x55, datain, 16);
    }

    /* burst R/W test, large bursts >> LGW_BURST_CHUNK */
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_wb(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x5A, dataout, ARRAY_SIZE(dataout));
    }
    for (i = 0; i < TIMING_REPEAT; ++i) {
        lgw_com_rb(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x5A, datain, ARRAY_SIZE(datain));
    }

    /* last read (blocking), just to be sure no to quit before the FTDI buffer is flushed */
    lgw_com_r(com_target, com_mux_mode, LGW_COM_MUX_TARGET_SX1301, 0x55, &data);
    printf("data received (simple read): %d\n", data);

    lgw_com_close(com_target);
    printf("End of test for loragw_spi.c\n");

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
