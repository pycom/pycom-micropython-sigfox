'''
P9 and P23 must be connected together for this test to pass.
'''

from machine import UART
from machine import Pin
import os
import time

uart_id_range = [1, 2]

for uart_id in uart_id_range:
    uart = UART(uart_id, 115200)
    print(uart)
    uart.init(57600, 8, None, 1, pins=('P23', 'P9'))
    uart.init(baudrate=9600, stop=2, parity=UART.EVEN, pins=('P23', 'P9'))
    uart.init(baudrate=115200, parity=UART.ODD, stop=1, pins=('P23', 'P9'))
    uart.read()
    print (uart.readall())
    print (uart.readline())
    buff=bytearray(1)
    print (uart.readinto(buff,1))
    print (uart.read())
    print (uart.any())
    print (uart.write('a'))
    uart.deinit()


# now it's time for some loopback tests between pins
for uart_id in uart_id_range:
    uart = UART(uart_id, 1000000, pins=('P9', 'P23'))
    print(uart)
    uart.read()
    print(uart.write(b'123456') == 6)
    print(uart.read() == b'123456')
    uart.deinit()
    uart = UART(uart_id, 1000000, pins=('P23', 'P9'))
    print(uart)
    uart.read()
    print(uart.write(b'123456') == 6)
    print(uart.read() == b'123456')
    uart.deinit()


for uart_id in uart_id_range:
    uart = UART(uart_id, 1000000, pins=('P23', 'P9'))
    print(uart.write(b'123') == 3)
    print(uart.read(1) == b'1')
    print(uart.read(2) == b'23')
    print(uart.read() == None)

    uart.write(b'123')
    buf = bytearray(3)
    print(uart.readinto(buf, 1) == 1)
    print(buf)
    print(uart.readinto(buf) == 2)
    print(buf)
    uart.deinit()

# check for memory leaks...
for i in range (0, 1000):
    uart = UART(1, 1000000)
    uart.deinit()
    uart = UART(2, 1000000)
    uart.deinit()

# next ones must raise
try:
    UART(1, 9600, parity=None, pins=('GP12', 'GP13', 'GP7'))
except Exception:
    print('Exception')

try:
    UART(1, 9600, parity=UART.ODD, pins=('GP12', 'GP7'))
except Exception:
    print('Exception')


# buffer overflow
for uart_id in uart_id_range:
    uart = UART(uart_id, 1000000, pins=('P9', 'P23'))
    buf = bytearray([0x55AA]*567)
    for i in range(200):
        r = uart.write(buf)
    r = uart.readall()
    r = uart.readall()
    print(r)
    print(uart.write(b'123456') == 6)
    print(uart.read() == b'123456')
    uart.deinit()
