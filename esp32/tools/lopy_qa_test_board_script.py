#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

from network import LoRa
import binascii
import pycom
import os

EMPTY_MAC_ADDRESS = b'ffffffffffffffff'

lora = LoRa(mode=LoRa.LORA)
pycom.heartbeat(False)

try:
    if binascii.hexlify(lora.mac()) != EMPTY_MAC_ADDRESS and os.uname().release == "{FW_VERSION}" and 'LoPy' in os.uname().machine:
        pycom.rgbled(0x008000)      # green
        print('QA Test OK')
    else:
        pycom.rgbled(0x800000)      # red
        print('QA Test failed')
except Exception:
        pycom.rgbled(0x800000)      # red
        print('QA Test failed')

while True:
    pass
