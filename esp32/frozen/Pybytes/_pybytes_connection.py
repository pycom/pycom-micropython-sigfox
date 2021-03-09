'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import os
import sys
import _thread
import time
import socket
import struct
import binascii
import pycom
from machine import WDT

try:
    from mqtt import MQTTClient
except:
    from _mqtt import MQTTClient
try:
    from pybytes_config_reader import PybytesConfigReader
except:
    from _pybytes_config_reader import PybytesConfigReader

try:
    from pybytes_protocol import PybytesProtocol
except:
    from _pybytes_protocol import PybytesProtocol

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug
try:
    from coap import COAPClient
except:
    from _coap import COAPClient


class PybytesConnection:
    def __init__(self, config, message_callback):
        if config is not None:
            self.__conf = config
            self.__conf_reader = PybytesConfigReader(config)
            try:
                self.__host = pycom.nvs_get('pybytes_server')
            except:
                self.__host = config.get('server')
            self.__ssl = config.get('ssl', False)
            self.__ssl_params = config.get('ssl_params', {})
            self.__user_name = config.get('username')
            self.__device_id = config.get('device_id')
            self.__mqtt_download_topic = "d" + self.__device_id
            self.__mqtt_upload_topic = "u" + self.__device_id
            self.__pybytes_protocol = PybytesProtocol(
                config, message_callback, pybytes_connection=self
            )
        self.__connection = None
        self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED
        self.__lora_socket = None
        self.lora = None
        self.lora_lock = _thread.allocate_lock()
        self.__sigfox_socket = None
        self.lte = None
        self.wlan = None
        self.__network_type = None
        self.__wifi_lte_watchdog = None

    def lte_ping_routine(self, delay):
        while True:
            self.send_ping_message()
            time.sleep(delay)

    def print_pretty_response(self, rsp):
        lines = rsp.split('\r\n')
        for line in lines:
            if line:
                if line not in ['OK']:
                    print(line)

    def __initialise_watchdog(self):
        if self.__conf.get('connection_watchdog', True):
            self.__wifi_lte_watchdog = WDT(
                timeout=constants.__WDT_TIMEOUT_MILLISECONDS
            )
            print('Initialized watchdog for WiFi and LTE connection with timeout {} ms'.format(constants.__WDT_TIMEOUT_MILLISECONDS)) # noqa
        else:
            print('Watchdog for WiFi and LTE was disabled, enable with "connection_watchdog": true in pybytes_config.json') # noqa

    # Establish a connection through WIFI before connecting to mqtt server
    def connect_wifi(self, reconnect=True, check_interval=0.5, timeout=120):
        self.__initialise_watchdog()

        if self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED: # noqa
            print("Error connect_wifi: Connection already exists. Disconnect First") # noqa
            return False
        try:
            from network import WLAN
            antenna = self.__conf.get('wlan_antenna', WLAN.INT_ANT)
            known_nets = [((self.__conf['wifi']['ssid'], self.__conf['wifi']['password']))] # noqa
            if antenna == WLAN.EXT_ANT:
                print("WARNING! Using external WiFi antenna.")

            '''to connect it to an existing network,
            the WiFi class must be configured as a station'''

            self.wlan = WLAN(mode=WLAN.STA, antenna=antenna)

            attempt = 0

            print_debug(3, 'WLAN connected? {}'.format(self.wlan.isconnected()))

            while not self.wlan.isconnected() and attempt < 3:
                attempt += 1
                print_debug(3, "Wifi connection attempt: {}".format(attempt))
                print_debug(3, 'WLAN connected? {}'.format(self.wlan.isconnected()))
                available_nets = None
                while available_nets is None:
                    try:
                        available_nets = self.wlan.scan()
                        for x in available_nets:
                            print_debug(5, x)
                        time.sleep(1)
                    except:
                        pass

                nets = frozenset([e.ssid for e in available_nets])
                known_nets_names = frozenset([e[0]for e in known_nets])
                net_to_use = list(nets & known_nets_names)
                try:
                    net_to_use = net_to_use[0]
                    pwd = dict(known_nets)[net_to_use]
                    sec = [e.sec for e in available_nets if e.ssid == net_to_use][0] # noqa
                    print_debug(99, "Connecting with {} and {}".format(net_to_use, pwd))
                    if sec == 0:
                        self.wlan.connect(net_to_use, timeout=self.__conf.get('wifi', {}).get('timeout', 10000))
                    else:
                        self.wlan.connect(net_to_use, (sec, pwd), timeout=self.__conf.get('wifi', {}).get('timeout', 10000))
                    start_time = time.time()
                    while not self.wlan.isconnected():
                        if time.time() - start_time > timeout:
                            raise TimeoutError('Timeout trying to connect via WiFi')  # noqa: F821
                        time.sleep(0.1)
                except Exception as e:
                    if str(e) == "list index out of range" and attempt == 3:
                        print("Please review Wifi SSID and password inside config")
                        self.wlan.deinit()
                        return False
                    elif attempt == 3:
                        print("Error connecting using WIFI: %s" % e)
                        return False
            self.__network_type = constants.__NETWORK_TYPE_WIFI
            print("WiFi connection established")
            return True
        except Exception as ex:
            print("Exception connect_wifi: {}".format(ex))
            return False

    # Establish a connection through LTE before connecting to mqtt server
    def connect_lte(self, activation_info=False, start_mqtt=True):
        if activation_info:
            lte_cfg = activation_info
        else:
            lte_cfg = self.__conf.get('lte')
            self.__initialise_watchdog()

        if lte_cfg is not None:
            if (os.uname().sysname not in ['FiPy', 'GPy']):
                print("You need a device with FiPy or GPy firmware to connect via LTE") # noqa
                return False
            try:
                from network import LTE
                time.sleep(3)
                if lte_cfg.get('carrier', 'standard') == 'standard':
                    carrier = None
                else:
                    carrier = lte_cfg.get('carrier')
                print_debug(1, 'LTE init(carrier={}, cid={})'.format(carrier, lte_cfg.get('cid', 1))) # noqa
                # instantiate the LTE object
                self.lte = LTE(carrier=carrier, cid=lte_cfg.get('cid', 1))
                try:
                    lte_type = lte_cfg.get('type') if len(lte_cfg.get('type')) > 0 else None
                except:
                    lte_type = None
                try:
                    lte_apn = lte_cfg.get('apn') if len(lte_cfg.get('type')) > 0 else None
                except:
                    lte_apn = None
                try:
                    lte_band = int(lte_cfg.get('band'))
                except:
                    lte_band = None
                print_debug(
                    1,
                    'LTE attach(band={}, apn={}, type={})'.format(
                        lte_band,
                        lte_apn,
                        lte_type
                    )
                )

                self.lte.attach(band=lte_band, apn=lte_apn, type=lte_type)  # noqa   # attach the cellular modem to a base station
                while not self.lte.isattached():
                    time.sleep(0.25)
                time.sleep(1)
                print_debug(1, 'LTE connect()')
                # start a data session and obtain an IP address
                self.lte.connect()
                print_debug(1, 'LTE is_connected()')
                while not self.lte.isconnected():
                    time.sleep(0.25)
                self.__network_type = constants.__NETWORK_TYPE_LTE
                print("LTE connection established")
                if start_mqtt:
                    print("connect_lte with start_mqtt is now removed please call communication_protocol or start_mqtt directly")
                return True
            except Exception as ex:
                print("Exception connect_lte: {}".format(ex))
                sys.print_exception(ex)
            return False
        else:
            print("Error... missing configuration!")
            return False

    # LORA
    def connect_lora_abp(self, lora_timeout, nanogateway):
        print_debug(1, 'Attempting to connect via LoRa')
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED): # noqa
            print("Error connect_lora_abp: Connection already exists. Disconnect First") # noqa
            return False
        try:
            from network import LoRa
        except Exception as ex:
            print("This device does not support LoRa connections: %s" % ex)
            return False

        lora_class = self.__conf.get('lora', {}).get('class', 0)
        if self.__conf.get('lora', {}).get('region') is not None:
            self.lora = LoRa(mode=LoRa.LORAWAN, region=self.__conf.get('lora').get('region'), device_class=lora_class)
        else:
            self.lora = LoRa(mode=LoRa.LORAWAN, device_class=lora_class)
        self.lora.nvram_restore()

        try:
            dev_addr = self.__conf['lora']['abp']['dev_addr']
            nwk_swkey = self.__conf['lora']['abp']['nwk_skey']
            app_swkey = self.__conf['lora']['abp']['app_skey']
        except Exception as ex:
            print("Invalid LoRaWAN ABP configuration!")
            print_debug(1, ex)
            return False
        timeout_ms = self.__conf.get('lora_timeout', lora_timeout) * 1000

        dev_addr = struct.unpack(">l", binascii.unhexlify(dev_addr.replace(' ', '')))[0] # noqa
        nwk_swkey = binascii.unhexlify(nwk_swkey.replace(' ', ''))
        app_swkey = binascii.unhexlify(app_swkey.replace(' ', ''))

        try:
            print("Trying to join LoRa.ABP for %d seconds..." % self.__conf.get('lora_timeout', lora_timeout))
            self.lora.join(
                activation=LoRa.ABP,
                auth=(dev_addr, nwk_swkey, app_swkey),
                timeout=timeout_ms
            )

            # if you want, uncomment this code, but timeout must be 0
            # while not self.lora.has_joined():
            #     print("Joining...")
            #     time.sleep(5)

            self.__open_lora_socket(nanogateway)
