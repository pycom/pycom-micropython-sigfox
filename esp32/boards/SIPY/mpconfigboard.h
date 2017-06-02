/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#define SIPY

#define MICROPY_HW_BOARD_NAME                                   "SiPy"
#define MICROPY_PY_SYS_PLATFORM                                 "SiPy"
#define MICROPY_HW_HB_PIN_NUM                                   (0)
#define MICROPY_HW_SAFE_PIN_NUM                                 (21)

#define DEFAULT_AP_SSID                                         "sipy-wlan"

#define MICROPY_HW_FLASH_SIZE                                   (4 * 1024 * 1024)

#if defined(OEM_VERSION)

    #define MICROPY_HW_ANTENNA_DIVERSITY                            (0)

#else

    #define MICROPY_HW_ANTENNA_DIVERSITY                            (1)
    #define MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM                    (16)

    #define MICROPY_LPWAN_DIO_PIN_NUM                               (23)
    #define MICROPY_LPWAN_DIO_PIN                                   (pin_GPIO23)

#endif