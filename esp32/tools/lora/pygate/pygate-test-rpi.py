from machine import UART
import machine
import time
import os
import gc

machine.pygate_init(None)
time.sleep(3)

uart = UART(1, 115200, timeout_chars=40, pins=('P23', 'P22'))

while True:
    if uart.any():
        rx_data = uart.read()
        print('rx_data: {}, len: {}'.format(rx_data, len(rx_data)))
        machine.pygate_cmd_decode(rx_data)
        tx_data =  machine.pygate_cmd_get()
        print('tx_data: ', tx_data)
        l = uart.write(tx_data)
    else:
        time.sleep_us(10)
