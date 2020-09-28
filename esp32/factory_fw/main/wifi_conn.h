/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef _WIFI_CONN_H_
#define _WIFI_CONN_H_

/**
 * Function to connect to the WiFi. The WiFi credentials are read from Pycom Configurations in the flash.
 * Make sure that the Pycom configurations are initialized and the WiFi credentials are set.
 */
esp_err_t wifi_connect();

#endif /* _WIFI_CONN_H */
