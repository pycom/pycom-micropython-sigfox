/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#define WIPY

#define MICROPY_HW_BOARD_NAME                                   "WiPy"
#define MICROPY_PY_SYS_PLATFORM                                 "WiPy"
#define MICROPY_HW_HB_PIN_NUM                                   (0)
#define MICROPY_HW_SAFE_PIN_NUM                                 (21)

#define DEFAULT_AP_SSID                                         "wipy-wlan"

#define MICROPY_HW_FLASH_SIZE                                   (4 * 1024 * 1024)

