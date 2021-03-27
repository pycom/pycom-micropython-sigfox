'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''


class PeriodicalPin:

    TYPE_DIGITAL = 0
    TYPE_ANALOG = 1
    TYPE_VIRTUAL = 2

    def __init__(self, persistent, pin_number, message_type, message, pin_type):
        self.pin_number = pin_number
        self.message_type = message_type
        self.message = message
        self.pin_type = pin_type
