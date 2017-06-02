/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2017 Semtech-Cycleo

Description:
 Utility to jump to the PicoCell MCU bootloader

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* EXIT_FAILURE */

#include "loragw_com.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define MSG(args...)    fprintf(stderr, args) /* message that is destined to the user */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define COM_PATH_DEFAULT "/dev/ttyACM0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

void *lgw_com_target = NULL; /*! generic pointer to the COM device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

void usage (void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* describe command line options */
void usage(void) {
    MSG("Available options:\n");
    MSG(" -h print this help\n");
    MSG(" -d <path> COM device to be used to access the concentrator board\n");
    MSG("            => default path: " COM_PATH_DEFAULT "\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv) {
    int i, x;
    lgw_com_cmd_t cmd;
    lgw_com_ans_t ans;
    /* COM interfaces */
    const char com_path_default[] = COM_PATH_DEFAULT;
    const char *com_path = com_path_default;

    while ((i = getopt (argc, argv, "hd:")) != -1) {
        switch (i) {
            case 'h':
                usage();
                return EXIT_FAILURE;
                break;

            case 'd':
                if (optarg != NULL) {
                    com_path = optarg;
                }
                break;

            default:
                MSG("ERROR: argument parsing use -h option for help\n");
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Open communication bridge */
    x = lgw_com_open(&lgw_com_target, com_path);
    if (x == LGW_COM_ERROR) {
        printf("ERROR: FAIL TO CONNECT BOARD ON %s\n", com_path);
        return -1;
    }

    /* prepare command to jump to bootloader */
    cmd.id = 'n';
    cmd.len_msb = 0;
    cmd.len_lsb = 0;
    cmd.address = 0;
    /* send command to MCU */
    x = lgw_com_send_command(lgw_com_target, cmd, &ans);
    if (x == LGW_COM_ERROR) {
        printf("ERROR: FAIL TO SEND COMMAND\n");
        return -1;
    }

    /* Close communication bridge */
    x = lgw_com_close(lgw_com_target);
    if (x == LGW_COM_ERROR) {
        printf("ERROR: FAIL TO DISCONNECT BOARD\n");
        return -1;
    }

    return EXIT_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */


