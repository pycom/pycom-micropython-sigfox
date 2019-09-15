"""
This test need a set of pins which can be set as inputs and have no external
pull up or pull down connected.
THIS TEST ONLY PASSES USING EXPANSION BOARD 2.1
ON EXPANSION BOARD 3.0 THE TEST FAILS ON LOPY4 AND WIPY DUE TO DIFFERENT REASONS
"""
from machine import Pin
from machine import UART
import os

mch = os.uname().machine

all_pins = [
    #'G2','P0',   #Pin used for UART
    #'G1','P1',   #Pin used for UART
    #'G23','P2',  #Pin used for ISP
    'G24','P3',
    'G11','P4',
    'G12','P5',
    'G13','P6',
    'G14','P7',
    'G15','P8',
    #'G16','P9',   #Pin used for I2C SDA
    #'G17','P10',  #Pin used for I2C SCLK
    'G22','P11',
    #'G28','P12',  #Pin used for factory reset
    'G5','P13',
    'G4','P14',
    #'G0','P15',   #Pin used for LTE_WAKE
    'G3','P16',
    #'G31','P17',  #Pin used for LTE_CTS
    #'G30','P18'  #Pin used for LTE_RX
    #'G6','P19',  #Pin used for LTE_RTS
    #'G7','P20',  #Pin used for LTE_TX
    'G8','P21',
    #'G9','P22'    #Pin used for the onewire sensor
    #'G10','P23'   #Pin used for I2C SDA loop
]

no_pull_up_pins = [
    'G5','P13',
    'G4','P14',
    'G0','P15',
    'G3','P16',
    'G31','P17',
    'G30','P18',
]
used_pins = []

used_pins = [
             'G12','P5',   #Pin used for Lora CLK
             'G13','P6',   #Pin used for Lora MOSI
             'G14','P7'    #Pin used for Lora MISO
             ''
]

pin_map = list(set(all_pins) - set(used_pins))
pin_map += ['P12'] * (len(all_pins)-len(pin_map))

# test initial value
p = Pin('P9', Pin.IN)
out = Pin('P23', Pin.OUT, value=1)
print(p() == 1)

def test_noinit():
    for p in pin_map:
        pin = Pin(p)
        val = pin.value()
        print (type(pin))

# test un-initialized pins
test_noinit()

def test_pin_read(pull):
    # enable the pull resistor on all pins, then read the value
    for p in pin_map:
        pin = Pin(p, mode=Pin.IN, pull=pull)
        val = pin()
        if p not in no_pull_up_pins:
            print ( val == 1 )

# test with pull-up and pull-down
print("PULL-UP")
test_pin_read(Pin.PULL_UP)
print("PULL-DOWN")
test_pin_read(Pin.PULL_DOWN)

# test all constructor combinations
magic_pin = pin_map[0]
pin = Pin(magic_pin)
pin = Pin(magic_pin, mode=Pin.IN)
pin = Pin(magic_pin, mode=Pin.OUT)
pin = Pin(magic_pin, mode=Pin.IN, pull=Pin.PULL_DOWN)
pin = Pin(magic_pin, mode=Pin.IN, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OPEN_DRAIN, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_DOWN)
pin = Pin(magic_pin, mode=Pin.OUT, pull=None)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP)
pin = Pin(magic_pin,value=1, mode=Pin.OUT)
pin = Pin(magic_pin,value=0, mode=Pin.OUT)
pin = Pin(magic_pin,value=1, mode=Pin.IN)
pin = Pin(magic_pin,value=0, mode=Pin.IN)
pin = Pin(magic_pin,)
pin = Pin(magic_pin, Pin.OUT, Pin.PULL_DOWN)
pin = Pin(magic_pin, Pin.OUT, alt=1)
pin = Pin(magic_pin, Pin.OUT, alt=-1)
print("Constructor pass")

# test all getters and setters
pin = Pin(magic_pin, mode=Pin.OUT)
# mode
print(pin.mode() == Pin.OUT)
pin.mode(Pin.IN)
print(pin.mode() == Pin.IN)
# pull
pin.pull(None)
print(pin.pull() == None)
pin.pull(Pin.PULL_DOWN)
print(pin.pull() == Pin.PULL_DOWN)
# id
print(pin.id() == magic_pin)

# all the next ones MUST raise
try:
    pin = Pin(magic_pin, mode=Pin.LOW_POWER, pull=Pin.PULL_UP) # incorrect mode value
except Exception:
    print('Exception')

try:
    pin = Pin(magic_pin, mode=Pin.IN, pull=Pin.HIGH_POWER) # incorrect pull value
except Exception:
    print('Exception')

try:
    pin = Pin('A0', Pin.OUT, Pin.PULL_DOWN) # incorrect pin id
except Exception:
    print('Exception')

try:
    pin.mode(Pin.PULL_UP) # incorrect pin mode
except Exception:
    print('Exception')

try:
    pin.pull(Pin.OUT) # incorrect pull
except Exception:
    print('Exception')

try:
    pin.id('ABC') # id cannot be set
except Exception:
    print('Exception')

# test the magic pin object
p = Pin(magic_pin, Pin.IN)
print(p)
print(p.id())
print(p.mode())
print(p.pull())
print(p.value())
