#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

import time
import machine
import pycom
from network import WLAN
from machine import Pin
wlan = WLAN(mode=WLAN.STA)

wifi_passed = False

red_led = Pin('P10', mode=Pin.OUT, value=0)
green_led = Pin('P11', mode=Pin.OUT, value=0)

def test_wifi():
    global wifi_passed
    nets = wlan.scan()
    for net in nets:
        if net.ssid == 'pycom-test-ap':
            wifi_passed = True
            break

test_wifi()
if not wifi_passed: # try twice
    time.sleep(1.0)
    test_wifi()

f = open('/flash/sys/test.fct', 'w')

if wifi_passed:
    pycom.heartbeat(False)
    pycom.rgbled(0x008000)   # green
    green_led(1)
    f.write('Test OK')
    print('Test OK')
else:
    pycom.heartbeat(False)
    pycom.rgbled(0x800000)   # red
    red_led(1)
    f.write('Test failed')
    print('Test failed')

f.close()
time.sleep(0.5)
while True:
    pass
