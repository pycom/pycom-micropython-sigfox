import struct
import machine
import os
import re
from network import WLAN
try:
    from network import LoRa
except:
    pass

__PROTOCOL_VERSION = 0x04

__TYPE_PING = 0x00
__TYPE_INFO = 0x01
__TYPE_NETWORK_INFO = 0x02
__TYPE_SCAN_INFO = 0x03
__TYPE_BATTERY_INFO = 0x04
__TYPE_OTA = 0x05
__TYPE_FCOTA = 0x06
__TYPE_PYBYTES = 0x0E
__PYBYTES_PROTOCOL = ">B%ds"
__PYBYTES_PROTOCOL_PING = ">B"
__PYBYTES_INTERNAL_PROTOCOL = ">BBH"
__PYBYTES_INTERNAL_PROTOCOL_VARIABLE = ">BB%ds"

__WIFI_NETWORK_FORMAT = ">6sBb"  # {"ssid":"%s", "mac_addr":"%s", "channel":"%s", "power":"%s"}

__USER_SYSTEM_MASK = 0x80
__PERSISTENT_MASK = 0x40
__NETWORK_TYPE_MASK = 0x30
__TYPE_MASK = 0xF

__DEVICE_TYPE_WIPY = 0x00
__DEVICE_TYPE_LOPY = 0x01
__DEVICE_TYPE_WIPY_2 = 0x02
__DEVICE_TYPE_SIPY = 0x03
__DEVICE_TYPE_LOPY_4 = 0x04
__DEVICE_TYPE_UNKNOWN = 0x05

class PybytesLibrary:
    def __init__(self):
        self.__network_type = None

    def pack_user_message(self, persistent, message_type, body):
        return self.__pack_message(False, persistent, message_type, body)

    def pack_pybytes_message(self, persistant, command, pin, value):
        body = struct.pack(__PYBYTES_INTERNAL_PROTOCOL, command, pin, value)
        return self.__pack_message(True, persistant, __TYPE_PYBYTES, body)

    def pack_pybytes_message_variable(self, persistant, command, pin, parameters):
        body = struct.pack(__PYBYTES_INTERNAL_PROTOCOL_VARIABLE % len(parameters),
                           command, pin, parameters)
        return self.__pack_message(True, persistant, __TYPE_PYBYTES, body)

    def pack_ping_message(self):
        return self.__pack_message(True, False, __TYPE_PING, None)

    def pack_battery_info(self, level):
        body = bytearray()
        body.append(level)

        return self.__pack_message(True, False, __TYPE_BATTERY_INFO, body)

    def pack_info_message(self):
        body = bytearray()
        sysname = os.uname().sysname

        if (sysname == 'WiPy'):
            body.append(__DEVICE_TYPE_WIPY_2)
        elif (sysname == 'LoPy'):
            body.append(__DEVICE_TYPE_LOPY)
        elif (sysname == 'SiPy'):
            body.append(__DEVICE_TYPE_SIPY)
        elif (sysname == 'LoPy4'):
            body.append(__DEVICE_TYPE_LOPY_4)
        else:
            body.append(__DEVICE_TYPE_UNKNOWN)

        release = self.__calc_int_version(os.uname().release)
        body.append((release >> 24) & 0xFF)
        body.append((release >> 16) & 0xFF)
        body.append((release >> 8) & 0xFF)
        body.append(release & 0xFF)

        body.append(__PROTOCOL_VERSION)

        return self.__pack_message(True, False, __TYPE_INFO, body)

    def pack_ota_message(self, result):
        body = bytearray()
        body.append(result)
        return self.__pack_message(True, False, __TYPE_OTA, body)

    def pack_fcota_hierarchy_message(self, hierarchy):
        stringTuple = 'h'
        stringTuple += ', '.join(map(str, hierarchy))
        body = stringTuple.encode("hex")
        return self.__pack_message(True, False, __TYPE_FCOTA, body)

    def pack_fcota_file_message(self, content, path, size):
        stringTuple = 'f{},{},{}'.format(path, size, content)
        body = stringTuple.encode("hex")
        return self.__pack_message(True, False, __TYPE_FCOTA, body)

    def pack_fcota_ping_message(self, activity):
        stringTuple = 'p{}'.format(activity)
        body = stringTuple.encode("hex")
        return self.__pack_message(True, False, __TYPE_FCOTA, body)

    def pack_network_info_message(self):
        mac_addr = machine.unique_id()

        ipAddress = WLAN().ifconfig()[0].split(".")

        body = bytearray()
        body += mac_addr
        body.append(int(ipAddress[0]))
        body.append(int(ipAddress[1]))
        body.append(int(ipAddress[2]))
        body.append(int(ipAddress[3]))

        return self.__pack_message(True, False, __TYPE_NETWORK_INFO, body)

    def pack_scan_info_message(self, lora):
        wlan = WLAN()

        if (wlan.mode() != WLAN.STA):
            wlan.mode(WLAN.STA)

        wifi_networks = wlan.scan()

        body = bytearray()

        max_networks = 5
        if (len(wifi_networks) < 5):
            max_networks = len(wifi_networks)

        for x in range(0, max_networks):
            wifi_pack = struct.pack(__WIFI_NETWORK_FORMAT, wifi_networks[x][1],
                                    wifi_networks[x][3], wifi_networks[x][4])
            body += wifi_pack

        if (lora):
            body.append(lora.stats().rssi)

        return self.__pack_message(True, False, __TYPE_SCAN_INFO, body)

    def __pack_message(self, user, persistent, message_type, body):
        if self.__network_type is None:
            # print("Error packing message without connection")
            return
        header = 0

        if (user):
            header = __USER_SYSTEM_MASK
        if (persistent):
            header = header | __PERSISTENT_MASK

        header = header | ((self.__network_type << 4) & __NETWORK_TYPE_MASK)

        header = header | (message_type & __TYPE_MASK)

        if body is not None:
            # print('__pack_message: %s' % struct.pack(__PYBYTES_PROTOCOL % len(body), header, body))
            return struct.pack(__PYBYTES_PROTOCOL % len(body), header, body)
        return struct.pack(__PYBYTES_PROTOCOL_PING, header)

    def unpack_message(self, message):
        # print('unpack_message {}'.format(message))
        header, body = struct.unpack(__PYBYTES_PROTOCOL % (len(message) - 1), message)
        user = (header & __USER_SYSTEM_MASK) >> 7
        permanent = (header & __PERSISTENT_MASK) >> 6
        network_type = (header & __NETWORK_TYPE_MASK) >> 4
        message_type = header & __TYPE_MASK

        return user, permanent, network_type, message_type, body

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
