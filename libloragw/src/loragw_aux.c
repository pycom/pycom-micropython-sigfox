/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
	Lora gateway auxiliary functions

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

#include <stdio.h>		/* printf fprintf */
#include <time.h>		/* clock_nanosleep */
#include <stdbool.h>	/* bool type */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#if DEBUG_AUX == 1
	#define DEBUG_MSG(str)				fprintf(stderr, str)
	#define DEBUG_PRINTF(fmt, args...)	fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#else
	#define DEBUG_MSG(str)
	#define DEBUG_PRINTF(fmt, args...)
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

/**
Pointer to a user-defined function used to protect access to the concentrator
Should be set when the library is used in a multi-threaded application
*/
int (*lock_func_ptr)(void) = NULL;

/**
Pointer to a user-defined function used to unprotect access to the concentrator
Should be set when the library is used in a multi-threaded application
*/
int (*unlock_func_ptr)(void) = NULL;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* This implementation is POSIX-pecific and require a fix to be compatible with C99 */
void wait_ms(unsigned long a) {
	struct timespec dly;
	struct timespec rem;
	
	dly.tv_sec = a / 1000;
	dly.tv_nsec = ((long)a % 1000) * 1000000;
	
	DEBUG_PRINTF("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);
	
	if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
		clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
		DEBUG_PRINTF("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
	}
	return;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void set_lock_func(int (*lock_func)(void)) {
	lock_func_ptr = lock_func;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void set_unlock_func(int (*unlock_func)(void)) {
	unlock_func_ptr = unlock_func;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_lock(void) {
	static bool disp_warning = true; /* to display the warning only once */
	
	if (lock_func_ptr == NULL) {
		if (disp_warning) {
			fprintf(stderr, "WARNING: lock callback disabled (message displayed only once)\n");
			disp_warning = false;
		}
		return 0;
	} else {
		return (*lock_func_ptr)();
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_unlock(void) {
	static bool disp_warning = true; /* to display the warning only once */
	
	if (unlock_func_ptr == NULL) {
		if (disp_warning) {
			fprintf(stderr, "WARNING: unlock callback disabled (message displayed only once)\n");
			disp_warning = false;
		}
		return 0;
	} else {
		return (*unlock_func_ptr)();
	}
}

/* --- EOF ------------------------------------------------------------------ */