#            print_debug(5, 'Stack size: {}'.format(self.__thread_stack_size))
#            _thread.stack_size(self.__thread_stack_size)
#            _thread.start_new_thread(self.__check_lora_messages, ())
            return True
        except Exception as e:
            message = str(e)
            if message == 'timed out':
                print("LoRa connection timeout: %d seconds" % self.__conf.get('lora_timeout', lora_timeout))
            else:
                print_debug(3, 'Exception in LoRa connect: {}'.format(e))
            return False

    def connect_lora_otaa(self, lora_timeout, nanogateway):
        print_debug(1, 'Attempting to connect via LoRa')
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED): # noqa
            print("Error connect_lora_otaa: Connection already exists. Disconnect First") # noqa
            return False
        try:
            from network import LoRa
        except Exception as ex:
            print("This device does not support LoRa connections: %s" % ex)
            return False

        try:
            dev_eui = self.__conf['lora']['otaa']['app_device_eui']
            app_eui = self.__conf['lora']['otaa']['app_eui']
            app_key = self.__conf['lora']['otaa']['app_key']
        except Exception as ex:
            print("Invalid LoRaWAN OTAA configuration!")
            print_debug(1, ex)
            return False

        timeout_ms = self.__conf.get('lora_timeout', lora_timeout) * 1000

        lora_class = self.__conf.get('lora', {}).get('class', 0)
        if self.__conf.get('lora', {}).get('region') is not None:
            self.lora = LoRa(mode=LoRa.LORAWAN, region=self.__conf.get('lora', {}).get('region'), device_class=lora_class)
        else:
            self.lora = LoRa(mode=LoRa.LORAWAN, device_class=lora_class)
        self.lora.nvram_restore()

        dev_eui = binascii.unhexlify(dev_eui.replace(' ', ''))
        app_eui = binascii.unhexlify(app_eui.replace(' ', ''))
        app_key = binascii.unhexlify(app_key.replace(' ', ''))
        try:
            if not self.lora.has_joined():
                print("Trying to join LoRa.OTAA for %d seconds..." % self.__conf.get('lora_timeout', lora_timeout))
                self.lora.join(
                    activation=LoRa.OTAA,
                    auth=(dev_eui, app_eui, app_key),
                    timeout=timeout_ms
                )

            # if you want, uncomment this code, but timeout must be 0
            # while not self.lora.has_joined():
            #     print("Joining...")
            #     time.sleep(5)

            self.__open_lora_socket(nanogateway)
