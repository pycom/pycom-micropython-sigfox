/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MACHUART_H_
#define MACHUART_H_

typedef enum {
    MACH_UART_0      =  0,
    MACH_UART_1      =  1,
    MACH_UART_2      =  2,
    MACH_NUM_UARTS
} mach_uart_id_t;

typedef struct _mach_uart_obj_t mach_uart_obj_t;
extern const mp_obj_type_t mach_uart_type;

void uart_init0 (void);
mach_uart_obj_t * uart_init (uint32_t uart_id, uint32_t baudrate);
uint32_t uart_rx_any(mach_uart_obj_t *self);
int uart_rx_char(mach_uart_obj_t *self);
bool uart_tx_char(mach_uart_obj_t *self, int c);
bool uart_tx_strn(mach_uart_obj_t *self, const char *str, uint len);

#endif  // MACHUART_H_
