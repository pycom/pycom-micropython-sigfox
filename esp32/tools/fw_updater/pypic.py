#!/usr/bin/env python

#   Copyright (c) 2016-2020, Pycom Limited.
#
#   This software is licensed under the GNU GPL version 3 or any
#   later version, with permitted additional terms. For more information
#   see the Pycom Licence v1.0 document supplied with this file, or
#   available at https://www.pycom.io/opensource/licensing

from __future__ import print_function

import argparse
import struct
import sys
import time

import serial

try:
    from local_settings import DEBUG
except:
    DEBUG = False
__version__ = '0.9.3'

CMD_PEEK = (0x0)
CMD_POKE = (0x01)
CMD_MAGIC = (0x02)
CMD_HW_VER = (0x10)
CMD_FW_VER = (0x11)
CMD_PROD_ID = (0x12)
CMD_SETUP_SLEEP = (0x20)
CMD_GO_SLEEP = (0x21)
CMD_CALIBRATE = (0x22)
CMD_BAUD_CHANGE = (0x30)
CMD_DFU = (0x31)

ANSELA_ADDR = (0x18C)
ANSELB_ADDR = (0x18D)
ANSELC_ADDR = (0x18E)

ADCON0_ADDR = (0x9D)
ADCON1_ADDR = (0x9E)

IOCAP_ADDR = (0x391)
IOCAN_ADDR = (0x392)

_ADCON0_CHS_POSN = (0x02)
_ADCON0_ADON_MASK = (0x01)
_ADCON1_ADCS_POSN = (0x04)
_ADCON0_GO_nDONE_MASK = (0x02)

ADRESL_ADDR = (0x09B)
ADRESH_ADDR = (0x09C)

TRISA_ADDR = (0x08C)
TRISC_ADDR = (0x08E)

PORTA_ADDR = (0x00C)
PORTC_ADDR = (0x00E)

WPUA_ADDR = (0x20C)

PCON_ADDR = (0x096)
STATUS_ADDR = (0x083)


# helper functions
def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def log(*args):
    print(' '.join(str(a) for a in args))


def error(msg):
    eprint('error:', msg)


def exit_with_error(code, msg):
    error(msg)
    sys.exit(code)


def warn(msg):
    eprint('warning:', msg)


def print_debug(msg):
    if DEBUG:
        eprint(msg)


