/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#define LOPY

#define MICROPY_HW_BOARD_NAME                                   "LoPy"
#define MICROPY_PY_SYS_PLATFORM                                 "LoPy"
#define MICROPY_HW_HB_PIN_NUM                                   (0)
#define MICROPY_HW_SAFE_PIN_NUM                                 (21)

#define DEFAULT_AP_SSID                                         "lopy-wlan"

#define MICROPY_HW_FLASH_SIZE                                   (4 * 1024 * 1024)

#if defined(OEM_VERSION)

    #define MICROPY_HW_ANTENNA_DIVERSITY                            (0)

    #define MICROPY_LPWAN_RESET_PIN_NUM
    #define MICROPY_LPWAN_RESET_PIN_NAME
    #define MICROPY_LPWAN_RESET_PIN
    #define MICROPY_LPWAN_USE_RESET_PIN                             0

    #define MICROPY_LPWAN_DIO_PIN_NUM                               (23)
    #define MICROPY_LPWAN_DIO_PIN_NAME                              GPIO23
    #define MICROPY_LPWAN_DIO_PIN                                   (pin_GPIO23)

    #define MICROPY_LPWAN_NCS_PIN_NUM                               (18)
    #define MICROPY_LPWAN_NCS_PIN_NAME                              GPIO18
    #define MICROPY_LPWAN_NCS_PIN                                   (pin_GPIO18)

#else

    #define MICROPY_HW_ANTENNA_DIVERSITY                            (1)
    #define MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM                    (16)

    #define MICROPY_LPWAN_RESET_PIN_NUM                             (18)
    #define MICROPY_LPWAN_RESET_PIN_NAME                            GPIO18
    #define MICROPY_LPWAN_RESET_PIN                                 (pin_GPIO18)
    #define MICROPY_LPWAN_USE_RESET_PIN                             1

    #define MICROPY_LPWAN_DIO_PIN_NUM                               (23)
    #define MICROPY_LPWAN_DIO_PIN_NAME                              GPIO23
    #define MICROPY_LPWAN_DIO_PIN                                   (pin_GPIO23)

    #define MICROPY_LPWAN_NCS_PIN_NUM                               (17)
    #define MICROPY_LPWAN_NCS_PIN_NAME                              GPIO17
    #define MICROPY_LPWAN_NCS_PIN                                   (pin_GPIO17)

#endif