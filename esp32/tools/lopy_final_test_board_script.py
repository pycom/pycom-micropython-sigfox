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

EXPECTED_MAC_ADDRESS = b"{MAC_ADDRESS}"

lora = LoRa(mode=LoRa.LORA)

if binascii.hexlify(lora.mac()) == EXPECTED_MAC_ADDRESS  and os.uname().release == "{FW_VERSION}" and 'LoPy' in os.uname().machine:
    pycom.heartbeat(False)
    pycom.rgbled(0x008000)   # green
    print('Test OK')
    while True:
        pass
else:
    print('Test failed')