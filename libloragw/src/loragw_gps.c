/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
	Library of functions to manage a GNSS module (typically GPS) for accurate 
	timestamping of packets and synchronisation of gateways.
	A limited set of module brands/models are supported.

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
#include <stdio.h>		/* printf fprintf */
#include <string.h>		/* memcpy */

#include <time.h>		/* struct timespec */
#include <fcntl.h>		/* open */
#include <termios.h>	/* tcflush */

#include <stdlib.h> // DEBUG

#include "loragw_reg.h"
#include "loragw_gps.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#if DEBUG_GPS == 1
	#define DEBUG_MSG(str)				fprintf(stderr, str)
	#define DEBUG_PRINTF(fmt, args...)	fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
	#define DEBUG_ARRAY(a,b,c)			for(a=0;a<b;++a) fprintf(stderr,"%x.",c[a]);fprintf(stderr,"end\n")
	#define CHECK_NULL(a)				if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_GPS_ERROR;}
#else
	#define DEBUG_MSG(str)
	#define DEBUG_PRINTF(fmt, args...)
	#define DEBUG_ARRAY(a,b,c)			for(a=0;a!=0;){}
	#define CHECK_NULL(a)				if(a==NULL){return LGW_GPS_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES -------------------------------------------------------- */

enum gps_msg {
	UNKNOWN,
	/* NMEA messages of interest */
	NMEA_RMC, /* Recommended Minimum data (time + date) */
	NMEA_GGA, /* Global positioning system fix data (pos + alt) */
	NMEA_GNS, /* GNSS fix data (pos + alt, sat number) */
	NMEA_ZDA, /* Time and Date */
	/* NMEA message useful for time reference quality assessment */
	NMEA_GBS, /* GNSS Satellite Fault Detection */
	NMEA_GST, /* GNSS Pseudo Range Error Statistics */
	NMEA_GSA, /* GNSS DOP and Active Satellites (sat number) */
	NMEA_GSV, /* GNSS Satellites in View (sat SNR) */
	/* Misc. NMEA messages */
	NMEA_GLL, /* Latitude and longitude, with time of position fix and status */
	NMEA_TXT, /* Text Transmission */
	NMEA_VTG, /* Course over ground and Ground speed */
	/* uBlox proprietary NMEA messages of interest */
	UBX_POSITION,
	UBX_TIME
};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define		DEFAULT_BAUDRATE	B9600

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */


/* result of the NMEA parsing */
static enum gps_msg		latest_msg = UNKNOWN; /* type of the latest parsed message */
static struct timespec	nmea_utc_time = {0, 0}; /* absolute time, ns accuracy */
static struct coord_s	nmea_coord = {0.0, 0.0, 0}; /* 3D coordinates */
static struct coord_s	nmea_coord_err = {0.0, 0.0, 0}; /* standard deviation of coordinates */

/* used for UTC <-> timestamp conversion */
volatile bool			ref_valid = false;
volatile uint32_t		ref_timestamp = 0;
volatile struct timespec	ref_utc_time = {0, 0};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

int nmea_checksum(char* nmea_string, unsigned max_str_len, char* checksum);

char nibble_to_hexchar(uint8_t a);

bool validate_nmea_checksum(char* serial_buff, unsigned max_str_len);

