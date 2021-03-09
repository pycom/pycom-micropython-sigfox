#!/usr/bin/env python

# This file is derived from the MicroPython project, http://micropython.org/
#
# Copyright (c) 2021, Pycom Limited and its licensors.
#
# This software is licensed under the GNU GPL version 3 or any later version,
# with permitted additional terms. For more information see the Pycom Licence
# v1.0 document supplied with this file, or available at:
# https://www.pycom.io/opensource/licensing


# This file is part of the MicroPython project, http://micropython.org/
#
# The MIT License (MIT)
#
# Copyright (c) 2014 Damien P. George
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


"""Generates the pins file for the ESP32."""

from __future__ import print_function

import argparse
import sys
import csv


#SUPPORTED_AFS = { 'UART': ('TX', 'RX', 'RTS', 'CTS'),
#                  'SPI': ('CLK', 'MOSI', 'MISO', 'CS0'),
#                  #'I2S': ('CLK', 'FS', 'DAT0', 'DAT1'),
#                  'I2C': ('SDA', 'SCL'),
#                  'TIM': ('PWM'),
#                  'SD': ('CLK', 'CMD', 'DAT0'),
#                  'ADC': ('CH0', 'CH1', 'CH2', 'CH3')
#                }

def parse_port_pin(name_str):
    """Parses the pin name string and returns the pin number"""
    if len(name_str) < 4:
        raise ValueError("Expecting pin name to be at least 4 characters")
    if name_str[:3] != 'GPI':
        raise ValueError("Expecting pin name to start with GPI")
    if name_str[3:].isdigit():
        return int(name_str[3:])
    elif name_str[4:].isdigit():
        return int(name_str[4:])
    raise ValueError("Expecting a numeric GPI(O) number")


#class AF:
#    """Holds the description of an alternate function"""
#    def __init__(self, name, idx, fn, unit, type):
#        self.name = name
#        self.idx = idx
#        if self.idx > 15:
#            self.idx = -1
#        self.fn = fn
#        self.unit = unit
#        self.type = type
#
#    def print(self):
#        print ('    AF({:16s}, {:4d}, {:8s}, {:4d}, {:8s}),    // {}'.format(self.name, self.idx, self.fn, self.unit, self.type, self.name))


class Pin:
    """Holds the information associated with a pin."""
    def __init__(self, name, pin_num):
        self.cpu_pin_name = name
        self.module_pin_name = ''
        self.pin_num = pin_num
        self.board_pin = False

    def print(self):
        print('// {}'.format(self.module_pin_name))
        print('pin_obj_t pin_{:4s} = PIN({:4s}, {:2d});\n'.format(
              self.cpu_pin_name, self.module_pin_name, self.pin_num))

    def set_module_pin_name(self, name):
        self.module_pin_name = name

    def print_header(self, hdr_file):
        hdr_file.write('extern pin_obj_t pin_{:s};\n'.format(self.cpu_pin_name))
        if self.module_pin_name:
            hdr_file.write('#define PIN_MODULE_{:3s}         pin_{:s}\n'.format(self.module_pin_name, self.cpu_pin_name))


class NamedPin(object):
    def __init__(self, name, pin):
        self._name = name
        self._pin = pin

    def pin(self):
        return self._pin

    def name(self):
        return self._name


class Pins:
    def __init__(self):
        self.cpu_pins = []      # list of cpu named pins
        self.board_pins = []    # list of expansion board named pins
        self.module_pins = []   # list of module named pins

    def find_pin_by_name(self, name):
        for named_pin in self.cpu_pins:
            pin = named_pin.pin()
            if pin.cpu_pin_name == name:
                return pin

    def parse_af_file(self, filename, pin_col, pinname_col, af_start_col):
        with open(filename, 'r') as csvfile:
            rows = csv.reader(csvfile)
            for row in rows:
                try:
                    pin_num = parse_port_pin(row[pinname_col])
                except:
                    continue
                if not row[pin_col].isdigit():
                    raise ValueError("Invalid pin number {:s} in row {:s}".format(row[pin_col]), row)
                pin = Pin(row[pinname_col], pin_num)
                # FIXME: hack to force the SX1272 pins to be available
                if row[pinname_col] == 'GPIO17' or row[pinname_col] == 'GPIO18' or row[pinname_col] == 'GPIO23':
                    pin.board_pin = True
                self.cpu_pins.append(NamedPin(row[pinname_col], pin))