#            print_debug(5, 'Stack size: {}'.format(self.__thread_stack_size))
#            _thread.stack_size(self.__thread_stack_size)
#            _thread.start_new_thread(self.__check_lora_messages, ())
            return True
        except Exception as e:
            message = str(e)
            if message == 'timed out':
                print("LoRa connection timeout: %d seconds" % self.__conf.get('lora_timeout', lora_timeout))
            else:
                print_debug(3, 'Exception in LoRa connect: {}'.format(e))
            return False

    def __open_lora_socket(self, nanogateway):
        if (nanogateway):
            for i in range(3, 16):
                self.lora.remove_channel(i)

            self.lora.add_channel(0, frequency=868100000, dr_min=0, dr_max=5)
            self.lora.add_channel(1, frequency=868100000, dr_min=0, dr_max=5)
            self.lora.add_channel(2, frequency=868100000, dr_min=0, dr_max=5)

        print("Setting up LoRa socket...")
        self.__lora_socket = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.__lora_socket.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)

        self.__connection_status = constants.__CONNECTION_STATUS_CONNECTED_LORA

        self.__pybytes_protocol.start_Lora(self)

        print("Connected using LoRa")

    # SIGFOX
    def connect_sigfox(self):
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED): # noqa
            print("Error: Connection already exists. Disconnect First")
            pass
        try:
            from network import Sigfox
        except Exception:
            print("This device does not support Sigfox connections")
            return
        sigfox_config = self.__conf.get('sigfox', {})
        if sigfox_config is None or sigfox_config.get('RCZ') is None:
            print(constants.__SIGFOX_WARNING)
        try:
            sf_rcz = int(sigfox_config.get('RCZ', 1)) - 1
            if sf_rcz >= 0 and sf_rcz <= 3:
                Sigfox(mode=Sigfox.SIGFOX, rcz=sf_rcz)
                self.__sigfox_socket = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW) # noqa
                self.__sigfox_socket.setblocking(True)
                self.__sigfox_socket.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, False) # noqa
                self.__network_type = constants.__NETWORK_TYPE_SIGFOX
                self.__connection_status = constants.__CONNECTION_STATUS_CONNECTED_SIGFOX # noqa
                self.__pybytes_protocol.start_Sigfox(self)
                print(
                    "Connected using Sigfox. Only upload stream is supported"
                )
                return True
            else:
                print('Invalid Sigfox RCZ specified in config!')
                return False
        except Exception as e:
            print('Exception in connect_sigfox: {}'.format(e))
            return False

    # COMMON
    def disconnect(self, keep_wifi=False, force=False):

        if self.__wifi_lte_watchdog is not None:
            self.__wifi_lte_watchdog = WDT(timeout=constants.__WDT_MAX_TIMEOUT_MILLISECONDS)
            print('Watchdog timeout has been increased to {} ms'.format(constants.__WDT_MAX_TIMEOUT_MILLISECONDS)) # noqa

        print_debug(
            1,
            'self.__connection_status={} | self.__network_type={}'.format(
                self.__connection_status, self.__network_type
            )
        )

        if (self.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED): # noqa
            print_debug(3, "Already disconnected")

        if (constants.__CONNECTION_STATUS_CONNECTED_MQTT_WIFI <= self.__connection_status <= constants.__CONNECTION_STATUS_CONNECTED_MQTT_LTE): # noqa
            print_debug(1, 'MQTT over WIFI||LTE... disconnecting MQTT')
            try:
                self.__connection.disconnect(force=force)
                self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED # noqa
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_LORA): # noqa
            print_debug(1, 'Connected over LORA... closing socket and saving nvram') # noqa
            try:
                self.__lora_socket.close()
                self.lora.nvram_save()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_SIGFOX): # noqa
            print_debug(1, 'Connected over SIGFOX... closing socket')
            try:
                self.__sigfox_socket.close()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__network_type == constants.__NETWORK_TYPE_WIFI and not keep_wifi):
            print_debug(1, 'Connected over WIFI... disconnecting')
            try:
                self.wlan.deinit()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__network_type == constants.__NETWORK_TYPE_LTE):
            print_debug(1, 'Connected over LTE... disconnecting')
            try:
                lte_cfg = self.__conf.get('lte')
                print_debug(1, 'lte.deinit(reset={})'.format(lte_cfg.get('reset', False))) # noqa
                self.lte.deinit(reset=lte_cfg.get('reset', False))
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        self.__network_type = None
        self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED

    def is_connected(self):
        return not (self.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED) # noqa

    # Added for convention with other connectivity classes
    def isconnected(self):
        return not (self.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED) # noqa

    def communication_protocol(self, connection_type):
        if self.__conf_reader.get_communication_type() == constants.__SIMPLE_COAP:

            if connection_type == 'wifi':
                target_connection_type = constants.__CONNECTION_STATUS_CONNECTED_COAP_WIFI
            elif connection_type == 'lte':
                target_connection_type = constants.__CONNECTION_STATUS_CONNECTED_COAP_LTE

            return self.start_coap(target_connection_type)

        else:

            if connection_type == 'wifi':
                target_connection_type = constants.__CONNECTION_STATUS_CONNECTED_MQTT_WIFI
            elif connection_type == 'lte':
                target_connection_type = constants.__CONNECTION_STATUS_CONNECTED_MQTT_LTE

            return self.start_mqtt(target_connection_type)

    def start_coap(self, connection_type):
        print_debug(1, 'CoAP Protocol')
        self.__connection = COAPClient(
            self.__conf_reader.get_coap_host(),
            self.__conf_reader.get_coap_port(),
        )
        self.__connection_status = connection_type
        self.__pybytes_protocol.start_coap(
            self,
            self.__network_type
        )
        return True

    def start_mqtt(self, connection_type):
        print_debug(1, 'MQTT Protocol')
        try:
            self.__connection = MQTTClient(
                self.__device_id,
                self.__host,
                self.__mqtt_download_topic,
                self.__pybytes_protocol,
                user=self.__user_name,
                password=self.__device_id
            )
            self.__connection.connect()
            self.__connection_status = connection_type
            self.__pybytes_protocol.start_MQTT(
                self,
                self.__network_type
            )
            print("Connected to MQTT {}".format(self.__host))
            return True
        except Exception as ex:
            if '{}'.format(ex) == '4':
                print('MQTT ERROR! Bad credentials when connecting to server: "{}"'.format(self.__host))
            else:
                print("MQTT ERROR! {}".format(ex))
            return False
