/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
  A communication bridge layer to abstract linux/windows OS or others.
  The current project support only linux os

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>             /* C99 types */
#include <stdio.h>              /* printf fprintf */
#include <stdlib.h>             /* malloc free */
#include <unistd.h>             /* lseek, close */
#include <fcntl.h>              /* open */
#include <string.h>             /* memset */
#include <errno.h>              /* Error number definitions */
#include <termios.h>            /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>

#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_com.h"
#include "loragw_com_linux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_COM == 1
#define DEBUG_MSG(str)                fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_COM_ERROR;}
#else
#define DEBUG_MSG(str)
#define DEBUG_PRINTF(fmt, args...)
#define CHECK_NULL(a)                if(a==NULL){return LGW_COM_ERROR;}
#endif

#define UNUSED(x) (void)(x)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE SHARED VARIABLES (GLOBAL) ------------------------------------ */

extern void *lgw_com_target; /*! generic pointer to the COM device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_com_open(void **com_target_ptr) {
#ifdef _WIN32
    return lgw_com_open_win(com_target_ptr);
#elif __linux__
    return lgw_com_open_linux(com_target_ptr);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_close(void *com_target) {
#ifdef _WIN32
    return lgw_com_close_win(com_target);
#elif __linux__
    return lgw_com_close_linux(com_target);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_w(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t data) {
#ifdef _WIN32
    return lgw_com_w_win(com_target, com_mux_mode, com_mux_target, address, data);
#elif __linux__
    return lgw_com_w_linux(com_target, com_mux_mode, com_mux_target, address, data);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_r(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data) {
#ifdef _WIN32
    return lgw_com_r_win(com_target, com_mux_mode, com_mux_target, address, data);
#elif __linux__
    return lgw_com_r_linux(com_target, com_mux_mode, com_mux_target, address, data);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_wb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
#ifdef _WIN32
    return lgw_com_wb_win(com_target, com_mux_mode, com_mux_target, address, data, size);
#elif __linux__
    return lgw_com_wb_linux(com_target, com_mux_mode, com_mux_target, address, data, size);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_com_rb(void *com_target, uint8_t com_mux_mode, uint8_t com_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
#ifdef _WIN32
    return lgw_com_rb_win(com_target, com_mux_mode, com_mux_target, address, data, size);
#elif __linux__
    return lgw_com_rb_linux(com_target, com_mux_mode, com_mux_target, address, data, size);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int SendCmd(CmdSettings_t CmdSettings, lgw_handle_t handle) {
#ifdef _WIN32
    return SendCmd_win(CmdSettings, handle);
#elif __linux__
    return SendCmd_linux(CmdSettings, handle);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int ReceiveAns(AnsSettings_t *Ansbuffer, lgw_handle_t handle) {
#ifdef _WIN32
    return ReceiveAns_win(Ansbuffer, handle);
#elif __linux__
    return ReceiveAns_linux(Ansbuffer, handle);
#elif __APPLE__
    DEBUG_PRINTF("System is not recognized.");
#elif __unix__
    DEBUG_PRINTF("System is not recognized.");
#elif __posix__
    DEBUG_PRINTF("System is not recognized.");
#else
    DEBUG_PRINTF("System is not recognized.");
#endif
}

/* --- EOF ------------------------------------------------------------------ */
