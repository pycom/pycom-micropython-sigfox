#!/usr/bin/env python

#   Copyright (c) 2016-2020, Pycom Limited.
#
#   This software is licensed under the GNU GPL version 3 or any
#   later version, with permitted additional terms. For more information
#   see the Pycom Licence v1.0 document supplied with this file, or
#   available at https://www.pycom.io/opensource/licensing

from __future__ import print_function

import argparse
import base64
import binascii
from io import BytesIO, StringIO
import json
import os
import re
import struct
import sys
import tarfile
import time
from traceback import print_exc

import serial.tools.list_ports

from esptool import ESP32ROM
import esptool
from pypic import Pypic

try:
    import humanfriendly
    humanfriendly_available = True
except:
    humanfriendly_available = False

DEBUG = False

FLASH_MODE = 'dio'
FLASH_FREQ = '80m'

DEFAULT_BAUD_RATE = 115200
FAST_BAUD_RATE = 921600
MAXPICREAD_BAUD_RATE = 230400

LORA_REGIONS = ["EU868", "US915", "AS923", "AU915", "IN865"]

PIC_BOARDS = ["04D8:F013", "04D8:F012", "04D8:EF98", "04D8:EF38", "04D8:ED14"]

PARTITIONS = { 'secureboot' : ["0x0", "0x8000"],
                'bootloader' : ["0x1000", "0x7000"],
                'partitions' : ["0x8000", "0x1000"],
                'nvs'       : ["0x9000", "0x7000"],
                'factory'   : ["0x10000", "0x180000"],
                'otadata'   : ["0x190000", "0x1000"],
                'ota_0'      : ["0x1a0000", "0x180000"],
                'fs'         : ["0x380000", "0x7F000"],
                'config'     : ["0x3FF000", "0x1000"],
                'fs1'        : ["0x400000", "0x400000"],
                'all'        : ["0x0"     , "0x800000"]}

PARTITIONS_NEW = {
                "factory"   : ["0x10000", "0x1AE000"],
                'otadata'   : ["0x1BE000", "0x1000"],
                "ota_0"     : ["0x1a0000", "0x1AE000"]
                }

# HERE_PATH = os.path.dirname(os.path.realpath(__file__))

# Beware not all regions are support in firmware yet!
# AS band on 923MHz
LORAMAC_REGION_AS923 = 0
# Australian band on 915MHz
LORAMAC_REGION_AU915 = 1
# Chinese band on 470MHz
LORAMAC_REGION_CN470 = 2
# Chinese band on 779MHz
LORAMAC_REGION_CN779 = 3
# European band on 433MHz
LORAMAC_REGION_EU433 = 4
# European band on 868MHz
LORAMAC_REGION_EU868 = 5
# South korean band on 920MHz
LORAMAC_REGION_KR920 = 6
# India band on 865MHz
LORAMAC_REGION_IN865 = 7
# North american band on 915MHz
LORAMAC_REGION_US915 = 8
# North american band on 915MHz with a maximum of 16 channels
LORAMAC_REGION_US915_HYBRID = 9


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)
    sys.stderr.flush()


def print_exception(e):
    if DEBUG:
        try:
            if sys.version_info[0] < 3:
                print_exc(e)
            else:
                print_exc()
        except Exception as ex:
            print_debug('Exception: {}'.format(e))
            print_debug('Exception exception: {}'.format(ex))


def print_debug(msg, show=DEBUG):
    if show:
        eprint(msg)


size_table = [
    (1024 ** 3, ' gb'),
    (1024 ** 2, ' mb'),
    (1024 ** 1, ' kb'),
    (1024 ** 0, ' bytes'),
    ]


def hr_size(size):

    if humanfriendly_available:
        return humanfriendly.format_size(size, binary=True)
    else:
        for factor, suffix in size_table:
            if size >= factor:
                break
        try:
            amount = int(size / factor)
        except Exception as e:
            print_exception(e)
            return str(amount) + " bytes"
        if isinstance(suffix, tuple):
            singular, multiple = suffix
            if amount == 1:
                suffix = singular
            else:
                suffix = multiple
        return str(amount) + suffix


class Args(object):
    pass


def load_tar(fileobj, prog, secure=False):
    script = None
    legacy = False
    try:
        tar = tarfile.open(mode="r", fileobj=fileobj)
    except Exception as e:
        print_exception(e)
        return e
    try:
        fsize = prog.int_flash_size()
        if fsize == 0x800000:
            try:
                if secure:
                    script_file = json.loads(tar.extractfile("script_8MB_enc").read().decode('UTF-8'))
                else:
                    script_file = json.loads(tar.extractfile("script_8MB").read().decode('UTF-8'))
            except:
                print_debug("Error Loading script_8MB ... defaulting to legacy script2!", True)
                legacy = True
        elif fsize == 0x400000:
            try:
                if secure:
                    script_file = json.load(tar.extractfile("script_4MB_enc").read().decode('UTF-8'))
                else:
                    script_file = json.load(tar.extractfile("script_4MB").read().decode('UTF-8'))
            except Exception as e:
                print_exception(e)
                print_debug("Error Loading script_4MB ... defaulting to legacy script2!", True)
                legacy = True
        else:
            return RuntimeError("Cannot detect flash size! .. Aborting")

        if legacy:
            script_file = json.loads(tar.extractfile("script2").read().decode('UTF-8'))

        version = script_file.get('version')
        if version is not None:
            print_debug('Script Version: {}'.format(version), True)
            partitions = script_file.get('partitions')
            if partitions is not None:
                PARTITIONS.update(partitions)
        else:
            raise ValueError('version not found in script2')

        try:
            script = script_file.get('script')
        except Exception as e:
            print_exception(e)

        if script is None:
            raise ValueError('script not found in script2')
    except RuntimeError as e:
        print_exception(e)
        return e
    except:
        try:
            script = json.loads(tar.extractfile("script").read().decode('UTF-8'))
        except Exception as e:
            script = e
            print_exception(e)
            raise ValueError("Your board is not supported by this firmware package")
    try:
        for i in range(len(script)):
            if script[i][0] == 'w' or script[i][0] == 'o':
                script[i][0] = script[i][0] + ":" + script[i][2]
                script[i][2] = tar.extractfile(script[i][2]).read()
    except Exception as e:
        script = e
        print_exception(e)

    tar.close()
    return script