bool match_label(char* s, char* label, int size, char wildcard);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* Calculate the checksum for a NMEA string
Skip the first '$' if necessary and calculate checksum until '*' character is reached (or max_str_len exceeded)
checksum must point to a 2-byte (or more) char array
Return position of the checksum in the string */
int nmea_checksum(char* nmea_string, unsigned max_str_len, char* checksum) {
	unsigned i = 0;
	uint8_t check_num = 0;
	uint8_t nibble = 0;
	
	/* check input parameters */
	if ((nmea_string == NULL) ||  (checksum == NULL) || (max_str_len < 2)) {
		DEBUG_MSG("Invalid parameters for nmea_checksum\n");
		return -1;
	}
	
	/* skip the first '$' if necessary */
	if (nmea_string[i] == '$') {
		i += 1;
	}
	
	/* xor until '*' or max length is reached */
	while (nmea_string[i] != '*') {
		check_num ^= nmea_string[i];
		i += 1;
		if (i >= max_str_len) {
			DEBUG_MSG("Maximum length reached for nmea_checksum\n");
			return -1;
		}
	}
	
	/* Convert checksum value to 2 hexadecimal characters */
	checksum[0] = nibble_to_hexchar(check_num / 16); /* upper nibble */
	checksum[1] = nibble_to_hexchar(check_num % 16); /* lower nibble */
	
	return i + 1;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

char nibble_to_hexchar(uint8_t a) {
	if (a < 10) {
		return '0' + a;
	} else if (a < 16) {
		return 'A' + (a-10);
	} else {
		return '?';
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool validate_nmea_checksum(char* serial_buff, unsigned max_str_len) {
	int checksum_index;
	char checksum[2]; /* 2 characters to calculate NMEA checksum */
	
	checksum_index = nmea_checksum(serial_buff, max_str_len, checksum);
	
	/* could we calculate a verification checksum ? */
	if (checksum_index < 0) {
		DEBUG_MSG("ERROR: IMPOSSIBLE TO PARSE NMEA SENTENCE\n");
		return false;
	}
	
	/* check if there are enough char in the serial buffer to read checksum */
	if (checksum_index >= (max_str_len - 2)) {
		DEBUG_MSG("ERROR: IMPOSSIBLE TO READ NMEA SENTENCE CHECKSUM\n");
		return false;
	}
	
	/* check the checksum per se */
	if ((serial_buff[checksum_index] == checksum[0]) && (serial_buff[checksum_index+1] == checksum[1])) {
		return true;
	} else {
		DEBUG_PRINTF("ERROR: NMEA CHECKSUM %c%c DOESN'T MATCH VERIFICATION CHECKSUM %c%c\n", serial_buff[checksum_index], serial_buff[checksum_index+1], checksum[0], checksum[1]);
		return false;
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool match_label(char* s, char* label, int size, char wildcard) {
	int i;
	
	for (i=0; i < size; i++) {
		if (label[i] == wildcard) continue;
		if (label[i] != s[i]) return false;
	}
	return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_gps_enable(char* tty_path, char* gps_familly, speed_t target_brate, int* fd_ptr) {
	int i;
	uint8_t tstbuf[8];
	struct termios ttyopt; /* serial port options */
	int gps_tty_dev; /* file descriptor to the serial port of the GNSS module */
	
	/* check input parameters */
	CHECK_NULL(tty_path);
	CHECK_NULL(fd_ptr);
	
	/* open TTY device */
	gps_tty_dev = open(tty_path, O_RDWR | O_NOCTTY);
	if (gps_tty_dev <= 0) {
		DEBUG_MSG("ERROR: TTY PORT FAIL TO OPEN, CHECK PATH AND ACCESS RIGHTS\n");
		return LGW_GPS_ERROR;
	}
	*fd_ptr = gps_tty_dev;
	
	/* manage the different GPS modules families */
	if (gps_familly != NULL) {
		DEBUG_MSG("WARNING: gps_familly parameter ignored for now\n"); // TODO
	}
	
	/* get actual serial port configuration */
	i = tcgetattr(gps_tty_dev, &ttyopt);
	if (i != 0) {
		DEBUG_MSG("ERROR: IMPOSSIBLE TO GET TTY PORT CONFIGURATION\n");
		return LGW_GPS_ERROR;
	}
	
	/* update baudrates */
	cfsetispeed(&ttyopt, DEFAULT_BAUDRATE);
	cfsetospeed(&ttyopt, DEFAULT_BAUDRATE);
	
	/* update terminal parameters */
	ttyopt.c_cflag |= CLOCAL; /* local connection, no modem control */
	ttyopt.c_cflag |= CREAD; /* enable receiving characters */
	ttyopt.c_cflag |= CS8; /* 8 bit frames */
	ttyopt.c_cflag &= ~PARENB; /* no parity */
	ttyopt.c_cflag &= ~CSTOPB; /* one stop bit */
	ttyopt.c_iflag |= IGNPAR; /* ignore bytes with parity errors */
	ttyopt.c_iflag |= ICRNL; /* map CR to NL */
	ttyopt.c_iflag |= IGNCR; /* Ignore carriage return on input */
	ttyopt.c_lflag |= ICANON; /* enable canonical input */
	
	/* set new serial ports parameters */
	i = tcsetattr(gps_tty_dev, TCSANOW, &ttyopt);
	if (i != 0){
		DEBUG_MSG("ERROR: IMPOSSIBLE TO UPDATE TTY PORT CONFIGURATION\n");
		return LGW_GPS_ERROR;
	}
	tcflush(gps_tty_dev, TCIOFLUSH);
	
	/* initialize global variables */
	latest_msg = UNKNOWN;
	memset((void *)&nmea_utc_time, 0, sizeof nmea_utc_time);
	memset((void *)&nmea_coord, 0, sizeof nmea_coord);
	memset((void *)&nmea_coord_err, 0, sizeof nmea_coord_err);
	ref_valid = false;
	ref_timestamp = 0;
	memset((void *)&ref_utc_time, 0, sizeof ref_utc_time);
	
	return LGW_GPS_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_parse_nmea(char* serial_buff, unsigned max_str_len) {
	int i;
	
	/* check input parameters */
	CHECK_NULL(serial_buff);
	
	/* look for some NMEA sentences in particular */
	if (max_str_len < 8) {
		DEBUG_MSG("ERROR: TOO SHORT TO BE A VALID NMEA SENTENCE\n");
		return LGW_GPS_ERROR;
	} else if (match_label(serial_buff, "$G?RMC", 6, '?')) {
		if (!validate_nmea_checksum(serial_buff, max_str_len)) {
			DEBUG_MSG("ERROR: INVALID RMC SENTENCE\n");
			return LGW_GPS_ERROR;
		}
		DEBUG_MSG("Note: Valid RMC sentence\n");
	} else if (match_label(serial_buff, "$G?GGA", 6, '?')) {
		if (!validate_nmea_checksum(serial_buff, max_str_len)) {
			DEBUG_MSG("ERROR: INVALID GGA SENTENCE\n");
			return LGW_GPS_ERROR;
		}
		DEBUG_MSG("Note: Valid GGA sentence\n");
	} else {
		DEBUG_MSG("WARNING: ignored NMEA sentence\n");
	}
	
	return LGW_GPS_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_gps_sync(uint32_t counter, struct timespec utc) {
	return LGW_GPS_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void lgw_gps_thread(void) {
	// int i, j;
	// bool thread_err = false;
	// char serialbuf[128];
	// char checksum[2];
	// ssize_t nb_char;
	
	// memset(serialbuf, 0, sizeof serialbuf);
	
	// while (!tread_err) {
		// /* blocking canonical read on serial port */
		// nb_char = read(gps_tty_dev, serialbuf, sizeof serialbuf)-1; /* guaranteed to end with a null char */
		
		// if (nb_char > 0) {
			// nmea_checksum(serialbuf, sizeof serialbuf, checksum);
			// printf("***Received %i chars (checksum %c%c): %s", nb_char, checksum[0], checksum[1], serialbuf);

			// /* parse the received NMEA */
			// lgw_parse_nmea(serialbuf);
			
			// /* if a specific NMEA frame was parsed, go fetch timestamp and match with absolute time */
			// if (latest_msg == NMEA_RMC) {
			// }
		// }
		// memset(serialbuf, 0, nb_char);
	// }
	return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_cnt2utc(uint32_t counter, struct timespec* utc) {
	
	CHECK_NULL(utc);
	
	return LGW_GPS_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_utc2cnt(struct timespec utc, uint32_t* counter) {

	CHECK_NULL(counter);
	
	return LGW_GPS_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_get_coord(struct coord_s* loc, struct coord_s* err, struct timespec* utc) {
	if (loc != NULL) {
		*loc = nmea_coord;
	}
	if (err != NULL) {
		*err = nmea_coord_err;
	}
	if (utc != NULL) {
		*utc = nmea_utc_time;
	}
	return LGW_GPS_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
