'''
P11 and P12 must be connected together for this test to pass.
'''

from machine import UART
from machine import Pin
import os
import time

# do not execute this test on the GPy and FiPy
if os.uname().sysname == 'GPy' or os.uname().sysname == 'FiPy':
    print("SKIP")
    import sys
    sys.exit()

uart = UART(2, 115200)
print(uart)
uart.init(57600, 8, None, 1, pins=('P11', 'P12'))
uart.init(baudrate=9600, stop=2, parity=UART.EVEN, pins=('P11', 'P12'))
uart.init(baudrate=115200, parity=UART.ODD, stop=1, pins=('P11', 'P12'))
uart.read()
print (uart.read())
print (uart.readline())
buff = bytearray(1)
print (uart.readinto(buff, 1))
print (uart.read())
print (uart.any())
print (uart.write('a'))
uart.deinit()

uart = UART(2, 1000000, pins=('P12', 'P11'))
print(uart)
uart.read()
print(uart.write(b'123456') == 6)
print(uart.read() == b'123456')
uart.deinit()
uart = UART(2, 1000000, pins=('P11', 'P12'))
print(uart)
uart.read()
print(uart.write(b'123456') == 6)
print(uart.read() == b'123456')
uart.deinit()

uart = UART(2, 1000000, pins=('P11', 'P12'))
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
    uart = UART(2, 1000000)
    uart.deinit()

# next ones must raise
try:
    UART(2, 9600, parity=None, pins=('GP12', 'GP13', 'GP7'))
except Exception:
    print('Exception')

try:
    UART(2, 9600, parity=UART.ODD, pins=('GP12', 'GP7'))
except Exception:
    print('Exception')

# buffer overflow
uart = UART(2, 1000000, pins=('P11', 'P12'))
buf = bytearray([0x55AA] * 567)
for i in range(200):
    r = uart.write(buf)
r = uart.read()
r = uart.read()
print(r)
print(uart.write(b'123456') == 6)
print(uart.read() == b'123456')
uart.deinit()
