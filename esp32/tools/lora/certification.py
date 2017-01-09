#!/usr/bin/env python
#
# Copyright (c) 2016, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

# import certification
# from network import LoRa
# compliance = certification.Compliance(activation=LoRa.OTAA)
# compliance.run()

from network import LoRa
import time
import binascii
import socket
import struct


APP_EUI = 'AD A4 DA E3 AC 12 67 6B'
APP_KEY = '11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'

DEV_ADDR = '00 00 00 0A'
NWK_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'
APP_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'


class Compliance:
    def __init__(self, activation=LoRa.OTAA):
        self.lora = LoRa(mode=LoRa.LORAWAN)
        self.lora.compliance_test(True, 0, False)  # enable testing

        if activation == LoRa.OTAA:
            app_eui = binascii.unhexlify(APP_EUI.replace(' ',''))
            app_key = binascii.unhexlify(APP_KEY.replace(' ',''))
            self.lora.join(activation=LoRa.OTAA, auth=(app_eui, app_key), timeout=0)
        else:
            dev_addr = struct.unpack(">l", binascii.unhexlify(DEV_ADDR.replace(' ','')))[0]
            nwk_swkey = binascii.unhexlify(NWK_SWKEY.replace(' ',''))
            app_swkey = binascii.unhexlify(APP_SWKEY.replace(' ',''))
            self.lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

        # wait until the module has joined the network
        while not self.lora.has_joined():
            time.sleep(2.5)
            print("Waiting to join...")

        print("Network joined!")
        self.s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, False)

        self.tx_payload = bytes([(self.lora.compliance_test().downlink_counter >> 8) & 0xFF,
                                  self.lora.compliance_test().downlink_counter & 0xFF])

    def run(self):
        while True:
            while not self.lora.compliance_test().running:
                time.sleep(5.0)
                print('Sending ready packet')
                self.s.send('Ready')

            print('Test running!')
            self.s.setblocking(False)
            while self.lora.compliance_test().running:
                time.sleep(5.0)

                if self.lora.compliance_test().state < 6: # re-join
                    self.s.send(self.tx_payload)

                    if self.lora.compliance_test().link_check:
                        self.tx_payload = bytes([5, self.lora.compliance_test().demod_margin,
                                                    self.lora.compliance_test().nbr_gateways])
                        # set the state to 1 and clear the link check flag
                        self.lora.compliance_test(True, 1, False)
                    else:
                        if self.lora.compliance_test().state == 4:
                            rx_payload = self.s.recv(255)
                            if rx_payload:
                                self.tx_payload = bytes([rx_payload[0]])
                                for i in range(1, len(rx_payload)):
                                    self.tx_payload += bytes([(rx_payload[i] + 1) & 0xFF])
                            self.lora.compliance_test(True, 1)  # set the state to 1
                        else:
                            self.tx_payload = bytes([(self.lora.compliance_test().downlink_counter >> 8) & 0xFF,
                                                      self.lora.compliance_test().downlink_counter & 0xFF])
