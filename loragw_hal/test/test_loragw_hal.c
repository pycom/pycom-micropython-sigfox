/*
 / _____)             _              | |    
( (____  _____ ____ _| |_ _____  ____| |__  
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
    Minimum test program for the loragw_hal 'library'
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdlib.h>     /* malloc & free */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf */
#include <string.h>     /* memset */

#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
    int i;
    int loop_var;
    int nb_pkt;
    
    const int iftype[] = LGW_IF_CONFIG; /* example to use the #define arrays */
    
    static struct lgw_conf_rxrf_s rfconf;
    static struct lgw_conf_rxif_s ifconf;
    static struct lgw_pkt_rx_s rxpkt[4]; /* array containing up to 4 inbound packets metadata */
    static struct lgw_pkt_tx_s txpkt; /* configuration and metadata for an outbound packet */
    static uint8_t txbuf[256]; /* buffer for the TX payload */
    
    printf("Beginning of test for loragw_hal.c\n");
    
    lgw_start();
    
    /* configure RF chain */
    memset(&rfconf, 0, sizeof(rfconf));
    rfconf.enable = true;
    rfconf.freq_hz = 867*10^6;
    lgw_rxrf_setconf(0, rfconf);
    
    /* configure Lora multi-SF channels (bandwidth cannot be set) */
    memset(&ifconf, 0, sizeof(ifconf));
    ifconf.enable = true;
    ifconf.rf_chain = 0;
    ifconf.datarate = DR_LORA_MULTI;
    ifconf.freq_hz = -187.5e3;
    for(i=0; i<=3; ++i) {
        lgw_rxif_setconf(i, ifconf);
        ifconf.freq_hz += 125e3;
    }
    
    loop_var = 1;
    while(loop_var == 1) {
        /* fetch all available packets */
        nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);
        
        /* filter/process the received packets */
        /* forward/store the received packets */
        
        /* free the memory used for RX payload(s) */
        for(i=0; i < nb_pkt; ++i) {
            free(rxpkt[i].payload);
        }
    }
    
    lgw_stop();
    
    printf("End of test for loragw_hal.c\n");
    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