class Pypic:

    def __init__(self, port):
        # we need bytesize to be 5 bits in order for the PIC to process the commands
        try:
            self.serial = serial.Serial(port, baudrate=115200, bytesize=serial.FIVEBITS, timeout=0.25, exclusive=True)
            connected = True
        except Exception as e:
            connected = False
            print_debug("Not connecting in exclusive mode because: %s" % str(e))
        if not connected:
            try:
                self.serial = serial.Serial(port, baudrate=115200, bytesize=serial.FIVEBITS, timeout=0.25)
            except Exception as e:
                raise e
        self.detected = False

        try:
            if self.read_fw_version() < 6:
                raise ValueError('PIC firmware out of date')
            else:
                self.detected = True
        except Exception:
            pass

    def _write(self, data, read=True, num_read_bytes=1):
        self.serial.write(data)
        if read:
            r_data = self.serial.read(2)
            if not r_data:
                raise Exception('Timeout while waiting for Rx data')
            if num_read_bytes == 2:
                (b1, b2) = struct.unpack('BB', r_data)
                return 256 * b2 + b1
            else:
                if sys.version_info[0] < 3:
                    return struct.unpack('B', r_data[0])[0]
                else:
                    return r_data[0]

    def _flush(self, num_read_bytes=10):
        r_data = self.serial.read(num_read_bytes)
        # print(r_data)
        return

    def _send_cmd(self, cmd, num_read_bytes=1):
        return self._write(bytearray([cmd]), True, num_read_bytes)

    def read_hw_version(self):
        return self._send_cmd(CMD_HW_VER, 2)

    def read_fw_version(self):
        return self._send_cmd(CMD_FW_VER, 2)

    def read_product_id(self):
        return self._send_cmd(CMD_PROD_ID, 2)

    def peek_memory(self, addr):
        return self._write(bytearray([CMD_PEEK, addr & 0xFF, (addr >> 8) & 0xFF]))

    def poke_memory(self, addr, value):
        self._write(bytearray([CMD_POKE, addr & 0xFF, (addr >> 8) & 0xFF, value & 0xFF]), False)

    def magic_write_read(self, addr, _and=0xFF, _or=0, _xor=0):
        return self._write(bytearray([CMD_MAGIC, addr & 0xFF, (addr >> 8) & 0xFF, _and & 0xFF, _or & 0xFF, _xor & 0xFF]))

    def magic_write(self, addr, _and=0xFF, _or=0, _xor=0):
        self._write(bytearray([CMD_MAGIC, addr & 0xFF, (addr >> 8) & 0xFF, _and & 0xFF, _or & 0xFF, _xor & 0xFF]), False)

    def toggle_bits_in_memory(self, addr, bits):
        self.magic_write(addr, _xor=bits)

    def mask_bits_in_memory(self, addr, mask):
        self.magic_write(addr, _and=mask)

    def set_bits_in_memory(self, addr, bits):
        self.magic_write(addr, _or=bits)

    def reset_pycom_module(self):
        # make RC5 an output
        self.mask_bits_in_memory(TRISC_ADDR, ~(1 << 5))
        # drive RC5 low
        self.mask_bits_in_memory(PORTC_ADDR, ~(1 << 5))
        time.sleep(0.2)
        # drive RC5 high
        self.set_bits_in_memory(PORTC_ADDR, 1 << 5)
        time.sleep(0.1)
        # make RC5 an input
        self.set_bits_in_memory(TRISC_ADDR, 1 << 5)

    def enter_pycom_programming_mode(self, reset=True):
        print_debug("Entering programming mode with reset={}".format(reset))
        # make RA5 an output
        self.mask_bits_in_memory(TRISA_ADDR, ~(1 << 5))
        # drive RA5 low
        self.mask_bits_in_memory(PORTA_ADDR, ~(1 << 5))
        # make RC0 an output
        self.mask_bits_in_memory(TRISC_ADDR, ~(1 << 0))
        # set RC0 low
        self.mask_bits_in_memory(PORTC_ADDR, ~(1 << 0))
        # perform reset
        if reset:
            self.reset_pycom_module()
        # We should keep RC0 low at this point in case someone
        # presses the reset button before the firmware upgrade
        # as this is mandatory for the regular expansion board
        self._flush()

    def exit_pycom_programming_mode(self, reset=True):
        print_debug("Leaving programming mode with reset={}".format(reset))
        # make RA5 an output
        self.mask_bits_in_memory(TRISA_ADDR, ~(1 << 5))
        # drive RA5 high
        self.set_bits_in_memory(PORTA_ADDR, 1 << 5)
        # make RC0 an input
        # This will prevent issues with the RGB LED
        self.set_bits_in_memory(TRISC_ADDR, 1 << 0)
        if reset:
            self.reset_pycom_module()
        self._flush()

    def isdetected(self):
        return self.detected

    def close(self):
        self.serial.close()


def main(args):
    parser = argparse.ArgumentParser(description='Sends internal commands to put the Pycom module in programming mode')
    parser.add_argument('-p', '--port', metavar='PORT', help='the serial port used to communicate with the PIC')
    parser.add_argument('--enter', action='store_true', help='enter programming mode')
    parser.add_argument('--exit', action='store_true', help='exit programming mode')
    parser.add_argument('--noreset', action='store_true', help='do not reset esp32')
    args = parser.parse_args()

    if not args.port:
        exit_with_error(1, 'no serial port specified')

    if (args.enter and args.exit) or (not args.enter and not args.exit):
        exit_with_error(1, 'invalid action requested')

    pic = Pypic(args.port)

    if pic.isdetected():
        if args.enter:
            pic.enter_pycom_programming_mode(not args.noreset)
        elif args.exit:
            pic.exit_pycom_programming_mode(not args.noreset)

        # print debug info about current PIC product
        print_debug("read_product_id(): 0x%X" % pic.read_product_id())
        print_debug("read_hw_version(): 0x%X" % pic.read_hw_version())
        print_debug("read_fw_version(): 0x%X" % pic.read_fw_version())

    pic.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
