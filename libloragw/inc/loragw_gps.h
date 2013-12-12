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


#ifndef _LORAGW_GPS_H
#define _LORAGW_GPS_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>		/* C99 types */
#include <time.h>		/* time library */
#include <termios.h>	/* speed_t */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/**
@struct coord_s
@brief Geodesic coordinates
*/
struct coord_s {
	double		lat;	/*!> latitude [-90,90] (North +, South -) */
	double		lon;	/*!> longitude [-180,180] (East +, West -)*/
	float		alt;	/*!> altitude in meters (WGS 84 geoid ref.) */
};

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_GPS_SUCCESS	 0
#define LGW_GPS_ERROR	-1

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Configure a GPS module, and enable GPS functions in concentrator (must be started)
@param tty_path path to the TTY connected to the GPS
@param gps_familly parameter (eg. ubx6 for uBlox gen.6)
@param brate target baudrate for communication (-1 to keep default target baudrate)
@param fd_ptr pointer to a variable to receive file descriptor on GPS tty
@return success if the function was able to connect and configure a GPS module
*/
int lgw_gps_enable(char* tty_path, char* gps_familly, speed_t target_brate, int* fd_ptr);

/**
@brief Parse messages coming from the GPS system (or other GNSS)
@param serial_buff pointer to the string to be parsed
@param max_str_len maximum sting lengths for NMEA parsing
@return success if the frame was parsed successfully
*/
int lgw_parse_nmea(char* serial_buff, unsigned max_str_len);

/**
@brief Take a timestamp and matched UTC time and refresh parameters for UTC <-> timestamp conversion
@param counter internal timestamp counter of a Lora gateway
@param utc UTC time, with ns precision (leap seconds are ignored)
@return success if timestamp was read and parameters could be refreshed
*/
int lgw_gps_sync(uint32_t counter, struct timespec utc);

/**
@brief Thread that do blocking serial read, NMEA parsing and estimation
You *MUST* use lgw_set_lock_function and lgw_set_lock_unfunction for thread safety.
This thread never return, but you can kill it safely (do no write on hardware, just read).
*/
void lgw_gps_thread(void);

/**
@brief Convert concentrator timestamp counter value to UTC time (ns precision) after RX
@param counter internal timestamp counter of a Lora gateway
@param utc pointer to store UTC time, with ns precision (leap seconds are ignored)
@return success if the function was able to convert timestamp to UTC
*/
int lgw_cnt2utc(uint32_t counter, struct timespec* utc);

/**
@brief Convert UTC time (ns precision) to concentrator timestamp counter value for TX
@param utc UTC time, with ns precision (leap seconds are ignored)
@param counter pointer to store internal timestamp counter of a Lora gateway
@return success if the function was able to convert UTC to timestamp
*/
int lgw_utc2cnt(struct timespec utc, uint32_t* counter);

/**
@brief Get the GPS solution (space & time) for the gateway
@param loc pointer to store coordinates (NULL to ignore)
@param err pointer to store coordinates standard deviation (NULL to ignore)
@param utc pointer to store UTC time, with ns precision (NULL to ignore)
@return success if the chosen elements could be returned
*/
int lgw_get_coord(struct coord_s* loc, struct coord_s* err,  struct timespec* utc);

#endif

/* --- EOF ------------------------------------------------------------------ */
