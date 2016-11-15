#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

import time

LORA_MAC_PATH='/flash/sys/lora.mac'

def MAC_to_bytearray(mac):
    import struct
    mac = mac.replace("-","").replace(":","")
    mac = int(mac, 16)
    return struct.pack('>Q', mac)

def write_mac(mac):
    with open(LORA_MAC_PATH, 'wb') as output:
        output.write(mac)
        time.sleep(0.5)

try:
    import machine
    new_mac = MAC_to_bytearray("{MAC_ADDRESS}")
    write_mac(new_mac)
    machine.reset()
except:
    print("LoRa MAC write failure")
