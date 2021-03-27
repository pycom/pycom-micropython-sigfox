/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef ESP32_APP_SYS_EVT_H_
#define ESP32_APP_SYS_EVT_H_

#include "esp_event_loop.h"

#define APP_SYS_EVT_NUM                 2

typedef enum
{
    APP_SYS_EVT_ETH = 0,
    APP_SYS_EVT_WIFI
}main_app_sys_evt_t;

extern void app_sys_register_evt_cb(main_app_sys_evt_t sys_evt, system_event_cb_t cb);

#endif /* ESP32_APP_SYS_EVT_H_ */
