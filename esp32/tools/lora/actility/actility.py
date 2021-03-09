#!/usr/bin/env python
#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

from network import LoRa
from machine import ADC
import time
import binascii
import socket
import struct


DEV_EUI = '1A 2B 3C 4D 01 02 03'
APP_EUI = 'AD A4 DA E3 AC 12 67 6B'
APP_KEY = '11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'

DEV_ADDR = '00 00 00 0A'
NWK_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'
APP_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'


class Actility:
    def __init__(self, activation=LoRa.OTAA, adr=False):
        self.lora = LoRa(mode=LoRa.LORAWAN, adr=adr)
        self.activation = activation

        self._join()

        self.s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_DR, 3)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, False)
        self.s.setblocking(False)

        self.adc = ADC()
        self.adc_c = self.adc.channel(pin='P13')

    def _join(self):
        if self.activation == LoRa.OTAA:
            dev_eui = binascii.unhexlify(DEV_EUI.replace(' ',''))
            app_eui = binascii.unhexlify(APP_EUI.replace(' ',''))
            app_key = binascii.unhexlify(APP_KEY.replace(' ',''))
            self.lora.join(activation=LoRa.OTAA, auth=(dev_eui, app_eui, app_key), timeout=0)
        else:
            dev_addr = struct.unpack(">l", binascii.unhexlify(DEV_ADDR.replace(' ','')))[0]
            nwk_swkey = binascii.unhexlify(NWK_SWKEY.replace(' ',''))
            app_swkey = binascii.unhexlify(APP_SWKEY.replace(' ',''))
            self.lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

        # wait until the module has joined the network
        while not self.lora.has_joined():
            time.sleep(5)
            print("Joining...")

        print("Network joined!")

    def run(self):
        while True:
            time.sleep(10)
            tx_data = '%d' % self.adc_c()
            print('Sending', tx_data)
            self.s.send(tx_data)
