#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

from network import WLAN
import binascii
import pycom
import time
import os

wlan = WLAN(mode=WLAN.STA)
wifi_passed = False

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

EMPTY_MAC_ADDRESS = b'ffffffffffff'

pycom.heartbeat(False)

try:
    f = open('/flash/sys/test.fct', 'r')
    initial_test_result = f.readall();
    if wifi_passed and binascii.hexlify(wlan.mac()) != EMPTY_MAC_ADDRESS and os.uname().release == "{FW_VERSION}" and 'WiPy' in os.uname().machine and initial_test_result == 'Test OK':
        pycom.rgbled(0x008000)      # green
        print('QA Test OK')
    else:
        pycom.rgbled(0x800000)      # red
        print('QA Test failed')
    f.close()
except Exception:
        pycom.rgbled(0x800000)      # red
        print('QA Test failed')

while True:
    pass