class NPyProgrammer(object):

    def __init__(self, port, baudrate, continuation=False, pypic=False, debug=False, reset=False, resultUpdateList=None, connect_read=False):
        self.__debug = debug
        self.__current_baudrate = self.__baudrate = baudrate
        self.__pypic = pypic
        self.__resultUpdateList = resultUpdateList
        self.__flash_size = 'detect'
        self.__progress_fs = None

        if re.match('^com[0-9].*', port, re.I):
            self.esp_port = port.lower()
        else:
            self.esp_port = port

        print_debug("Connecting to ESP32 with baudrate: %d" % self.__baudrate, self.__debug)

        if continuation == False:
            if (self.__pypic):
                self.enter_pycom_programming_mode()
            self.esp = ESP32ROM(self.esp_port, DEFAULT_BAUD_RATE)
            if not self.__pypic:
                self.esp.connect()
            else:
                self.esp.connect(mode='no_reset')

            self.esp = self.esp.run_stub()
            if connect_read and pypic:
                self.__current_baudrate = self.get_baudrate(True)
                self.esp.change_baud(self.__current_baudrate, self.__resultUpdateList)
            elif self.__current_baudrate != 115200:
                self.esp.change_baud(self.__baudrate, self.__resultUpdateList)
        else:
            esp = ESP32ROM(self.esp_port, self.__baudrate)
            self.esp = esp.STUB_CLASS(esp)  # enable the stub functions

    def is_pypic(self):
        return self.__pypic

    def get_resultUpdateList(self):
        return self.__resultUpdateList

    def get_baudrate(self, read=False):
        if self.__pypic and read and self.__baudrate > MAXPICREAD_BAUD_RATE:
            return MAXPICREAD_BAUD_RATE
        else:
            return self.__baudrate

    def set_baudrate(self, read=False):
        if self.__current_baudrate != self.get_baudrate(read):
            try:
                self.__current_baudrate = self.get_baudrate(read)
                self.esp.change_baud(self.__current_baudrate, self.__resultUpdateList)
            except Exception as e:
                print_exception(e)

    def read(self, offset, size):
        self.set_baudrate(True)
        ret_val = self.esp.read_flash(offset, size, resultUpdateList=self.__resultUpdateList, partition=self.partition_name(offset))
        self.set_baudrate(False)
        return ret_val

    def check_partition(self, partition):
        if PARTITIONS.get(partition) is None:
            return False
        else:
            return True

    def partition_name(self, offset):
        if offset > 0:
            for partition in PARTITIONS.keys():
                if int(PARTITIONS.get(partition)[0], 16) == offset:
                    return partition
        return str(offset)

    def int_flash_size(self):
        args = Args()
        args.flash_size = self.__flash_size
        args.flash_mode = FLASH_MODE
        args.flash_freq = FLASH_FREQ
        args.compress = True
        args.verify = False
        args.no_stub = False

        if args.flash_size == 'detect':
            esptool.detect_flash_size(self.esp, args)
            self.__flash_size = args.flash_size
        str_flash_size = args.flash_size
        try:
            int_flash_size = int(str_flash_size.replace('MB', '')) * 0x100000
        except:
            int_flash_size = (4 * 0x100000) if args.flash_size != '8MB' else (8 * 0x100000)
        return int_flash_size

    def erase(self, offset, section_size, ui_label=None, updateList=False):
        msg = "Erasing %s at address 0x%08X" % (hr_size(section_size), int(offset))
        if ui_label is None:
            print(msg)
        else:
            ui_label.setText(msg)
        if updateList and self.__resultUpdateList is not None:
            self.__resultUpdateList.append("Erased %s at address 0x%08X" % (hr_size(section_size), offset))

        MAX_SECTION_SIZE = 0x380000
        offset = int((offset // 4096) * 4096)
        iterations = int((section_size + MAX_SECTION_SIZE - 1) // MAX_SECTION_SIZE)
        for x in range(iterations):
            s = min(section_size, MAX_SECTION_SIZE)
            s = int(((s + 4095) // 4096) * 4096)
            # We'll give it 3 attempts, if it still fails we lost the connection
            try:
                self.esp.erase_region(offset, s, progress_fs=self.__progress_fs)
            except Exception as e:
                try:
                    self.esp.erase_region(offset, s, progress_fs=self.__progress_fs)
                except:
                    try:
                        self.esp.erase_region(offset, s, progress_fs=self.__progress_fs)
                    except:
                        print_exception(e)
                        raise e
            offset += s

    def erase_all(self, ui_label=None):
        args = Args()
        args.flash_size = self.__flash_size
        args.flash_mode = FLASH_MODE
        args.flash_freq = FLASH_FREQ
        args.compress = True
        args.verify = False
        args.no_stub = False
        if args.flash_size == 'detect':
            esptool.detect_flash_size(self.esp, args)
            self.__flash_size = args.flash_size
        self.set_baudrate(False)
        if ui_label is not None:
            ui_label.setText("Erasing entire flash... this may take a while")
        if self.__resultUpdateList is not None:
            self.__resultUpdateList.append("Erasing entire flash")
        ret_val = self.esp.erase_flash()
        return ret_val

    def write(self, offset, contents, compress=True, flash_size='detect', std_out=None, ui_label=None, file_name=None, updateList=True, progress_fs=None):
        if progress_fs is not None:
            self.__progress_fs = progress_fs
        args = Args()
        args.flash_size = self.__flash_size if flash_size == 'detect' else flash_size
        args.flash_mode = FLASH_MODE
        args.flash_freq = FLASH_FREQ
        args.compress = compress
        args.verify = False
        args.no_stub = False
        if args.flash_size == 'detect':
            esptool.detect_flash_size(self.esp, args)
            self.__flash_size = args.flash_size
        self.set_baudrate(False)

        fmap = BytesIO(contents)
        args.addr_filename = [[offset, fmap]]
        if std_out is not None:
            sys.stdout = std_out
        first_exception = None
        for x in range(0, 3):
            try:
                esptool.write_flash(self.esp, args, ui_label=ui_label, file_name=file_name, resultUpdateList=self.__resultUpdateList if updateList else None, partition=self.partition_name(offset), progress_fs=self.__progress_fs)
                fmap.close()
                return
            except AttributeError as ae:
                print_exc(ae)
                raise RuntimeError('Content at offset 0x%x does not fit available flash size of %s' % (offset, args.flash_size))
            except Exception as e:
                print_debug("Exception in write: {}".format(e), self.__debug)
                if x == 0:
                    first_exception = e
        fmap.close()
        if first_exception is not None:
            raise first_exception

    def detect_flash_size(self):
        args = Args()
        args.flash_size = 'detect'
        args.flash_mode = FLASH_MODE
        args.flash_freq = FLASH_FREQ
        args.compress = True
        args.verify = False
        args.no_stub = False
        esptool.detect_flash_size(self.esp, args)
        return args.flash_size

    def write_script(self, offset, contents, config_block, overwrite=False, size=None, ui_label=None, file_name=None):
        if overwrite or size is None:
            self.write(offset, contents, ui_label=ui_label, file_name=file_name)
        else:
            if (len(contents) > size):
                print_debug("Truncating this instruction by %s as it finishes outside specified partition size!" % hr_size(len(contents) - size), self.__debug)
                contents = contents[:size]

            args = Args()
            args.flash_size = self.__flash_size
            args.flash_mode = FLASH_MODE
            args.flash_freq = FLASH_FREQ
            args.compress = True
            args.verify = False
            args.no_stub = False

            if args.flash_size == 'detect':
                esptool.detect_flash_size(self.esp, args)
                self.__flash_size = args.flash_size

            finish_addr = offset + len(contents)
            str_flash_size = args.flash_size
            try:
                int_flash_size = int(str_flash_size.replace('MB', '')) * 0x100000
            except:
                int_flash_size = (4 * 0x100000) if args.flash_size != '8MB' else (8 * 0x100000)

            cb_start = int(PARTITIONS.get('config')[0], 16)
            cb_len = int(PARTITIONS.get('config')[1], 16)
            cb_end = cb_start + cb_len

            if finish_addr > int_flash_size:
                print_debug("Truncating this instruction by %s as it finishes outside available flash memory!" % ((hr_size(finish_addr - int_flash_size))), self.__debug)
                contents = contents[:int_flash_size - offset]
                finish_addr = offset + len(contents)
            if offset >= int_flash_size:
                print_debug("Ignoring this instruction as it starts outside available flash memory!", self.__debug)
            elif offset < cb_end and finish_addr > cb_start:
                if offset >= cb_start and finish_addr <= cb_end:
                    print_debug("Offset[0x%X] until finish_addr[0x%X] would only write within the CB! Skipping..." % (offset, finish_addr), self.__debug)
                else:
                    print_debug("Offset[0x%X] + Content[%s] would overwrite CB! It ends at: 0x%X" % (offset, hr_size(len(contents)), finish_addr), self.__debug)
                    if config_block is None:
                        print_debug("I need to read the config block because I didn't receive it as a parameter", self.__debug)
                        config_block = self.read(cb_start, cb_len)
                    if offset < cb_start and finish_addr > cb_end:
                        self.write(offset, contents[0:cb_start - offset] + config_block + contents[cb_start - offset:], flash_size=args.flash_size, ui_label=ui_label)
                    elif offset <= cb_start and finish_addr <= cb_end:
                        self.write(offset, contents[0:cb_start - offset] + config_block, flash_size=args.flash_size, ui_label=ui_label)
                    elif offset > cb_start and finish_addr > cb_end:
                        self.write(offset, config_block + contents[cb_start - offset:], flash_size=args.flash_size, ui_label=ui_label)
                    else:
                        raise RuntimeError("I am unable to protect the config block from being overwritten.")
            else:
                print_debug("Offset[0x%X] + Content[%s] finish at: 0x%X" % (offset, hr_size(len(contents)), finish_addr), self.__debug)
                self.write(offset, contents.ljust(size, b'\xFF'), flash_size=args.flash_size, ui_label=ui_label, file_name=file_name)

    def write_remote(self, contents):
        cb_start = int(PARTITIONS.get('config')[0], 16)
        cb_len = int(PARTITIONS.get('config')[1], 16)
        config_block = self.read(cb_start, cb_len)
        self.write(cb_start, contents[0:52] + config_block[52:])

    def run_script(self, script, config_block=None, erase_fs=False, chip_id=None, ui_label=None, progress_fs=None, erase_nvs=False):
        self.__progress_fs = progress_fs
        if script is None:
            raise ValueError('Invalid or no script file in firmware package!')
        if DEBUG:
            for instruction in script:
                print_debug('Instruction: {} {}'.format(instruction[0], instruction[1]))
        ota_updated = False
        ota = None
        img_size = 0xffffffff
        no_erase = False
        for instruction in script:
            if instruction[1] == 'fs' or instruction[1] == 'fs1':
                erase_fs = (instruction[0] == 'e')
            elif instruction[1] == 'nvs':
                erase_nvs = (instruction[0] == 'e')
            elif instruction[1] == 'all':
                erase_fs = False
                erase_nvs = False
                no_erase = not (instruction[0] == 'e')
        start_time = time.time()
        total_size = 0
        for instruction in script:
            if instruction[0] == 'e':
                if no_erase:
                    print_debug("Ignoring erase instruction %s as entire flash partition is being written." % instruction[1], self.__debug)
                    continue
                if instruction[1] == 'fs' or instruction[1] == 'fs1':
                    continue
                if instruction[1] == 'all':
                    self.erase_all(ui_label=ui_label)
                    ota_updated = True
                    continue
                if check_partition(instruction[1]):
                    instruction1 = int(PARTITIONS.get(instruction[1])[0], 16)
                    instruction2 = int(PARTITIONS.get(instruction[1])[1], 16)
                elif instruction[1] == 'nvs':
                    instruction1 = int(PARTITIONS.get('nvs')[0], 16)
                    instruction2 = int(PARTITIONS.get('nvs')[1], 16)
                elif instruction[1] == 'cb' or instruction[1] == 'config':
                    instruction1 = int(PARTITIONS.get('config')[0], 16)
                    instruction2 = int(PARTITIONS.get('config')[1], 16)
                elif instruction[1] == 'ota' or instruction[1] == 'otadata':
                    instruction1 = int(PARTITIONS.get('otadata')[0], 16)
                    instruction2 = int(PARTITIONS.get('otadata')[1], 16)
                    ota_updated = True
                else:
                    instruction1 = int(instruction[1], 16)
                    instruction2 = int(instruction[2], 16)
                if instruction1 <= int(PARTITIONS.get('otadata')[0], 16) and instruction1 + instruction2 >= int(PARTITIONS.get('otadata')[0], 16) + int(PARTITIONS.get('otadata')[1], 16):
                    ota_updated = True
                if ota_updated:
                    print_debug("OTA partition has been erased.", self.__debug)
                total_size += instruction2
                self.erase(instruction1, instruction2, ui_label=ui_label)

        if total_size > 0 and self.__resultUpdateList is not None:
            if humanfriendly_available:
                self.__resultUpdateList.append('Erased {} in {}'.format(humanfriendly.format_size(total_size, binary=True), humanfriendly.format_timespan(time.time() - start_time)))
            else:
                self.__resultUpdateList.append('Erased {} in {0:.2f} seconds'.format(hr_size(total_size), time.time() - start_time))

        if erase_fs:
            self.erase_fs(chip_id, ui_label=ui_label)
        if erase_nvs:
            self.erase(int(PARTITIONS.get('nvs')[0], 16), int(PARTITIONS.get('nvs')[1], 16), ui_label=ui_label, updateList = True)

        for instruction in script:
            if instruction[0].split(':', 2)[0] == 'w' or instruction[0].split(':', 2)[0] == 'o':
                file_name = instruction[0].split(':', 2)[-1]
                print_debug(("Writing %s " + ("to partition %s" if check_partition(instruction[1]) else "at offset %s")) % (file_name, instruction[1]), self.__debug)
                psize = None
                instruction1 = instruction[1]
                if instruction[1] == 'fs' or instruction[1] == 'fs1':
                    erase_fs = False
                if instruction[1] == 'otadata':
                    ota_updated = True
                if check_partition(instruction[1]):
                    if instruction[1] == "ota_0":
                        ota = True
                        img_size = len(instruction[2])
                    elif instruction[1] == "factory":
                        if ota is None:
                            ota = False
                        img_size = len(instruction[2])
                    psize = int(PARTITIONS.get(instruction[1])[1], 16)
                    if instruction[1] == "all":
                        psize = self.int_flash_size()
                    instruction1 = PARTITIONS.get(instruction[1])[0]
                elif instruction[1] == 'cb':
                    instruction1 = PARTITIONS.get('config')[0]
                    psize = int(PARTITIONS.get('config')[1], 16)
                elif instruction[1] == 'ota':
                    ota_updated = True
                    instruction1 = PARTITIONS.get('otadata')[0]
                    psize = int(PARTITIONS.get('otadata')[1], 16)
                try:
                    int_instr1 = int(instruction1, 16)
                except ValueError:
                    raise ValueError('Invalid partition or memory region %s' % str(instruction1))
                self.write_script(int_instr1, instruction[2], config_block, instruction[0].split(':', 2)[0] == 'o', size=psize, ui_label=ui_label, file_name=file_name)
                if (int_instr1 <= int(PARTITIONS.get('otadata')[0], 16)) and (int_instr1 + (len(instruction[2]) if psize is None else psize) >= int(PARTITIONS.get('otadata')[0], 16) + int(PARTITIONS.get('otadata')[1], 16)):
                    ota_updated = True
                    print_debug("OTA partition has been written.", self.__debug)
            elif instruction[0] != 'e':
                raise ValueError('Invalid script command %s' % instruction[0].split(':', 2)[0])
        if ota is not None and not ota_updated:
            self.set_ota(ota, img_size, ui_label=ui_label)

    def set_ota(self, ota, image_size, ui_label=None):
        ota_signature = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
        crc32_header = b'\xff\xff\xff\xff'
        if ota:
            ota_data = struct.pack('<5I16s', 1, 0, 0, image_size, 0, ota_signature)
            ota_crc32 = (binascii.crc32(crc32_header + ota_data) % (1 << 32))
            ota_part = struct.pack('<36sI', ota_data, ota_crc32)
            if ui_label is not None:
                ui_label.setText("Setting otadata partition to boot from ota_0")
            if self.__resultUpdateList is not None:
                self.__resultUpdateList.append("Booting from partition: <b>ota_0</b>")
            print_debug("Setting otadata partition to boot from ota_0", self.__debug)
            self.write(int(PARTITIONS.get('otadata')[0], 16), ota_part.ljust(int(PARTITIONS.get('otadata')[1], 16), b'\xff'))
        else:
            ota_data = struct.pack('<5I16s', 0, 1, 0, image_size, 0, ota_signature)
            ota_crc32 = (binascii.crc32(crc32_header + ota_data) % (1 << 32))
            ota_part = struct.pack('<36sI', ota_data, ota_crc32)
            if ui_label is not None:
                ui_label.setText("Setting otadata partition to boot from factory partition")
            if self.__resultUpdateList is not None:
                self.__resultUpdateList.append("Booting from partition: <b>factory</b>")
            print_debug("Setting otadata partition to boot from factory", self.__debug)
            self.write(int(PARTITIONS.get('otadata')[0], 16), ota_part.ljust(int(PARTITIONS.get('otadata')[1], 16), b'\xff'))

    def erase_sytem_mem(self):
        # Erase first 3.5Mb (this way fs and MAC address will be untouched)
        self.erase(0, 0x3100000)

    def erase_fs(self, chip_id=None, ui_label=None, progress_fs=None):
        self.__progress_fs = progress_fs
        start_time = time.time()
        print_debug('self.int_flash_size() = {}'.format(self.int_flash_size()))
        print_debug('chip_id = {}'.format(chip_id))
        if self.int_flash_size() == 0x800000:
            print_debug("Erasing 8MB device flash fs", self.__debug)
            # self.erase(int(PARTITIONS.get('fs')[0], 16), int(PARTITIONS.get('fs')[1], 16), ui_label=ui_label, updateList=False)
            section_size = int(PARTITIONS.get('fs1')[1], 16) / 8
            for x in range(0, 8):
                self.erase(int(PARTITIONS.get('fs1')[0], 16) + (x * section_size), section_size, ui_label=ui_label, updateList=False)
            if self.__resultUpdateList is not None:
                if humanfriendly_available:
                    self.__resultUpdateList.append("Erased {} flash fs in {}".format(humanfriendly.format_size(int(PARTITIONS.get('fs1')[1], 16), binary=True), humanfriendly.format_timespan(time.time() - start_time)))
                else:
                    self.__resultUpdateList.append("Erased {} flash fs in {0:.2f} seconds".format(hr_size(int(PARTITIONS.get('fs1')[1], 16)), time.time() - start_time))
        else:
            print_debug("Erasing 4MB device flash fs", self.__debug)
            self.erase(int(PARTITIONS.get('fs')[0], 16), int(PARTITIONS.get('fs')[1], 16), ui_label=ui_label, updateList=False)
            if self.__resultUpdateList is not None:
                if humanfriendly_available:
                    self.__resultUpdateList.append("Erased {} flash fs in {}".format(humanfriendly.format_size(int(PARTITIONS.get('fs')[1], 16), binary=True), humanfriendly.format_timespan(time.time() - start_time)))
                else:
                    self.__resultUpdateList.append("Erased {} flash fs in {0:.2f} seconds".format(hr_size(int(PARTITIONS.get('fs')[1], 16)), time.time() - start_time))

    def get_chip_id(self):
        try:
            return self.esp.get_chip_description()
        except Exception as e:
            try:
                return self.esp.get_chip_description()
            except:
                print_exception(e)
                raise e

    def flash_bin(self, dest_and_file_pairs, ui_label=None, file_name=None, progress_fs=None):
        args = Args()

        args.flash_size = self.__flash_size
        args.flash_mode = FLASH_MODE
        args.flash_freq = FLASH_FREQ
        args.compress = True
        args.verify = True

        if args.flash_size == 'detect':
            esptool.detect_flash_size(self.esp, args)
            self.__flash_size = args.flash_size

        dest_and_file = list(dest_and_file_pairs)

        for i, el in enumerate(dest_and_file):
            dest_and_file[i][1] = open(el[1], "rb")

        args.addr_filename = dest_and_file

        esptool.write_flash(self.esp, args, ui_label=ui_label, file_name=file_name, resultUpdateList=self.__resultUpdateList, progress_fs=progress_fs)

    def set_wifi_config(self, config_block, wifi_ssid=None, wifi_pwd=None, wifi_on_boot=None):
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')
        if wifi_on_boot is not None:
            if wifi_on_boot == True:
                wob = b'\xbb'
            else:
                wob = b'\xba'
        else:
            if sys.version_info[0] < 3:
                wob = config_block[53]
            else:
                wob = config_block[53].to_bytes(1, byteorder='little')
        if wifi_ssid is not None:
            ssid = wifi_ssid.encode().ljust(33, b'\x00')
        else:
            ssid = config_block[54:87]

        if wifi_pwd is not None:
            pwd = wifi_pwd.encode().ljust(65, b'\x00')
        else:
            pwd = config_block[87:152]

        new_config_block = config_block[0:53] \
                           +wob \
                           +ssid \
                           +pwd  \
                           +config_block[152:]
        return self.set_pybytes_config(new_config_block, force_update=True)

    def set_lte_config(self, config_block, carrier=None, apn=None, lte_type=None, cid=None, band=None, reset=None):
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')

        if carrier is not None:
            cb_carrier = str(carrier[0:128]).ljust(129, b'\x00')
        else:
            cb_carrier = config_block[634:763]

        if apn is not None:
            cb_apn = str(apn[0:128]).ljust(129, b'\x00')
        else:
            cb_apn = config_block[763:892]

        if lte_type is not None:
            cb_lte_type = str(lte_type[0:16]).ljust(17, b'\x00')
        else:
            cb_lte_type = config_block[892:909]

        if cid is not None:
            cb_cid = struct.pack('>B', int(cid))
        else:
            cb_cid = config_block[909]

        if band is not None:
            cb_band = struct.pack('>B', int(band))
        else:
            cb_band = config_block[910]

        if reset is not None:
            cb_reset = struct.pack('>B', int(reset == 'True'))
        else:
            cb_reset = config_block[911]

        new_config_block = config_block[0:634] \
                           +cb_carrier \
                           +cb_apn \
                           +cb_lte_type  \
                           +cb_cid  \
                           +cb_band  \
                           +cb_reset  \
                           +config_block[912:]
        return self.set_pybytes_config(new_config_block, force_update=True)

    def set_pycom_config(self, config_block, boot_fs_type=None):
        print_debug('This is set_pycom_config with boot_fs_type={} [{}]'.format(boot_fs_type, type(boot_fs_type)))
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')
        if boot_fs_type is None:
            print_debug('Not doing anything because boot_fs_type is None')
            return config_block
        if DEBUG:
            self.print_cb(config_block)
        if boot_fs_type is not None:
            if str(boot_fs_type) == 'LittleFS' or boot_fs_type == 1 or str(boot_fs_type) == '1':
                fs = b'\x01'
            else:
                fs = b'\x00'

        new_config_block = config_block[0:533] \
                           +fs \
                           +config_block[534:]
        if DEBUG:
            self.print_cb(config_block)
        if self.__resultUpdateList is not None:
            self.__resultUpdateList.append('File system type set to <b>{}</b>'.format('LittleFS' if (boot_fs_type == 'LittleFS' or boot_fs_type == 1 or boot_fs_type == '1') else 'FatFS'))
        return self.set_pybytes_config(new_config_block, force_update=True)

    def print_cb(self, config_block):
        if DEBUG:
            for x in range(0, 30):
                print(binascii.hexlify(config_block[x * 32:x * 32 + 32]).decode('UTF-8'))

    def set_pybytes_config(self, config_block, userid=None, device_token=None, mqttServiceAddress=None, network_preferences=None, extra_preferences=None, force_update=None, auto_start=None):
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')
        if device_token is not None:
            token = str(device_token)[0:39].ljust(40, b'\x00')
        else:
            token = config_block[162:202]

        if mqttServiceAddress is not None:
            address = str(mqttServiceAddress)[0:39].ljust(40, b'\x00')
        else:
            address = config_block[202:242]

        if userid is not None:
            uid = str(userid)[0:99].ljust(100, b'\x00')
        else:
            uid = config_block[242:342]

        if network_preferences is not None:
            nwp = str(network_preferences)[0:54].ljust(55, b'\x00')
        else:
            nwp = config_block[342:397]

        if extra_preferences is not None:
            ep = str(extra_preferences)[0:99].ljust(100, b'\x00')
        else:
            ep = config_block[397:497]

        if force_update is not None:
            if force_update:
                fu = b'\x01'
            else:
                fu = b'\x00'
        else:
            if sys.version_info[0] < 3:
                fu = config_block[497]
            else:
                fu = config_block[497].to_bytes(1, byteorder='little')

        if auto_start is not None:
            if auto_start:
                asf = b'\x01'
            else:
                asf = b'\x00'
        else:
            if sys.version_info[0] < 3:
                asf = config_block[498]
            else:
                asf = config_block[498].to_bytes(1, byteorder='little')

        new_config_block = config_block[0:162] \
                           +token \
                           +address  \
                           +uid \
                           +nwp \
                           +ep \
                           +fu \
                           +asf \
                           +config_block[499:]

        # self.print_cb(new_config_block)
        return new_config_block

    def str2region(self, lora_region_str):
        if lora_region_str is not None:
            return {
                'EU868' : LORAMAC_REGION_EU868,
                'US915' : LORAMAC_REGION_US915,
                'AU915' : LORAMAC_REGION_AU915,
                'AS923' : LORAMAC_REGION_AS923
            }.get(lora_region_str, 0xff)
        else:
            return 0xff

    def region2str(self, lora_region_int):
        if lora_region_int is not None:
            return {
                LORAMAC_REGION_EU868  : 'EU868',
                LORAMAC_REGION_US915  : 'US915',
                LORAMAC_REGION_AU915  : 'AU915',
                LORAMAC_REGION_AS923  : 'AS923'
            }.get(lora_region_int, 'NONE')
        else:
            return None

    def set_lpwan_config(self, config_block, lora_region=None):
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')
        if lora_region is not None:
            if sys.version_info[0] < 3:
                region = chr(self.str2region(lora_region))
            else:
                region = self.str2region(lora_region).to_bytes(1, byteorder='little')
        else:
            if sys.version_info[0] < 3:
                region = config_block[52]
            else:
                region = config_block[52].to_bytes(1, byteorder='little')

        new_config_block = config_block[0:52] \
                           +region \
                           +config_block[53:]
        return new_config_block

    def set_sigfox_config(self, config_block, sid=None, pac=None, pubkey=None, privkey=None):
        config_block = config_block.ljust(int(PARTITIONS.get('config')[1], 16), b'\x00')
        if sid is not None:
            if not len(sid)==8:
                raise ValueError('ID must be 8 HEX characters')
            sigid = bytearray.fromhex(sid).ljust(4, b'\x00')
        else:
            sigid = config_block[8:12]

        if pac is not None:
            if not len(pac)==16:
                raise ValueError('PAC must be 16 HEX characters')
            spac = bytearray.fromhex(pac).ljust(8, b'\x00')
        else:
            spac = config_block[12:20]

        if privkey is not None:
            if not len(privkey)==32:
                raise ValueError('private key must be 32 HEX characters')
            sprivkey = bytearray.fromhex(privkey).ljust(16, b'\x00')
        else:
            sprivkey = config_block[20:36]

        if pubkey is not None:
            if not len(pubkey)==32:
                raise ValueError('public key must be 32 HEX characters')
            spubkey = bytearray.fromhex(pubkey).ljust(16, b'\x00')
        else:
            spubkey = config_block[36:52]

        new_config_block = config_block[0:8] \
                           +sigid \
                           +spac \
                           + sprivkey \
                           + spubkey \
                           +config_block[52:]
        return new_config_block


    def read_mac(self):
        # returns a tuple with (wifi_mac, bluetooth_mac)
        return self.esp.read_mac()

    def reset_pycom_module(self):
        pic = Pypic(self.esp_port)
        if pic.isdetected():
            pic.reset_pycom_module()
        pic.close()

    def exit_pycom_programming_mode(self, reset=True):
        if not self.__pypic:
            self.esp.hard_reset()
        time.sleep(.5)
        del self.esp
        if (self.__pypic):
            pic = Pypic(self.esp_port)
            if pic.isdetected():
                pic.exit_pycom_programming_mode(reset)
            pic.close()

    def enter_pycom_programming_mode(self):
        pic = Pypic(self.esp_port)
        if pic.isdetected():
            print("Product ID: %d HW Version: %d FW Version: 0.0.%d" % (pic.read_product_id(), pic.read_hw_version(), pic.read_fw_version()))
            pic.enter_pycom_programming_mode()
        pic.close()


def check_usbid(port):
    for n, (portname, desc, hwid) in enumerate(sorted(serial.tools.list_ports.comports(), reverse=True)):
        if (portname.replace("/dev/tty.", "/dev/cu.").upper() == str(port).replace("/dev/tty.", "/dev/cu.").upper()):
            for usbid in PIC_BOARDS:
                if usbid in hwid:
                    return True
            else:
                return False
    raise ValueError('Invalid serial port %s! Use list command to show valid ports.' % port)


def list_usbid():
    for n, (portname, desc, hwid)  in enumerate(sorted(serial.tools.list_ports.comports(), reverse=True)):
        print("%s  [%s] [%s]" % (portname, desc, hwid))


def check_partition(partition):
    if PARTITIONS.get(partition) is None:
        return False
    else:
        return True


def check_lora_region(region):
    if region in LORA_REGIONS:
        return True
    else:
        return False


def str2bool(v):
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected [yes, true, 1 or no, false, 0]')


def process_arguments():
    cmd_parser = argparse.ArgumentParser(description='Update your Pycom device with the specified firmware image file\n\nFor more details please see https://docs.pycom.io/chapter/advance/cli.html')
    cmd_parser.add_argument('-v', '--verbose', action='store_true', help='show verbose output from esptool')
    cmd_parser.add_argument('-d', '--debug', action='store_true', help='show debuggin output from fwtool')
    cmd_parser.add_argument('-q', '--quiet', action='store_true', help='suppress success messages')
    cmd_parser.add_argument('-p', '--port', default=None, help='the serial port to use')
    cmd_parser.add_argument('-s', '--speed', default=None, type=int, help='baudrate')
    cmd_parser.add_argument('-c', '--continuation', action='store_true', help='continue previous connection')
    cmd_parser.add_argument('-x', '--noexit', action='store_true', help='do not exit firmware update mode')
    cmd_parser.add_argument('--ftdi', action='store_true', help='force running in ftdi mode')
    cmd_parser.add_argument('--pic', action='store_true', help='force running in pic mode')
    cmd_parser.add_argument('-r', '--reset', action='store_true', help='use Espressif reset mode')

    subparsers = cmd_parser.add_subparsers(dest='command')

    subparsers.add_parser('list', help='Get list of available COM ports')

    subparsers.add_parser('chip_id', help='Show ESP32 chip_id')
    subparsers.add_parser('wmac', help='Show WiFi MAC')
    subparsers.add_parser('smac', help='Show LPWAN MAC')
    subparsers.add_parser('exit', help='Exit firmware update mode')

    if DEBUG:
        cmd_parser_bootpart = subparsers.add_parser('boot_part', add_help=False)  # , help='Check / Set active boot partition')
        cmd_parser_bootpart.add_argument('-p', '--partition', default=None, help="Set the activate boot partition [factory or ota_0]")

    cmd_parser_pycom = subparsers.add_parser('pycom', help='Check / Set pycom parameters')
    cmd_parser_pycom.add_argument('--fs_type', default=None, help="Set the file system type ['FatFS' or 'LittleFS']")
    cmd_parser_sigfox = subparsers.add_parser('sigfox', help='Show/Update sigfox details')
    cmd_parser_sigfox.add_argument('--id', default=None, help='Update Sigfox id')
    cmd_parser_sigfox.add_argument('--pac', default=None, help='Update Sigfox pac')
    cmd_parser_sigfox.add_argument('--pubkey', default=None, help='Update Sigfox public key')
    cmd_parser_sigfox.add_argument('--privkey', default=None, help='Update Sigfox private key')

    cmd_parser_flash = subparsers.add_parser('flash', help='Write firmware image to flash')
    cmd_parser_flash.add_argument('-t', '--tar', default=None, help='perform the upgrade from a tar[.gz] file')
    cmd_parser_flash.add_argument('-f', '--file', default=None, help='flash binary file to a single partition')
    cmd_parser_flash.add_argument('--secureboot', action='store_true', help='Flash Encrypted binaries if available')
    help_msg = 'The partition to flash ('
    for partition in PARTITIONS.keys():
        help_msg += (partition + ", ")
    cmd_parser_flash.add_argument('-p', '--partition', default=None, help=help_msg[:-2] + ')')
    # cmd_parser_flash.add_argument('-d', '--directory', default=None, help='directory to look for files when using -s / --script')
    # cmd_parser_flash.add_argument('-s', '--script', default=None, help='script file to execute')
    cmd_parser_copy = subparsers.add_parser('copy', help='Read/Write flash memory partition')
    help_msg = 'The partition to read/write ('
    for partition in PARTITIONS.keys():
        help_msg += (partition + ", ")
    cmd_parser_copy.add_argument('-p', '--partition', default=None, help=help_msg[:-2] + ')')
    cmd_parser_copy.add_argument('-f', '--file', default=None, help='name of the binary file (default: <wmac>-<part>.bin)')
    cmd_parser_copy.add_argument('-r', '--restore', default=False, action='store_true', help='restore partition from binary file')
    cmd_parser_copy.add_argument('-b', '--backup', action='store_true', help='backup partition to binary file (default)')

    cmd_parser_write = subparsers.add_parser('write', help='Write to flash memory')
    cmd_parser_write.add_argument('-a', '--address', default=None, type=int, help='address to write to')
    cmd_parser_write.add_argument('--contents', default=None, help='contents of the memory to write (base64)')

    cmd_parser_write_remote = subparsers.add_parser('write_remote', add_help=False)
    cmd_parser_write_remote.add_argument('--contents', default=None)

    cmd_parser_wifi = subparsers.add_parser('wifi', help='Get/Set default WIFI parameters')
    cmd_parser_wifi.add_argument('--ssid', default=None, help='Set Wifi SSID')
    cmd_parser_wifi.add_argument('--pwd', default=None, help='Set Wifi PWD')
    # This doesn't really work as we updated the field to a bitfield
    #cmd_parser_wifi.add_argument('--wob', type=str2bool, nargs='?', const=True, help='Set Wifi on boot')

    cmd_parser_pybytes = subparsers.add_parser('pybytes', help='Read/Write pybytes configuration')
    cmd_parser_pybytes.add_argument('--token', default=None, help='Set Device Token')
    cmd_parser_pybytes.add_argument('--mqtt', default=None, help='Set mqttServiceAddress')
    cmd_parser_pybytes.add_argument('--uid', default=None, help='Set userId')
    cmd_parser_pybytes.add_argument('--nwprefs', default=None, help='Set network preferences')
    cmd_parser_pybytes.add_argument('--extraprefs', default=None, help='Set extra preferences')
    cmd_parser_pybytes.add_argument('--carrier', default=None, help='Set LTE carrier')
    cmd_parser_pybytes.add_argument('--apn', default=None, help='Set LTE apn')
    cmd_parser_pybytes.add_argument('--type', default=None, help='Set LTE type')
    cmd_parser_pybytes.add_argument('--cid', default=None, help='Set LTE cid')
    cmd_parser_pybytes.add_argument('--band', default=None, help='Set LTE band')
    cmd_parser_pybytes.add_argument('--lte_reset', default=None, type=str2bool, nargs='?', const=True, help='Set LTE reset')
    cmd_parser_pybytes.add_argument('--auto_start', default=None, type=str2bool, nargs='?', const=True, help='Set Pybytes auto_start')

    cmd_parser_cb = subparsers.add_parser('cb', help='Read/Write config block')
    cmd_parser_cb.add_argument('-f', '--file', default=None, help='name of the backup file  (default: <wmac>.cb)')
    cmd_parser_cb.add_argument('-b', '--backup', action='store_true', help='backup cb partition to file')
    cmd_parser_cb.add_argument('-r', '--restore', action='store_true', help='restore cb partition from file (default)')

    cmd_parser_nvs = subparsers.add_parser('nvs', help='Read/Write non volatile storage')
    cmd_parser_nvs.add_argument('-f', '--file', default=None, help='name of the backup file (default: <wmac>.nvs)')
    cmd_parser_nvs.add_argument('-r', '--restore', action='store_true', help='restore nvs partition from file')
    cmd_parser_nvs.add_argument('-b', '--backup', action='store_true', help='backup nvs partition to file (default)')

    cmd_parser_ota = subparsers.add_parser('ota', help='Read/Write ota block')
    cmd_parser_ota.add_argument('-f', '--file', default=None, help='name of the backup file (default: <wmac>.ota)')
    cmd_parser_ota.add_argument('-r', '--restore', action='store_true', help='restore ota partition from file')
    cmd_parser_ota.add_argument('-b', '--backup', action='store_true', help='backup ota partition to file (default)')

    hlp_msg = ""
    for region in LORA_REGIONS:
        hlp_msg += " " + region
    cmd_parser_lpwan = subparsers.add_parser('lpwan', help='Get/Set LPWAN parameters [' + hlp_msg + ']')
    cmd_parser_lpwan.add_argument('--region', default=None, help='Set default LORA region')
    cmd_parser_lpwan.add_argument('--erase_region', action='store_true', help='Erase default LORA region')
    cmd_parser_lpwan.add_argument('--lora_region', action='store_true', help='Output only LORA region')

    subparsers.add_parser('erase_fs', help='Erase flash file system area')
    subparsers.add_parser('erase_all', help='Erase entire flash!')

    args = cmd_parser.parse_args()

#     try:
#         if args.command == 'flash' and (args.file is not None or args.partition is not None):
#             args.command = 'copy'
#             args.restore = True
#     except Exception as e:
#         print_exception(e)
#         raise e
#
    try:
        if args.command == 'boot_part' and args.partition is not None:
            if args.partition != 'factory' and args.partition != 'ota_0':
                raise ValueError('Partition must be factory or ota_0')
        if hasattr(args, "region") and args.region is not None:
            args.region = args.region.replace('"', '')
        if args.pic and args.ftdi:
            raise ValueError('Cannot force both ftdi and pic mode!')
        if args.pic and args.reset:
            raise ValueError('Cannot use Espressif reset in pic mode!')
        if args.command == 'flash' and args.tar is None and (args.file is None or args.partition is None):
            raise ValueError('You must either specifiy the tar[.gz] file or both the file and partition to flash')
        if  args.command != 'list' and args.port is None:
            try:
                args.port = os.environ['ESPPORT']
            except:
                raise ValueError('The port must be specified when using %s' % args.command)
        if  args.command != 'list' and args.speed is None:
            try:
                args.speed = int(os.environ['ESPBAUD'])
            except:
                args.speed = FAST_BAUD_RATE
        if (hasattr(args, "restore") and args.restore == True) and (hasattr(args, "backup") and args.backup == True):
            raise ValueError('Cannot backup and restore at the same time')
        if (hasattr(args, "command") and args.command == 'lpwan' and hasattr(args, "region") and args.region is not None):
            if (not check_lora_region(args.region)):
                err_msg = 'Invalid LoRa region ' + args.region + ' must be one of:'
                for region in LORA_REGIONS:
                    err_msg += " " + region
                raise ValueError(err_msg)
        if (args.command == 'copy' and (not hasattr(args, "partition") or args.partition is None)):
            err_msg = 'partition must be one of:'
            for partition in PARTITIONS.keys():
                err_msg += " " + partition
            raise ValueError(err_msg)

        if (args.command == 'copy' and hasattr(args, "partition") and args.partition is not None):
            if (not check_partition(args.partition)):
                err_msg = 'Invalid partition ' + args.partition + ' must be one of:'
                for partition in PARTITIONS.keys():
                    err_msg += " " + partition
                raise ValueError(err_msg)
    except Exception as e:
        print_exception(e)
        raise e
    return args


def mac_to_string(mac):
    result = ''

    if type(mac) == str:
        m = []
        for c in mac:
            m.append(struct.unpack("B", c)[0])
        mac = m

    for c in mac:
        result += '{:02X}'.format(c)

    return result


def print_result(result):
    print(result)


def main():
    try:
        args = process_arguments()

        if args.command == 'list':
            list_usbid()
            sys.exit(0)
        if sys.version_info[0] < 3:
            new_stream = BytesIO()
        else:
            new_stream = StringIO()
        old_stdout = sys.stdout
        if (args.ftdi):
            pypic = False
            if args.command == 'exit':
                sys.exit(0)
        elif (args.pic):
            pypic = True
        else:
            pypic = check_usbid(args.port)
        if not args.quiet:
            if (pypic):
                print("Running in PIC mode")
            else:
                print("Running in FTDI mode")
        if args.command == 'erase_all' and not args.quiet:
            print("Erasing the board can take up to 40 seconds.")

        if pypic == True:
            args.continuation = False

        if not (DEBUG or args.debug) and (args.verbose == False):
            sys.stdout = new_stream
        nPy = NPyProgrammer(args.port, args.speed, args.continuation, pypic, args.debug, args.reset)
        if hasattr(args, 'noexit') and args.noexit == True and not args.continuation:
            # Make sure we set the desired baud rate else we won't be able to re-connect later
            nPy.set_baudrate()

        if args.command == 'flash':

            def progress_fs(msg):
                msg = ' ' + msg + ' ' * 15
                padding = '\b' * len(msg)
                sys.stderr.write(msg + padding)
                sys.stderr.flush()

            if args.tar is not None:
                try:
                    tar_file = open(args.tar, "rb")
                    script = load_tar(tar_file, nPy, args.secureboot)
                    if not args.quiet:
                        # sys.stdout = old_stdout
                        sys.stderr.flush()
                        nPy.run_script(script, progress_fs=progress_fs)
                    else:
                        nPy.run_script(script)
                    sys.stdout = old_stdout
                    if not args.quiet:
                        print("Flash operation successful." + ' ' * 15)
                except Exception as e:
                    print_exception(e)
                    raise e
            else:
                try:
                    bf = open(args.file, 'rb')
                    content = bf.read()
                    bf.close()
                    script = [['w:{}'.format(args.file), args.partition, content]]
                    if not args.quiet:
                        sys.stderr.flush()
                        nPy.run_script(script, progress_fs=progress_fs)
                    else:
                        nPy.run_script(script)
                    sys.stdout = old_stdout
                    if not args.quiet:
                        print("Flash operation successful.")
                except Exception as e:
                    print_exception(e)
                    raise e

        elif args.command == 'wmac':
            wmac = mac_to_string(nPy.read_mac())
            sys.stdout = old_stdout
            print("WMAC=%s" % wmac)

        elif args.command == 'smac':
            smac = mac_to_string(nPy.read(int(PARTITIONS.get('config')[0], 16), 8))
            sys.stdout = old_stdout
            print("SMAC=%s" % smac)

        elif args.command == 'write':
            nPy.write(args.address, base64.b64decode(args.contents))
            if not args.quiet:
                sys.stdout = old_stdout
                print("Board programmed successfully")

        elif args.command == 'write_remote':
            nPy.write_remote(base64.b64decode(args.contents))
            if not args.quiet:
                sys.stdout = old_stdout
                print("Board configuration programmed successfully")

        elif args.command == 'chip_id':
            sys.stdout = old_stdout
            print(nPy.get_chip_id())

        elif args.command == 'erase_all':
            t = time.time()
            nPy.erase_all()
            if not args.quiet:
                sys.stdout = old_stdout
                if humanfriendly_available:
                    print("Board erased successfully in {} ".format(humanfriendly.format_timespan(time.time() - t)))
                else:
                    print("Board erased successfully in {0:.2f} seconds".format(round(time.time() - t, 2)))

        elif args.command == 'boot_part':
            if not args.quiet:
                sys.stdout = old_stdout
            if args.partition is None:
                ota_data = nPy.read(int(PARTITIONS.get('otadata')[0], 16), int(PARTITIONS.get('otadata')[1], 16))
                eprint(binascii.hexlify(ota_data[0:25]))

        elif args.command == 'sigfox':
            config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
            if args.id is not None or args.pac is not None or args.pubkey is not None or args.privkey is not None:
                new_config_block = nPy.set_sigfox_config(config_block, args.id, args.pac, args.pubkey, args.privkey)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
                sys.stdout = old_stdout
                if not args.quiet:
                    print("Sigfox credentials programmed successfully")
            else:
                sys.stdout = old_stdout
                sid = binascii.hexlify(config_block[8:12]).decode('UTF-8').upper()
                pac = binascii.hexlify(config_block[12:20]).decode('UTF-8').upper()
                print("SID: %s" % sid)
                print("PAC: %s" % pac)

        elif args.command == 'lpwan':
            config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
            if hasattr(args, "region") and args.region is not None:
                new_config_block = nPy.set_lpwan_config(config_block, args.region)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
                sys.stdout = old_stdout
                if not args.quiet:
                    print("Region " + args.region + " programmed successfully")
            elif hasattr(args, "erase_region") and args.erase_region:
                new_config_block = nPy.set_lpwan_config(config_block, 0xff)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
                sys.stdout = old_stdout
                if not args.quiet:
                    print("Region erased successfully")
            else:
                sys.stdout = old_stdout
                smac = mac_to_string(config_block[:8])
                sid = binascii.hexlify(config_block[8:12]).decode('UTF-8').upper()
                pac = binascii.hexlify(config_block[12:20]).decode('UTF-8').upper()
                if hasattr(args, "lora_region") and args.lora_region:
                    try:
                        region = config_block[52]
                        print(nPy.region2str(ord(region)))
                    except:
                        print("NONE")
                else:
                    print("SMAC: %s" % smac)
                    print("SID: %s" % sid)
                    print("PAC: %s" % pac)
                    try:
                        region = config_block[52]
                        if sys.version_info[0] < 3:
                            print("LORA REGION=%s" % nPy.region2str(ord(region)))
                        else:
                            print("LORA REGION=%s" % nPy.region2str(region))
                    except:
                        print("LORA REGION=[not set]")

        elif (args.command == 'copy'):
            if not (hasattr(args, 'file') and args.file is not None):
                wmac = mac_to_string(nPy.read_mac())
                fname = wmac + "-" + args.partition + ".bin"
            else:
                fname = args.file
            psize = nPy.int_flash_size() if args.partition == 'all' else int(PARTITIONS.get(args.partition)[1], 16)

            if (args.restore == False):
                if args.quiet:
                    flash_progress = None
                else:

                    def flash_progress(progress, length):
                        if humanfriendly_available:
                            msg = 'Read %s from %s (%d %%)%s' % (humanfriendly.format_size(progress, keep_width=(progress > 1024000), binary=True), args.partition, progress * 100.0 / length, ' ' * 15)
                        else:
                            msg = 'Read %s from %s (%d %%)%s' % (hr_size(progress), args.partition, progress * 100.0 / length, ' ' * 15)
                        padding = '\b' * len(msg)
                        # if progress == length:
                        #    padding = '\n'
                        sys.stderr.write(msg + padding)
                        sys.stderr.flush()

                partf = None
                try:
                    nPy.set_baudrate(True)
                    t = time.time()
                    data = nPy.esp.read_flash(int(PARTITIONS.get(args.partition)[0], 16), psize, flash_progress)
                    t = time.time() - t
                    partf = open(fname, "wb")
                    partf.write(data)
                    sys.stdout = old_stdout
                    if not args.quiet:
                        print('\rRead %d bytes at 0x%x in %.1f seconds (%.1f kbit/s)...'
                                % (len(data), int(PARTITIONS.get(args.partition)[0], 16), t, len(data) / t * 8 / 1000))
                        print('Partition saved to %s' % fname)
                except Exception as e:
                    print_exception(e)
                    raise e
                finally:
                    if partf is not None:
                        partf.close()
            else:

                def progress_fs(msg):
                    msg = ' ' + msg + ' ' * 15
                    padding = '\b' * len(msg)
                    sys.stderr.write(msg + padding)
                    sys.stderr.flush()

                try:
                    t = time.time()
                    partf = open(fname, "rb")
                    partition_bin = partf.read(int(PARTITIONS.get(args.partition)[1], 16))
                    partf.close()
                    if not args.quiet:
                        nPy.write(int(PARTITIONS.get(args.partition)[0], 16), partition_bin.ljust(psize, b'\xFF'), progress_fs=progress_fs)
                    else:
                        nPy.write(int(PARTITIONS.get(args.partition)[0], 16), partition_bin.ljust(psize, b'\xFF'), progress_fs=None)
                    if not args.quiet:
                        sys.stdout = old_stdout
                        t = time.time() - t
                        if humanfriendly_available:
                            print('Wrote %s from %s in %s (%.1f kbit/s)...' % (hr_size(len(partition_bin)), fname , humanfriendly.format_timespan(t), len(partition_bin) / t * 8 / 1000))
                        else:
                            print('Wrote %s from %s in %.1f seconds (%.1f kbit/s)...' % (hr_size(len(partition_bin)), fname , t, len(partition_bin) / t * 8 / 1000))
                except Exception as e:
                    print_exception(e)
                    raise e

        elif (args.command == 'cb'):
            if not (hasattr(args, 'file') and args.file is not None):
                wmac = mac_to_string(nPy.read_mac())
                fname = wmac + ".cb"
            else:
                fname = args.file
            if (args.restore == False):
                try:
                    if not fname == '-':
                        cbf = open(fname, "wb")
                    config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
                    if not fname == '-':
                        cbf.write(binascii.b2a_base64(config_block))
                        cbf.close()
                        if not args.quiet:
                            sys.stdout = old_stdout
                            print('Config block saved to %s' % fname)
                    else:
                        sys.stdout = old_stdout
                        print(binascii.b2a_base64(config_block))
                except Exception as e:
                    print_exception(e)
                    raise e
            else:
                try:
                    cbf = open(fname, "rb")
                    config_block = cbf.read()
                    cbf.close()
                    nPy.write(int(PARTITIONS.get('config')[0], 16), base64.b64decode(config_block)[:int(PARTITIONS.get('config')[1], 16)])
                    if not args.quiet:
                        sys.stdout = old_stdout
                        print('Config block restored from %s' % fname)
                except Exception as e:
                    print_exception(e)
                    raise e

        elif args.command == 'nvs':
            if not (hasattr(args, 'file') and args.file is not None):
                wmac = mac_to_string(nPy.read_mac())
                fname = wmac + ".nvs"
            else:
                fname = args.file
            if (args.restore == False):
                try:
                    if not fname == '-':
                        nvsf = open(fname, "wb")
                    nvs = nPy.read(int(PARTITIONS.get('nvs')[0], 16), int(PARTITIONS.get('nvs')[1], 16))
                    if not fname == '-':
                        nvsf.write(binascii.b2a_base64(nvs))
                        nvsf.close()
                        if not args.quiet:
                            sys.stdout = old_stdout
                            print('NVS partition saved to %s' % fname)
                    else:
                        sys.stdout = old_stdout
                        print(binascii.b2a_base64(nvs))
                except Exception as e:
                    print_exception(e)
                    raise e
            else:
                try:
                    nvsf = open(fname, "rb")
                    nvs = nvsf.read()
                    nvsf.close()
                    nPy.write(int(PARTITIONS.get('nvs')[0], 16), base64.b64decode(nvs)[:int(PARTITIONS.get('nvs')[1], 16)])
                    if not args.quiet:
                        sys.stdout = old_stdout
                        print('NVS partition restored from %s' % fname)
                except Exception as e:
                    print_exception(e)
                    raise e

        elif args.command == 'ota':
            if not (hasattr(args, 'file') and args.file is not None):
                wmac = mac_to_string(nPy.read_mac())
                fname = wmac + ".ota"
            else:
                fname = args.file
            if (args.restore == False):
                try:
                    if not fname == '-':
                        otaf = open(fname, "wb")
                    ota = nPy.read(int(PARTITIONS.get('otadata')[0], 16), int(PARTITIONS.get('otadata')[1], 16))
                    if not fname == '-':
                        otaf.write(binascii.b2a_base64(ota))
                        otaf.close()
                        if not args.quiet:
                            sys.stdout = old_stdout
                            print('OTA block saved to %s' % fname)
                    else:
                        sys.stdout = old_stdout
                        print(binascii.b2a_base64(ota))
                except Exception as e:
                    print_exception(e)
                    raise e
            else:
                try:
                    otaf = open(fname, "rb")
                    ota = otaf.read()
                    otaf.close()
                    nPy.write(int(PARTITIONS.get('otadata')[0], 16), base64.b64decode(ota)[:int(PARTITIONS.get('otadata')[1], 16)])
                    if not args.quiet:
                        sys.stdout = old_stdout
                        print('OTA block restored from %s' % fname)
                except Exception as e:
                    print_exception(e)
                    raise e

        elif args.command == 'wifi':
            config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
            if (hasattr(args, "ssid") and args.ssid is not None) or (hasattr(args, "pwd") and args.pwd is not None) or (hasattr(args, "wob") and args.wob is not None):
                new_config_block = nPy.set_wifi_config(config_block, args.ssid, args.pwd, args.wob)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
                sys.stdout = old_stdout
            else:
                sys.stdout = old_stdout
                try:
                    wifi_ssid = config_block[54:86].split(b'\x00')[0].decode('utf-8')
                    print("WIFI_SSID=%s" % wifi_ssid)
                except:
                    print("WIFI_SSID=[not set]")
                try:
                    wifi_pwd = config_block[87:151].split(b'\x00')[0].decode('utf-8')
                    print("WIFI_PWD=%s" % wifi_pwd)
                except:
                    print("WIFI_PWD=[not set]")
                try:
                    if sys.version_info[0] < 3:
                        wob = config_block[53]
                    else:
                        wob = config_block[53].to_bytes(1, byteorder='little')
                    print_debug('wob: {}'.format(wob))
                    if (wob == (b'\xfe') or wob == (b'\xba')):
                        print("WIFI_ON_BOOT=OFF")
                    else:
                        print("WIFI_ON_BOOT=ON")
                except:
                    print("WIFI_ON_BOOT=[not set]")

        elif args.command == 'pycom':
            config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
            nPy.print_cb(config_block)
            if (args.fs_type is not None):
                new_config_block = nPy.set_pycom_config(config_block, args.fs_type)
                nPy.print_cb(new_config_block)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
            else:
                sys.stdout = old_stdout
                try:
                    if sys.version_info[0] < 3:
                        fs_type = config_block[533]
                    else:
                        fs_type = config_block[533].to_bytes(1, byteorder='little')
                    if fs_type == (b'\x00'):
                        print("fs_type=FatFS")
                    if fs_type == (b'\x01'):
                        print("fs_type=LittleFS")
                    if fs_type == (b'\xff'):
                        print("fs_type=[not set]")
                except:
                    print("fs_type=[not set]")
                        
        elif args.command == 'pybytes':
            config_block = nPy.read(int(PARTITIONS.get('config')[0], 16), int(PARTITIONS.get('config')[1], 16))
            if (hasattr(args, "token") and args.token is not None) \
                or (hasattr(args, "mqtt") and args.mqtt is not None) \
                or (hasattr(args, "uid") and args.uid is not None) \
                or (hasattr(args, "nwprefs") and args.nwprefs is not None) \
                or (hasattr(args, "auto_start") and args.auto_start is not None) \
                or (hasattr(args, "carrier") and args.carrier is not None) \
                or (hasattr(args, "apn") and args.apn is not None) \
                or (hasattr(args, "type") and args.type is not None) \
                or (hasattr(args, "cid") and args.cid is not None) \
                or (hasattr(args, "band") and args.band is not None) \
                or (hasattr(args, "lte_reset") and args.lte_reset is not None) \
                or (hasattr(args, "extraprefs") and args.extraprefs is not None):
                new_config_block = nPy.set_pybytes_config(config_block, args.uid, args.token, args.mqtt, args.nwprefs, args.extraprefs, True, args.auto_start)
                new_config_block = nPy.set_lte_config(new_config_block, args.carrier, args.apn, args.type, args.cid, args.band, args.reset)
                nPy.write(int(PARTITIONS.get('config')[0], 16), new_config_block)
                sys.stdout = old_stdout
            else:
                sys.stdout = old_stdout
                try:
                    pybytes_device_token = config_block[162:201].split(b'\x00')[0].decode('utf-8')
                    print("TOKEN=%s" % pybytes_device_token)
                except:
                    print("TOKEN=[not set]")
                try:
                    pybytes_mqttServiceAddress = config_block[202:241].split(b'\x00')[0].decode('utf-8')
                    print("SERVER=%s" % pybytes_mqttServiceAddress)
                except:
                    print("SERVER=[not set]")
                try:
                    pybytes_userId = config_block[242:341].split(b'\x00')[0].decode('utf-8')
                    print("USERID=%s" % pybytes_userId)
                except:
                    print("USERID=[not set]")
                try:
                    pybytes_nwprefs = config_block[342:396].split(b'\x00')[0].decode('utf-8')
                    print("NWPREFS=%s" % pybytes_nwprefs)
                except:
                    print("NWPREFS=[not set]")
                try:
                    pybytes_eprefs = config_block[397:496].split(b'\x00')[0].decode('utf-8')
                    print("EXTRAPREFS=%s" % pybytes_eprefs)
                except:
                    print("EXTRAPREFS=[not set]")

        elif args.command == 'erase_fs':

            def progress_fs(msg):
                msg = msg + ' ' * 10
                padding = '\b' * len(msg)
                sys.stderr.write(msg + padding)
                sys.stderr.flush()

            try:
                nPy.erase_fs(progress_fs=progress_fs)
                if not args.quiet:
                    sys.stdout = old_stdout
                    print('Flash file system erasure successful' + ' ' * 10)
            except Exception as e:
                print_exception(e)
                raise e

        if (not (args.command == "list" or (hasattr(args, 'noexit') and args.noexit == True))) or args.command == 'exit':
            nPy.exit_pycom_programming_mode()

    except ValueError as e:
        print_exception(e)
        eprint(format(e))

    except Exception as e:
        if DEBUG:
            print_exc(e)
        else:
            eprint('Exception: {}'.format(e))
        try:
            nPy.exit_pycom_programming_mode()
        except Exception as e:
            sys.exit(2)
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
