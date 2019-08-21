'''
Copyright (c) 2019, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

from machine import UART


class Terminal:

    def __init__(self, pybytes_protocol):
        self.__pybytes_protocol = pybytes_protocol
        self.original_terminal = UART(0, 115200)
        self.message_from_pybytes = False
        self.message_to_send = ''

    def write(self, data):
        if self.message_from_pybytes:
            self.message_to_send += data
            # self.__pybytes_protocol.__send_terminal_message(data)
        else:
            self.original_terminal.write(data)

    def read(self, size):
        return self.original_terminal.read(size)

    def message_sent_from_pybytes_start(self):
        self.message_from_pybytes = True

    def message_sent_from_pybytes_end(self):
        if self.message_to_send != '':
            self.__pybytes_protocol.__send_terminal_message(
                self.message_to_send
            )
            self.message_to_send = ''
        self.message_from_pybytes = False
