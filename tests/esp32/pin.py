"""
This test need a set of pins which can be set as inputs and have no external
pull up or pull down connected.
P9 and P23 must be connected together
"""
from machine import Pin
from machine import UART
import os

mch = os.uname().machine
'''
pin_map = ['P0','P1','P2','P3','P4','P5','P6','P7','P8','P9','P10','P11','P13',
           'P14','P15','P16','P17','P18','P19','P20','P21','P22','P23',
           'G2','G1','G23','G24','G11','G12','G13','G14','G15','G16','G17',
           'G22','G28','G5','G4','G0','G3','G31','G30','G6','G7','G8','G9','G10',
           'GPIO03','GPIO1','GPIO0','GPIO4','GPIO15','GPIO5','GPIO27','GPIO19',
           'GPIO19','GPIO2','GPIO12','GPIO13','GPIO22','GPIO21','GPI37','GPI36',
           'GPI38','GPI39','GPI35','GPI34','GPI32','GPIO33','GPIO26','GPIO25',
           'GPIO14']
'''
pin_map = [
    'G2','GPIO3','P0',
    'G1','GPIO1','P1',
    'G23','GPIO0','P2',
    'G24','GPIO4','P3',
    'G11','GPIO15','P4',
    'G12','GPIO5','P5',
    'G13','GPIO27','P6',
    'G14','GPIO19','P7',
    'G15','GPIO2','P8',
    'G16','GPIO12','P9',
    'G17','GPIO13','P10',
    'G22','GPIO22','P11',
    'G28','GPIO21','P12',
    'G5','GPI37','P13',
    'G4','GPI36','P14',
    'G0','GPI38','P15',
    'G3','GPI39','P16',
    'G31','GPI35','P17',
    'G30','GPI34','P18',
    'G6','GPIO32','P19',
    'G7','GPIO33','P20',
    'G8','GPIO26','P21',
    'G9','GPIO25','P22',
    'G10','GPIO14','P23'
]

if 'LoPy' in mch:
    used_pins = ['G12','P5',
                 'G13','P6',
                 'G14','P7']
    pin_map = [x if x in used_pins else 'P2' for x in pin_map]


# test initial value
p = Pin('P9', Pin.IN)
out = Pin('P23', Pin.OUT, value=1)
print(p() == 1)
put = Pin('P23', Pin.OUT, value=0)
print(p() == 0)

def test_noinit():
    for p in pin_map:
        pin = Pin(p)
        val = pin.value()
        uart = UART(0, 115200)
        print (type(pin))
        print (val)

# test un-initialized pins
test_noinit()
def test_pin_read(pull):
    # enable the pull resistor on all pins, then read the value
    for p in pin_map:
        pin = Pin(p, mode=Pin.IN, pull=pull)
        val = pin()
        uart = UART(0, 115200)
        print (val)
# test with pull-up and pull-down
print("PULL-UP")
test_pin_read(Pin.PULL_UP)
print("PULL-DOWN")
test_pin_read(Pin.PULL_DOWN)


def test_pin_af():
    for p in pin_map:
        for af in Pin(p).alt_list():
            if af[1] <= max_af_idx:
                Pin(p, mode=Pin.ALT, alt=af[1])
                Pin(p, mode=Pin.ALT_OPEN_DRAIN, alt=af[1])


#test_pin_af() # try the entire af range on all pins

# test all constructor combinations
magic_pin = pin_map[2]
pin = Pin(magic_pin)
pin = Pin(magic_pin, mode=Pin.IN)
pin = Pin(magic_pin, mode=Pin.OUT)
pin = Pin(magic_pin, mode=Pin.IN, pull=Pin.PULL_DOWN)
pin = Pin(magic_pin, mode=Pin.IN, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OPEN_DRAIN, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_DOWN)
pin = Pin(magic_pin, mode=Pin.OUT, pull=None)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP, drive=0)
pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP, drive=100)
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
# drive
#pin.drive(Pin.MED_POWER)
#print(pin.drive() == Pin.MED_POWER)
#pin.drive(Pin.HIGH_POWER)
#print(pin.drive() == Pin.HIGH_POWER)
# id
print(pin.id() == magic_pin)

# all the next ones MUST raise
'''
try:
    pin = Pin(magic_pin, mode=Pin.OUT, pull=Pin.PULL_UP, drive=pin.IN) # incorrect drive value
except Exception:
    print('Exception')
'''
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

'''try:
    pin = Pin(magic_pin, Pin.IN, Pin.PULL_UP, alt=0) # af specified in GPIO mode
except Exception:
    print('Exception')

try:
    pin = Pin(magic_pin, Pin.OUT, Pin.PULL_UP, alt=7) # af specified in GPIO mode
except Exception:
    print('Exception')

try:
    pin = Pin(magic_pin, Pin.ALT, Pin.PULL_UP, alt=0) # incorrect af
except Exception:
    print('Exception')

try:
    pin = Pin(magic_pin, Pin.ALT_OPEN_DRAIN, Pin.PULL_UP, alt=-1) # incorrect af
except Exception:
    print('Exception')

try:
    pin = Pin(magic_pin, Pin.ALT_OPEN_DRAIN, Pin.PULL_UP, alt=16) # incorrect af
except Exception:
    print('Exception')
'''
'''
try:
    pin.mode(Pin.PULL_UP) # incorrect pin mode
except Exception:
    print('Exception')

try:
    pin.pull(Pin.OUT) # incorrect pull
except Exception:
    print('Exception')

try:
    pin.drive(Pin.IN) # incorrect drive strength
except Exception:
    print('Exception')
'''
try:
    pin.id('ABC') # id cannot be set
except Exception:
    print('Exception')

#test pin object
p = Pin(magic_pin, Pin.IN)
print(p)
print(p.id())
print(p.mode())
print(p.pull())
print(p.value())
print(p.drive())
#print(p.alt())
