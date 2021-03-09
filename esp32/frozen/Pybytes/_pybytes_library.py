'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

import struct
import machine
import os
import re
from network import WLAN
from time import sleep
import binascii

try:
    from network import LoRa # noqa
except:
    pass


class PybytesLibrary:
    def __init__(self, pybytes_connection, pybytes_protocol):
        self.__network_type = None
        self.__pybytes_connection = pybytes_connection
        self.__pybytes_protocol = pybytes_protocol

    def pack_user_message(self, message_type, body):
        return self.__pack_message(message_type, body)

    def pack_pybytes_message(self, command, pin, value):
        print_debug(5, "This is pack_pybytes_message({}, {}, {})".format(command, pin, value))
        body = struct.pack(constants.__PYBYTES_INTERNAL_PROTOCOL, command, pin, value)
        return self.__pack_message(constants.__TYPE_PYBYTES, body)

    def pack_release_info_message(self, releaseId):
        print_debug(5, "This is pack_release_info_message")
        print("This is pack_release_info_message")
        return self.__pack_message(constants.__TYPE_RELEASE_INFO, releaseId)

    def pack_pybytes_message_variable(self, command, pin, parameters, message_type):
        print_debug(5, "This is pack_pybytes_message_variable({}, {}, {})".format(command, pin, parameters))
        body = struct.pack(constants.__PYBYTES_INTERNAL_PROTOCOL_VARIABLE % len(parameters),
                           command, pin, parameters)
        return self.__pack_message(getattr(constants, message_type), body)

    def pack_ping_message(self):
        return self.__pack_message(constants.__TYPE_PING, None)

    def pack_battery_info(self, level):
        body = bytearray()
        body.append(level)

        return self.__pack_message(constants.__TYPE_BATTERY_INFO, body)

    def pack_info_message(self, releaseVersion=None):
        print_debug(5, "This is pack_info_message()")
        body = bytearray()
        sysname = os.uname().sysname

        if (sysname == 'WiPy'):
            body.append(constants.__DEVICE_TYPE_WIPY_2)
        elif (sysname == 'LoPy'):
            body.append(constants.__DEVICE_TYPE_LOPY)
        elif (sysname == 'SiPy'):
            body.append(constants.__DEVICE_TYPE_SIPY)
        elif (sysname == 'LoPy4'):
            body.append(constants.__DEVICE_TYPE_LOPY_4)
        else:
            body.append(constants.__DEVICE_TYPE_UNKNOWN)

        release = self.__calc_int_version(os.uname().release)
        body.append((release >> 24) & 0xFF)
        body.append((release >> 16) & 0xFF)
        body.append((release >> 8) & 0xFF)
        body.append(release & 0xFF)

        if releaseVersion is None:
            releaseVersion = 0

        body.append((releaseVersion >> 8) & 0xFF)
        body.append(releaseVersion & 0xFF)

        if hasattr(os.uname(), 'pymesh'):
            body.append(constants.__FWTYPE_PYMESH)
        elif hasattr(os.uname(), 'pygate'):
            body.append(constants.__FWTYPE_PYGATE)
        else:
            body.append(constants.__FWTYPE_DEFAULT)

        return self.__pack_message(constants.__TYPE_INFO, body)

    def pack_ota_message(self, result):
        body = bytearray()
        body.append(result)
        return self.__pack_message(constants.__TYPE_OTA, body)

    def pack_fcota_hierarchy_message(self, hierarchy):
        stringTuple = 'h'
        stringTuple += ', '.join(map(str, hierarchy))
        body = stringTuple.encode("hex")
        return self.__pack_message(constants.__TYPE_FCOTA, body)

    def pack_fcota_file_message(self, content, path, size):
        stringTuple = 'f{},{},{}'.format(path, size, content)
        body = stringTuple.encode("hex")
        return self.__pack_message(constants.__TYPE_FCOTA, body)

    def pack_fcota_ping_message(self, activity):
        stringTuple = 'p{}'.format(activity)
        body = stringTuple.encode("hex")
        return self.__pack_message(constants.__TYPE_FCOTA, body)

    def pack_network_info_message(self):
        mac_addr = machine.unique_id()

        ipAddress = WLAN().ifconfig()[0].split(".")

        body = bytearray()
        body += mac_addr
        body.append(int(ipAddress[0]))
        body.append(int(ipAddress[1]))
        body.append(int(ipAddress[2]))
        body.append(int(ipAddress[3]))

        return self.__pack_message(constants.__TYPE_NETWORK_INFO, body)

    def pack_scan_info_message(self, lora):
        print_debug(5, 'this is pack_scan_info_message()')

        scan_attempts = 0
        wifi_networks = []
        wifi_deinit = False
        if self.__pybytes_connection.wlan is None:
            self.__pybytes_connection.wlan = WLAN(mode=WLAN.STA)
            wifi_deinit = True

        while not wifi_networks and scan_attempts < 5:
            wifi_networks = self.__pybytes_connection.wlan.scan(type=WLAN.SCAN_PASSIVE)
            scan_attempts += 1
            print_debug(6, 'number of scanned wifi_networks: {}'.format(len(wifi_networks)))
            print_debug(6, 'scan_attempts: {}'.format(scan_attempts))

            if not wifi_networks:
                while not self.__pybytes_connection.wlan.isconnected():
                    print_debug(6, 'wifi disconnected, sleeping...')
                    sleep(0.5)

        body = bytearray()

        for x in range(0, len(wifi_networks)):
            wifi_pack = struct.pack(constants.__WIFI_NETWORK_FORMAT, wifi_networks[x][1],
                                    wifi_networks[x][3], wifi_networks[x][4])
            body += wifi_pack

        if (lora):
            body.append(lora.stats().rssi)

        if wifi_deinit:
            self.__pybytes_connection.wlan.deinit()
            self.__pybytes_connection.wlan = None
        return self.__pack_message(constants.__TYPE_SCAN_INFO, body)

    def __pack_message(self, message_type, body):
        if self.__network_type is None:
            print_debug(0, "Error packing message without connection")
            return
        header = 0

        header = header | ((self.__network_type << 4) & constants.__NETWORK_TYPE_MASK)
        header = header | (message_type & constants.__TYPE_MASK)

        if body is not None:
            print_debug(3, '__pack_message: %s' % binascii.hexlify(struct.pack(constants.__PYBYTES_PROTOCOL % len(body), header, body)))
            return struct.pack(constants.__PYBYTES_PROTOCOL % len(body), header, body)
        return struct.pack(constants.__PYBYTES_PROTOCOL_PING, header)

    def unpack_message(self, message):
        print_debug(5, 'This is PybytesLibrary.unpack_message(message={})'.format(message))
        header, body = struct.unpack(constants.__PYBYTES_PROTOCOL % (len(message) - 1), message)
        print_debug(6, 'header: {} body: {}'.format(header, body))
        network_type = (header & constants.__NETWORK_TYPE_MASK) >> 4
        print_debug(6, 'network_type: {}'.format(network_type))
        message_type = header & constants.__TYPE_MASK
        print_debug(6, 'message_type: {}'.format(message_type))
        return network_type, message_type, body

    def __calc_int_version(self, version):
        known_types = ['a', 'b', 'rc', 'r']
        version_parts = version.split(".")
        dots = len(version_parts) - 1

        if dots != 2 and dots != 3:
            return None

        if dots == 2:
            version_parts.append('r0')

        release_type_number = re.match("([^0-9]+)([0-9]+)", version_parts[3])
        release_type = known_types.index(release_type_number.group(1))
        if release_type == -1:
            return None

        version_parts[3] = release_type_number.group(2)

        # convert the numbers to integers
        for idx, val in enumerate(version_parts):
            version_parts[idx] = int(val)

        # number of bits per position: 6.7.10.2.7

        version = version_parts[0]
        version = (version << 7) | version_parts[1]
        version = (version << 10) | version_parts[2]
        version = (version << 2) | release_type
        version = (version << 7) | version_parts[3]

        return version

    def set_network_type(self, networkType):
        self.__network_type = networkType

    def get_network_type(self):
        return self.__network_type
