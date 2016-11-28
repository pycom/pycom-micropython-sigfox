'''
P9 and P23 must be connected together for this test to pass.
'''

from machine import UART
from machine import Pin
import os
import time

uart_id_range = [1,2]

for uart_id in uart_id_range:
    uart = UART(uart_id, 38400)
    print(uart)
    uart.init(57600, 8, None, 1, pins=('P23', 'P9'))
    uart.init(baudrate=9600, stop=2, parity=UART.EVEN, pins=('P23', 'P9'))
    uart.init(baudrate=115200, parity=UART.ODD, stop=1, pins=('P23', 'P9'))
    #uart = UART(baudrate=1
    uart.read()
    print (uart.readall())
    print (uart.readline())
    buff=bytearray(1)
    print (uart.readinto(buff,1))
    print (uart.read())
    print (uart.any())
    print (uart.write('a'))
    #uart.deinit()


#uart = UART(baudrate=1000000)
#uart = UART()
#print(uart)
#uart = UART(baudrate=38400, pins=('GP12', 'GP13'))
#print(uart)
#uart = UART(pins=('GP12', 'GP13'))
#print(uart)
#uart = UART(pins=(None, 'GP17'))
#print(uart)
#uart = UART(baudrate=57600, pins=('GP16', 'GP17'))
#print(uart)


#loopback between ports
#Connected P9 to P23 and P10 to P22
#uart1 = UART(1, 1000000, pins=('P9', 'P10'))
#uart2 = UART(2, 1000000, pins=('P22', 'P23'))
#print(uart1.write(b'123456') == 6)
#print(uart2.read() == b'123456')


# now it's time for some loopback tests between pins
for uart_id in uart_id_range:
    uart1 = UART(uart_id, 1000000, pins=('P9', 'P23'))
    print(uart1)
    uart1.read()
    print(uart1.write(b'123456') == 6)
    print(uart1.read() == b'123456')
    uart1 = UART(uart_id, 1000000, pins=('P23', 'P9'))
    print(uart1)
    uart1.read()
    print(uart1.write(b'123456') == 6)
    print(uart1.read() == b'123456')


uart1 = UART(1, 1000000, pins=('P23', 'P9'))
print(uart1.write(b'123') == 3)
print(uart1.read(1) == b'1')
print(uart1.read(2) == b'23')
print(uart1.read() == None)

uart1.write(b'123')
buf = bytearray(3)
print(uart1.readinto(buf, 1) == 1)
print(buf)
print(uart1.readinto(buf) == 2)
print(buf)


'''
# try initializing without the id
uart0 = UART(baudrate=1000000, pins=uart_pins[0][0])
uart0.write(b'1234567890')
time.sleep_ms(2) # because of the fifo interrupt levels
print(uart1.any() == 10)
print(uart1.readline() == b'1234567890')
print(uart1.any() == 0)

uart0.write(b'1234567890')
print(uart1.readall() == b'1234567890')
'''

'''
# tx only mode
uart0 = UART(0, 1000000, pins=('GP12', None))
print(uart0.write(b'123456') == 6)
print(uart1.read() == b'123456')
print(uart1.write(b'123') == 3)
print(uart0.read() == None)

# rx only mode
uart0 = UART(0, 1000000, pins=(None, 'GP13'))
print(uart0.write(b'123456') == 6)
print(uart1.read() == None)
print(uart1.write(b'123') == 3)
print(uart0.read() == b'123')
'''

'''
# leave pins as they were (rx only mode)
uart0 = UART(0, 1000000, pins=None)
print(uart0.write(b'123456') == 6)
print(uart1.read() == None)
print(uart1.write(b'123') == 3)
print(uart0.read() == b'123')
'''


# check for memory leaks...
for i in range (0, 1000):
    uart1 = UART(1, 1000000)
    uart2 = UART(2, 1000000)

# next ones must raise
try:
    UART(1, 9600, parity=None, pins=('GP12', 'GP13', 'GP7'))
except Exception:
    print('Exception')

try:
    UART(1, 9600, parity=UART.ODD, pins=('GP12', 'GP7'))
except Exception:
    print('Exception')



'''
uart1 = UART(0, 1000000)
uart1.deinit()
try:
    uart0.any()
except Exception:
    print('Exception')

try:
    uart0.read()
except Exception:
    print('Exception')

try:
    uart0.write('abc')
except Exception:
    print('Exception')

try:
    uart0.sendbreak('abc')
except Exception:
    print('Exception')

try:
    UART(2, 9600)
except Exception:
    print('Exception')

'''
'''
for uart_id in uart_id_range:
    uart = UART(uart_id, 1000000)
    uart.deinit()
    # test printing an unitialized uart
    print(uart)
    # initialize it back and check that it works again
    uart.init(115200)
    print(uart)
    uart.read()
'''

#Buffer overflow
uart1 = UART(1, 1000000, pins=('P9', 'P23'))
buf = bytearray([0x55AA]*567)
print("Bach 0")
for i in range(1000):
    r = uart1.write(buf)
r = uart1.readall()
r = uart1.readall()
print(uart1.write(b'123456') == 6)
print(uart1.read() == b'123456')
