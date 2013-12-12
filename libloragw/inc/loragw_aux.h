/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    ©2013 Semtech-Cycleo

Description:
	Lora gateway library common auxiliary functions

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


#ifndef _LORAGW_AUX_H
#define _LORAGW_AUX_H

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief Wait for a certain time (millisecond accuracy)
@param t number of milliseconds to wait.
*/
void wait_ms(unsigned long t);

/**
@brief Set function pointer to lock function
@param lock_func pointer to lock function
*/
void set_lock_func(int (*lock_func)(void));

/**
@brief Set function pointer to unlock function
@param lock_func pointer to unlock function
*/
void set_unlock_func(int (*unlock_func)(void));

/**
@brief Lock exclusive access to the concentrator
@return 0 if success or callbacks disabled, error number otherwise
*/
int lgw_lock(void);

/**
@brief Unlock exclusive access to the concentrator
@return 0 if success or callbacks disabled, error number otherwise
*/
int lgw_unlock(void);

#endif

/* --- EOF ------------------------------------------------------------------ */
