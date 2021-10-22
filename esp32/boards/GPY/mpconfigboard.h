/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#define GPY

#define MICROPY_HW_BOARD_NAME                                   "GPy"
#define MICROPY_PY_SYS_PLATFORM                                 "GPy"
#define MICROPY_HW_HB_PIN_NUM                                   (0)
#define MICROPY_HW_SAFE_PIN_NUM                                 (21)

#define DEFAULT_AP_SSID                                         "gpy-wlan"

#define MICROPY_LTE_TX_PIN                                      (&PIN_MODULE_P5)
#define MICROPY_LTE_RX_PIN                                      (&PIN_MODULE_P98)
#define MICROPY_LTE_RTS_PIN                                     (&PIN_MODULE_P7)
#define MICROPY_LTE_CTS_PIN                                     (&PIN_MODULE_P99)

#define MICROPY_LTE_UART_ID                                     2
#define MICROPY_LTE_UART_BAUDRATE                               921600

extern uint32_t micropy_hw_flash_size;

extern uint32_t micropy_hw_antenna_diversity_pin_num;

extern bool micropy_lpwan_use_reset_pin;
extern uint32_t micropy_lpwan_reset_pin_num;
extern uint32_t micropy_lpwan_reset_pin_index;
extern void * micropy_lpwan_reset_pin;

extern uint32_t micropy_lpwan_dio_pin_num;
extern uint32_t micropy_lpwan_dio_pin_index;
extern void * micropy_lpwan_dio_pin;

extern uint32_t micropy_lpwan_ncs_pin_num;
extern uint32_t micropy_lpwan_ncs_pin_index;
extern void * micropy_lpwan_ncs_pin;
