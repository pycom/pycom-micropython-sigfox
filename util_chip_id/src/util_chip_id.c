/*
/ _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
\____ \| ___ |    (_   _) ___ |/ ___)  _ \
_____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
 (C)2013 Semtech-Cycleo

Description:
  Get unique ID of the PicoCell gateway board

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
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */

#include <signal.h>     /* sigaction */
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* rand */

#include "loragw_reg.h"
#include "loragw_mcu.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define MSG(args...)    fprintf(stderr, args) /* message that is destined to the user */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define COM_PATH_DEFAULT "/dev/ttyACM0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

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
    MSG(" -l generate a new guid.json file\n");
    MSG(" -p print the ID of the PicoCell gateway\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv) {
    int i, x;
    uint8_t uid[8];
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

    x = lgw_connect(com_path);
    if (x == -1) {
        printf("ERROR: FAIL TO CONNECT BOARD ON %s\n", com_path);
        return -1;
    }

    lgw_mcu_get_unique_id(&uid[0]);
    printf("%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);

    x = lgw_disconnect();
    if (x == -1) {
        printf("ERROR: FAIL TO DISCONNECT BOARD\n");
        return -1;
    }

    return EXIT_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */


