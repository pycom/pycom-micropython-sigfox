/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "py/mpconfig.h"
#include "py/misc.h"
#include "py/nlr.h"
#include "py/mphal.h"
#include "serverstask.h"
//#include "debug.h"
#include "telnet.h"
#include "ftp.h"
//#include "pybwdt.h"
#include "modusocket.h"
#include "mpexception.h"
#include "modnetwork.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    uint32_t timeout;
    bool enabled;
    bool do_disable;
    bool do_enable;
    bool do_reset;
    bool do_wlan_cycle_power;
    bool reset_and_safe_boot;
} servers_data_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile servers_data_t servers_data = {.timeout = SERVERS_DEF_TIMEOUT_MS};
static volatile bool sleep_sockets = false;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
char servers_user[SERVERS_USER_PASS_LEN_MAX + 1];
char servers_pass[SERVERS_USER_PASS_LEN_MAX + 1];
extern TaskHandle_t svTaskHandle;
/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void TASK_Servers (void *pvParameters) {
    bool cycle = false;

    strcpy (servers_user, SERVERS_DEF_USER);
    strcpy (servers_pass, SERVERS_DEF_PASS);

    telnet_init();
    ftp_init();

    for ( ; ; ) {

        if (servers_data.do_enable) {
            // enable network services
            telnet_enable();
            ftp_enable();
            // now set/clear the flags
            servers_data.enabled = true;
            servers_data.do_enable = false;
        }
        else if (servers_data.do_disable) {
            // disable network services
            telnet_disable();
            ftp_disable();
            // now clear the flags
            servers_data.do_disable = false;
            servers_data.enabled = false;
        }
        else if (servers_data.do_reset) {
            // resetting the servers is needed to prevent half-open sockets
            servers_data.do_reset = false;
            if (servers_data.enabled) {
                telnet_reset();
                ftp_reset();
            }
            // and we should also close all user sockets. We do it here
            // for convinience and to save on code size.
            modusocket_close_all_user_sockets();
        }

        if (cycle) {
            telnet_run();
        } else {
            ftp_run();
        }

        if (sleep_sockets) {
//            pybwdt_srv_sleeping(true);  //  FIXME
//            modusocket_enter_sleep();   //  FIXME
//            pybwdt_srv_sleeping(false); // FIXME
            vTaskDelay((SERVERS_CYCLE_TIME_MS * 2) / portTICK_PERIOD_MS);
            if (servers_data.do_wlan_cycle_power) {
                servers_data.do_wlan_cycle_power = false;
//                wlan_off_on(); // FIXME
            }
            sleep_sockets = false;
        }

        // set the alive flag for the wdt
//        pybwdt_srv_alive(); // FIXME

        if (servers_data.reset_and_safe_boot) {
            mp_hal_reset_safe_and_boot(true);
        }

        // move to the next cycle
        cycle = cycle ? false : true;
        vTaskDelay (SERVERS_CYCLE_TIME_MS / portTICK_PERIOD_MS);
    }
}

void servers_start (void) {
    servers_data.do_enable = true;
    vTaskDelay((SERVERS_CYCLE_TIME_MS * 3) / portTICK_PERIOD_MS);
}

void servers_stop (void) {
    //get task handle to check if this is called from servers task
    TaskHandle_t cuur_task = xTaskGetCurrentTaskHandle();
    if(cuur_task == svTaskHandle)
    {
        // disable network services
        telnet_disable();
        ftp_disable();
        // now clear the flags
        servers_data.enabled = false;
    }
    else
    {
        servers_data.do_disable = true;
        do {
            vTaskDelay(SERVERS_CYCLE_TIME_MS / portTICK_PERIOD_MS);
        } while (servers_are_enabled());
    }
    vTaskDelay((SERVERS_CYCLE_TIME_MS * 3) / portTICK_PERIOD_MS);
}

void servers_reset (void) {
    servers_data.do_reset = true;
}

void servers_wlan_cycle_power (void) {
    servers_data.do_wlan_cycle_power = true;
}

void servers_reset_and_safe_boot (void) {
    servers_data.reset_and_safe_boot = true;
}

bool servers_are_enabled (void) {
    return servers_data.enabled;
}

void server_sleep_sockets (void) {
    sleep_sockets = true;
    vTaskDelay((SERVERS_CYCLE_TIME_MS + 1) / portTICK_PERIOD_MS);
}

void servers_close_socket (int32_t *sd) {
    if (*sd > 0) {
        modusocket_socket_delete(*sd);
        closesocket(*sd);
        *sd = -1;
    }
}

void servers_set_login (char *user, char *pass) {
    if (strlen(user) > SERVERS_USER_PASS_LEN_MAX || strlen(pass) > SERVERS_USER_PASS_LEN_MAX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
    memcpy(servers_user, user, SERVERS_USER_PASS_LEN_MAX);
    memcpy(servers_pass, pass, SERVERS_USER_PASS_LEN_MAX);
}

void servers_set_timeout (uint32_t timeout) {
    if (timeout < SERVERS_MIN_TIMEOUT_MS) {
        // timeout is too low
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
    servers_data.timeout = timeout;
}

uint32_t servers_get_timeout (void) {
    return servers_data.timeout;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