#                af_idx = 0
#                for af in row[af_start_col:]:
#                    af_splitted = af.split('_')
#                    fn_name = af_splitted[0].rstrip('0123456789')
#                    if  fn_name in SUPPORTED_AFS:
#                        type_name = af_splitted[1]
#                        if type_name in SUPPORTED_AFS[fn_name]:
#                            unit_idx = af_splitted[0][-1]
#                            pin.add_af(AF(af, af_idx, fn_name, int(unit_idx), type_name))
#                    af_idx += 1

    def parse_board_file(self, filename, pin_name_col):
        with open(filename, 'r') as csvfile:
            rows = csv.reader(csvfile)
            for row in rows:
                pin = self.find_pin_by_name(row[pin_name_col])
                if pin:
                    pin.board_pin = True
                    self.board_pins.append(NamedPin(row[0], pin))
                    self.module_pins.append(NamedPin(row[2], pin))
                    pin.set_module_pin_name(row[2])

    def print_named(self, label, named_pins):
        print('')
        print('STATIC DRAM_ATTR mp_map_elem_t pin_{:s}_pins_locals_dict_table[] = {{'.format(label))
        for named_pin in named_pins:
            pin = named_pin.pin()
            if pin.board_pin:
                print('    {{ MP_OBJ_NEW_QSTR(MP_QSTR_{:6s}), (mp_obj_t)&pin_{:6s} }},'.format(named_pin.name(), pin.cpu_pin_name))
        print('};')
        print('MP_DEFINE_RAM_DICT(pin_{:s}_pins_locals_dict, pin_{:s}_pins_locals_dict_table);'.format(label, label));

    def print_named_no_qstr(self, label, named_pins):
        print('')
        print('STATIC DRAM_ATTR mp_map_elem_t pin_{:s}_pins_locals_dict_table[] = {{'.format(label))
        for named_pin in named_pins:
            pin = named_pin.pin()
            if pin.board_pin:
                print('    {{ MP_OBJ_NEW_QSTR(MP_QSTR_), (mp_obj_t)&pin_{:6s} }},'.format(pin.cpu_pin_name))
        print('};')
        print('MP_DEFINE_RAM_DICT(pin_{:s}_pins_locals_dict, pin_{:s}_pins_locals_dict_table);'.format(label, label));

    def print(self):
        for named_pin in self.cpu_pins:
            pin = named_pin.pin()
            if pin.board_pin:
                pin.print()
        self.print_named_no_qstr('cpu', self.cpu_pins)
        self.print_named('module', self.module_pins)
        self.print_named('exp_board', self.board_pins)
        print('')

    def print_header(self, hdr_filename):
        with open(hdr_filename, 'wt') as hdr_file:
            for named_pin in self.cpu_pins:
                pin = named_pin.pin()
                if pin.board_pin:
                    pin.print_header(hdr_file)

    def print_qstr(self, qstr_filename):
        with open(qstr_filename, 'wt') as qstr_file:
            pin_qstr_set = set([])
            af_qstr_set = set([])
            for named_pin in self.module_pins:
                pin_qstr_set |= set([named_pin.name()])
            for named_pin in self.board_pins:
                pin_qstr_set |= set([named_pin.name()])
            print('// Board pins', file=qstr_file)
            for qstr in sorted(pin_qstr_set):
                print('Q({})'.format(qstr), file=qstr_file)
#            print('\n// Pin AFs', file=qstr_file)
#            for qstr in sorted(af_qstr_set):
#                print('Q({})'.format(qstr), file=qstr_file)


def main():
    parser = argparse.ArgumentParser(
        prog="make-pins.py",
        usage="%(prog)s [options] [command]",
        description="Generate board specific pin file"
    )
    parser.add_argument(
        "-a", "--af",
        dest="af_filename",
        help="Specifies the alternate function file for the chip",
        default="esp32_af.csv"
    )
    parser.add_argument(
        "-b", "--board",
        dest="board_filename",
        help="Specifies the board file",
    )
    parser.add_argument(
        "-p", "--prefix",
        dest="prefix_filename",
        help="Specifies beginning portion of generated pins file",
        default="esp32_prefix.c"
    )
    parser.add_argument(
        "-q", "--qstr",
        dest="qstr_filename",
        help="Specifies name of generated qstr header file",
        default="build/pins_qstr.h"
    )
    parser.add_argument(
        "-r", "--hdr",
        dest="hdr_filename",
        help="Specifies name of generated pin header file",
        default="build/pins.h"
    )
    args = parser.parse_args(sys.argv[1:])

    pins = Pins()

    print('// This file was automatically generated by make-pins.py')
    print('//')
    if args.af_filename:
        print('// --af {:s}'.format(args.af_filename))
        pins.parse_af_file(args.af_filename, 0, 1, 2)

    if args.board_filename:
        print('// --board {:s}'.format(args.board_filename))
        pins.parse_board_file(args.board_filename, 1)

    if args.prefix_filename:
        print('// --prefix {:s}'.format(args.prefix_filename))
        print('')
        with open(args.prefix_filename, 'r') as prefix_file:
            print(prefix_file.read())
    pins.print()
    pins.print_qstr(args.qstr_filename)
    pins.print_header(args.hdr_filename)


if __name__ == "__main__":
    main()
