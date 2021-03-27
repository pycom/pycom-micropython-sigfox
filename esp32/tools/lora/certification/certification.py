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
import time
import binascii
import socket
import struct
import machine


DEV_EUI = '11 22 33 44 55 66 77 88'
APP_EUI = 'AD A4 DA E3 AC 12 67 6B'
APP_KEY = '11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'

DEV_ADDR = '00 00 00 0A'
NWK_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'
APP_SWKEY = '2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'

ACTIVATE_MSG = 'READY'


class Compliance:
    def __init__(self, region=LoRa.EU868, activation=LoRa.OTAA):
        self.lora = LoRa(mode=LoRa.LORAWAN, region=region)
        # enable testing
        self.lora.compliance_test(True, 0, False)
        self.activation = activation
        self.rejoined = False

    def _join(self):
        if self.activation == LoRa.OTAA:
            dev_eui = binascii.unhexlify(DEV_EUI.replace(' ', ''))
            app_eui = binascii.unhexlify(APP_EUI.replace(' ', ''))
            app_key = binascii.unhexlify(APP_KEY.replace(' ', ''))
            self.lora.join(activation=LoRa.OTAA, auth=(dev_eui, app_eui, app_key), timeout=0)
        else:
            dev_addr = struct.unpack('>l', binascii.unhexlify(DEV_ADDR.replace(' ', '')))[0]
            nwk_swkey = binascii.unhexlify(NWK_SWKEY.replace(' ', ''))
            app_swkey = binascii.unhexlify(APP_SWKEY.replace(' ', ''))
            self.lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

        # wait until the module has joined the network
        print('Joining.', end='', flush=True)
        time.sleep(0.1)
        while not self.lora.has_joined():
            time.sleep(0.5)
            print('.', end='', flush=True)

        print('')
        print('Network joined!')

        self.s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_DR, 2)
        self.s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, False)
        self.s.setblocking(True)

        print('Waiting for test activation...')

    def run(self):

        while True:
            if not self.rejoined:
                time.sleep(1)
                self._join()
            else:
                self.rejoined = False

            while not self.lora.compliance_test().running:
                time.sleep(1)
                self.s.send(ACTIVATE_MSG)

            print('Test running!')

            self.tx_payload = bytes([(self.lora.compliance_test().downlink_counter >> 8) & 0xFF,
                                      self.lora.compliance_test().downlink_counter & 0xFF])

            while self.lora.compliance_test().running:
                # re-join
                if self.lora.compliance_test().state < 6:
                    try:
                        self.s.send(self.tx_payload)
                    except Exception:
                        pass
                    time.sleep(1.5)

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
                            # set the state to 1
                            self.lora.compliance_test(True, 1)
                        else:
                            self.tx_payload = bytes([(self.lora.compliance_test().downlink_counter >> 8) & 0xFF,
                                                      self.lora.compliance_test().downlink_counter & 0xFF])
                else:
                    self.rejoined = True
                    time.sleep(3.5)
                    self._join()

            # The test has been disabled, send one more message in order to signal that test mode
            # has been deactivated and then wait a few seconds before trying to join again
            time.sleep(3)
            self.s.send(ACTIVATE_MSG)
            if self.activation == LoRa.OTAA:
                time.sleep(3)
            else:
                time.sleep(5)
            if not self.rejoined:
                machine.deepsleep(1000)
