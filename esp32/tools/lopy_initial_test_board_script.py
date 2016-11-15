#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

import time
import machine
from network import WLAN
wlan = WLAN(mode=WLAN.STA)

wifi_passed = False
lora_passed = False

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

from network import LoRa
import socket
lora = LoRa(mode=LoRa.LORA)

s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(False)

time.sleep(1.5)

def test_lora(ls):
    import time
    global lora_passed
    for i in range(3):
        if ls.recv(16) == b'Pycom':
            lora_passed = True
            break
        time.sleep(1.5)

test_lora(s)

if wifi_passed and lora_passed:
    print('Test OK')
else:
    print('Test failed')

time.sleep(0.5)
machine.reset()
