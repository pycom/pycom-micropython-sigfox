'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''
try:
    import urequest
except:
    import _urequest as urequest

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

import pycom
import time
import json


class PybytesPymeshConfig():
    def __init__(self, pybytes=None):
        self.__pymesh = None
        self.__pymesh_config = None
        self.__pymesh_br_enabled = False
        self.__pack_tocken_prefix = "tkn"
        self.__pack_tocken_sep = "#"
        self.__pybytes = pybytes

    def pymesh_init(self):
        try:
            from pymesh_config import PymeshConfig
        except:
            from _pymesh_config import PymeshConfig

        try:
            from pymesh import Pymesh
        except:
            from _pymesh import Pymesh

        try:
            pycom.heartbeat(False)
        except:
            pass

        # read config file, or set default values
        self.__pymesh_config = PymeshConfig.read_config()

        # initialize Pymesh
        self.__pymesh = Pymesh(self.__pymesh_config, self.pymesh_new_message_cb)

        self.__pymesh_br_enabled = False

        if self.__pymesh_config.get("br_ena", False):
            if self.__pybytes.isconnected():
                if not self.__pymesh_br_enabled:
                    self.__pymesh_br_enabled = True
                    print_debug(99, "Set as border router")
                    self.__pymesh.br_set(PymeshConfig.BR_PRIORITY_NORM, self.pymesh_new_br_message_cb)

            else:  # not connected anymore to pybytes
                if self.__pymesh_br_enabled:
                    self.__pymesh_br_enabled = False
                    print_debug(99, "Remove as Pymesh border router")
                    self.__pymesh.br_remove()

    def unpack_pymesh_message(self, signal_number, value):
        deviceID = self.__pybytes.__conf["device_id"]
        monitoringData = json.dumps(self.__pymesh.mesh.get_node_info())
        if self.__pymesh_br_enabled:
            self.__pybytes.__pybytes_connection.__pybytes_protocol.send_pybytes_custom_method_values(signal_number, [value])
            self.__pybytes.send_node_signal(2, monitoringData, deviceID)
        else:
            pyb_port = self.__pymesh.mac() & 0xFFFF
            pyb_ip = '1:2:3::' + hex(pyb_port)[2:]
            pkt_start = self.__pack_tocken_prefix + self.__pack_tocken_sep + deviceID + self.__pack_tocken_sep

            # send data to the port equal with signal_number
            self.__pymesh.send_mess_external(pyb_ip, signal_number, pkt_start + value)

            time.sleep(3) # shouldn't send too fast to BR

            # hardcode monitoring data to be sent on signal #2
            self.__pymesh.send_mess_external(pyb_ip, 2, pkt_start + monitoringData)

    def pymesh_new_message_cb(self, rcv_ip, rcv_port, rcv_data):
        ''' callback triggered when a new packet arrived '''
        print_debug(99, 'Incoming %d bytes from %s (port %d):' % (len(rcv_data), rcv_ip, rcv_port))
        print_debug(99, 'Received: {} '.format(rcv_data))

        # user code to be inserted, to send packet to the designated Mesh-external interface
        try:
            for _ in range(3):
                pycom.rgbled(0x888888)
                time.sleep(.2)
                pycom.rgbled(0)
                time.sleep(.1)
        except:
            pass
        return

    def pymesh_new_br_message_cb(self, rcv_ip, rcv_port, rcv_data, dest_ip, dest_port):
        ''' callback triggered when a new packet arrived for the current Border Router,
        having destination an IP which is external from Mesh '''
        print_debug(99, 'Incoming %d bytes from %s (port %d), to external IPv6 %s (port %d)' % (len(rcv_data), rcv_ip, rcv_port, dest_ip, dest_port))
        print_debug(99, 'Received: {} '.format(rcv_data))

        try:
            for _ in range(2):
                pycom.rgbled(0x0)
                time.sleep(.1)
                pycom.rgbled(0x663300)
        except:
            pass
        # try to find Pybytes Token if include in rcv_data
        token = ""
        if rcv_data.startswith(self.__pack_tocken_prefix):
            x = rcv_data.split(self.__pack_tocken_sep.encode())
            if len(x) > 2:
                token = x[1]
                rcv_data = rcv_data[len(self.__pack_tocken_prefix) + len(token) + len(self.__pack_tocken_sep):]

                # send data to Pybytes only if it's coded properly
                pkt = 'BR %d B from %s (%s), to %s ( %d): %s' % (len(rcv_data), token, rcv_ip, dest_ip, dest_port, str(rcv_data))
                print_debug(99, 'Pymesh node packet: {} '.format(pkt))
                self.__pybytes.send_node_signal(dest_port & 0xFF, str(rcv_data.decode()).replace("#", ""), token.decode())
        return

    def get_config(self, token, silent=False):
        url = '{}://{}/pymesh/{}'.format(
            constants.__DEFAULT_PYCONFIG_PROTOCOL,
            constants.__DEFAULT_PYCONFIG_DOMAIN,
            token
        )
        try:
            pymesh = urequest.get(url, headers={'content-type': 'application/json'})
        except Exception as e:
            if not silent:
                print("Exception: {}".format(e))
        return pymesh.content

    def write_config(self, wmac, file='/flash/pymesh_config.json', pymeshSettings={}, silent=False):
        try:
            customSettings = json.loads(pymeshSettings.decode())
            default = {
                "LoRa": customSettings["LoRa"],
                "Pymesh": customSettings["Pymesh"],
                "debug": 5,
                "ble_api": False,
                "ble_name_prefix": "Device-{}".format(wmac),
                "br_prio": 0,
                "br_ena": customSettings["br_ena"],
                "autostart": True
            }

            f = open(file, 'w')
            f.write(json.dumps(default).encode('utf-8'))
            f.close()
            if not silent:
                print("Pymesh configuration written to {}".format(file))
        except Exception as e:
            if not silent:
                print("Exception: {}".format(e))
