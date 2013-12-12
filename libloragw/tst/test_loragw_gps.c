/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
	Minimum test program for the loragw_gps 'library'

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>		/* C99 types */
#include <stdbool.h>	/* bool type */
#include <stdio.h>		/* printf */
#include <string.h>		/* memset */
#include <signal.h>		/* sigaction */
#include <stdlib.h>		/* exit */
#include <unistd.h>		/* read */

#include "loragw_hal.h"
#include "loragw_gps.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = 1;;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = 1;
	}
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main()
{
	struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
	
	int i, j;
	char serialbuf[128]; /* buffer to receive GPS data */
	char checksum[2]; /* 2 characters to calculate NMEA checksum */
	ssize_t nb_char;
	int gps_tty_dev; /* file descriptor to the serial port of the GNSS module */
	
	/* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	
	/* Intro message and library information */
	printf("Beginning of test for loragw_gps.c\n");
	printf("*** Library version information ***\n%s\n***\n", lgw_version_info());
	
	/* start concentrator */
	
	/* Open and configure GPS */
	i = lgw_gps_enable("/dev/ttyACM0", NULL, -1, &gps_tty_dev);
	if (i != LGW_GPS_SUCCESS) {
		fprintf(stderr, "ERROR: IMPOSSIBLE TO ENABLE GPS\n");
		exit(EXIT_FAILURE);
	}
	
	/* initialize some variables before loop */
	memset(serialbuf, 0, sizeof serialbuf);
	
	/* loop until user action */
	while ((quit_sig != 1) && (exit_sig != 1)) {
		/* blocking canonical read on serial port */
		nb_char = read(gps_tty_dev, serialbuf, sizeof(serialbuf)-1); /* guaranteed to end with a null char */
		
		if (nb_char > 0) {
			/* display received serial data and checksum */
			printf("***Received %i chars: %s", nb_char, serialbuf);
			
			/* parse the received NMEA */
			lgw_parse_nmea(serialbuf, sizeof(serialbuf));
			
			/* if a specific NMEA frame was parsed, go fetch timestamp and match with absolute time */
			// if (latest_msg == NMEA_RMC) {
			// }
			
			/* clear serial buffer */
			memset(serialbuf, 0, nb_char);
		}
	}
	
	/* clean up before leaving */
	if (exit_sig == 1) {
		
	}
	
	printf("\nEnd of test for loragw_gps.c\n");
	exit(EXIT_SUCCESS);
}

/* --- EOF ------------------------------------------------------------------ */
