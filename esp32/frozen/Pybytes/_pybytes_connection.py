try:
    from mqtt import MQTTClient
except:
    from _mqtt import MQTTClient
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

import os
import _thread
import time
import socket
import struct
import binascii

class PybytesConnection:
    def __init__(self, config, message_callback):
        self.__conf = config
        self.__host = config['server']
        self.__ssl = config.get('ssl') and config['ssl'] == True
        if self.__ssl == True:
            self.__ssl_params = config.get('ssl_params')
        else:
            self.__ssl_params = {}
        self.__user_name = config['username']
        self.__device_id = config['device_id']
        self.__mqtt_download_topic = "d" + self.__device_id
        self.__mqtt_upload_topic = "u" + self.__device_id
        self.__connection = None
        self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED
        self.__pybytes_protocol = PybytesProtocol(config, message_callback)
        self.__lora_socket = None
        self.lora = None
        self.lora_lock = _thread.allocate_lock()
        self.__sigfox_socket = None
        self.lte = None
        self.wlan = None
        self.__network_type = None


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

    def connect_wifi(self, reconnect=True, check_interval=0.5):
        """Establish a connection through WIFI before connecting to mqtt server"""
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_wifi: Connection already exists. Disconnect First")
            return False
        try:
            from network import WLAN
            antenna = self.__conf.get('wlan_antenna', WLAN.INT_ANT)
            known_nets = [((self.__conf['wifi']['ssid'], self.__conf['wifi']['password']))]
            if antenna == WLAN.EXT_ANT:
                print("WARNING! Using external WiFi antenna.")
            '''to connect it to an existing network, the WiFi class must be configured as a station'''
            self.wlan = WLAN(mode=WLAN.STA, antenna=antenna)
            original_ssid = self.wlan.ssid()
            original_auth = self.wlan.auth()

            available_nets = self.wlan.scan()
            nets = frozenset([e.ssid for e in available_nets])
            known_nets_names = frozenset([e[0]for e in known_nets])
            net_to_use = list(nets & known_nets_names)
            try:
                net_to_use = net_to_use[0]
                pwd = dict(known_nets)[net_to_use]
                sec = [e.sec for e in available_nets if e.ssid == net_to_use][0]
                self.wlan.connect(net_to_use, (sec, pwd), timeout=10000)
                while not self.wlan.isconnected():
                    time.sleep(0.1)
            except Exception as e:
                if (str(e) == "list index out of range"):
                    print("Please review Wifi SSID and password inside config")
                else:
                    print("Error connecting using WIFI: %s" % e)

                self.wlan.deinit()
                return False
            self.__network_type = constants.__NETWORK_TYPE_WIFI
            print("WiFi connection established")
            self.__mqtt_check_interval = check_interval
            try:
                self.__connection = MQTTClient(self.__device_id, self.__host, self.__mqtt_download_topic, user=self.__user_name,
                                               password=self.__device_id, reconnect=reconnect, ssl=self.__ssl, ssl_params = self.__ssl_params)
                self.__connection.connect()
                self.__connection_status = constants.__CONNECTION_STATUS_CONNECTED_MQTT_WIFI
                self.__pybytes_protocol.start_MQTT(self, check_interval, constants.__NETWORK_TYPE_WIFI)
                print("Connected to MQTT {}".format(self.__host))
                return True
            except Exception as ex:
                if '{}'.format(ex) == '4':
                    print('MQTT ERROR! Bad credentials when connecting to server: "{}"'.format(self.__host))
                else:
                    print("MQTT ERROR! {}".format(ex))
                return False
        except Exception as ex:
            print("Exception connect_wifi: {}".format(ex))
            return False

    def connect_lte(self, reconnect=True, check_interval=0.5):
        """Establish a connection through LTE before connecting to mqtt server"""
        lte_cfg = self.__conf.get('lte')
        if lte_cfg is not None:
            if (os.uname()[0] not in ['FiPy', 'GPy']):
                print("You need a device with FiPy or GPy firmware to connect via LTE")
                return False
            try:
                from network import LTE
                time.sleep(3)
                print_debug(1, 'LTE init(carrier={})'.format(lte_cfg.get('carrier')))
                self.lte = LTE(carrier=lte_cfg.get('carrier'))         # instantiate the LTE object
                print_debug(1, 'LTE attach(band={}, apn={})'.format(lte_cfg.get('band'), lte_cfg.get('apn')))
                self.lte.attach(band=lte_cfg.get('band'), apn=lte_cfg.get('apn'))        # attach the cellular modem to a base station
                while not self.lte.isattached():
                    time.sleep(0.25)
                time.sleep(1)
                print_debug(1, 'LTE connect(cid={})'.format(lte_cfg.get('cid',1)))
                self.lte.connect(cid=lte_cfg.get('cid',1))       # start a data session and obtain an IP address
                print_debug(1, 'LTE is_connected()')
                while not self.lte.isconnected():
                    time.sleep(0.25)
                print("LTE connection established")
                self.__network_type = constants.__NETWORK_TYPE_LTE
                self.__mqtt_check_interval = check_interval
                try:
                    self.__connection = MQTTClient(self.__device_id, self.__host, self.__mqtt_download_topic, user=self.__user_name,
                                                   password=self.__device_id, reconnect=reconnect, ssl=self.__ssl, ssl_params = self.__ssl_params)
                    self.__connection.connect()
                    self.__connection_status = constants.__CONNECTION_STATUS_CONNECTED_MQTT_LTE
                    self.__pybytes_protocol.start_MQTT(self, check_interval, constants.__NETWORK_TYPE_WIFI)
                    print("Connected to MQTT {}".format(self.__host))
                    return True
                except Exception as ex:
                    if '{}'.format(ex) == '4':
                        print('MQTT ERROR! Bad credentials when connecting to server: "{}"'.format(self.__host))
                    else:
                        print("MQTT ERROR! {}".format(ex))
                    return False
            except Exception as ex:
                print("Exception connect_lte: {}".format(ex))
                return False
        else:
            print("Error... missing configuration!")
            return False

    # LORA
    def connect_lora_abp(self, lora_timeout, nanogateway):
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_lora_abp: Connection already exists. Disconnect First")
            return False
        try:
            from network import LoRa
        except Exception as ex:
            print("This device does not support LoRa connections: %s" % ex)
            return False

        self.lora.nvram_restore()
        self.lora = LoRa(mode=LoRa.LORAWAN)

        dev_addr = self.__conf['lora']['abp']['dev_addr']
        nwk_swkey = self.__conf['lora']['abp']['nwk_skey']
        app_swkey = self.__conf['lora']['abp']['app_skey']
        timeout_ms = self.__conf.get('lora_timeout', lora_timeout) * 1000

        dev_addr = struct.unpack(">l", binascii.unhexlify(dev_addr.replace(' ', '')))[0]
        nwk_swkey = binascii.unhexlify(nwk_swkey.replace(' ', ''))
        app_swkey = binascii.unhexlify(app_swkey.replace(' ', ''))

        try:
            print("Trying to join LoRa.ABP for %d seconds..." % lora_timeout)
            self.lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey),
                           timeout=timeout_ms)

            # if you want, uncomment this code, but timeout must be 0
            # while not self.lora.has_joined():
            #     print("Joining...")
            #     time.sleep(5)

            self.__open_lora_socket(nanogateway)

            _thread.stack_size(self.__thread_stack_size)
            _thread.start_new_thread(self.__check_lora_messages, ())
            return True
        except Exception as e:
            message = str(e)
            if message == 'timed out':
                print("LoRa connection timeout: %d seconds" % lora_timeout)

            return False

    def connect_lora_otta(self, lora_timeout, nanogateway):
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_lora_otta: Connection already exists. Disconnect First")
            return False
        try:
            from network import LoRa
        except Exception as ex:
            print("This device does not support LoRa connections: %s" % ex)
            return False

        dev_eui = self.__conf['lora']['otaa']['app_device_eui']
        app_eui = self.__conf['lora']['otaa']['app_eui']
        app_key = self.__conf['lora']['otaa']['app_key']
        timeout_ms = self.__conf.get('lora_timeout',lora_timeout) * 1000

        self.lora.nvram_restore()
        self.lora = LoRa(mode=LoRa.LORAWAN)

        dev_eui = binascii.unhexlify(dev_eui.replace(' ', ''))
        app_eui = binascii.unhexlify(app_eui.replace(' ', ''))
        app_key = binascii.unhexlify(app_key.replace(' ', ''))
        try:
            print("Trying to join LoRa.OTAA for %d seconds..." % lora_timeout)
            self.lora.join(activation=LoRa.OTAA, auth=(dev_eui, app_eui, app_key),
                           timeout=timeout_ms)

            # if you want, uncomment this code, but timeout must be 0
            # while not self.lora.has_joined():
            #     print("Joining...")
            #     time.sleep(5)

            self.__open_lora_socket(nanogateway)

            _thread.stack_size(self.__thread_stack_size)
            _thread.start_new_thread(self.__check_lora_messages, ())
            return True
        except Exception as e:
            message = str(e)
            if message == 'timed out':
                print("LoRa connection timeout: %d seconds" % lora_timeout)

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
        if (self.__connection_status != constants.__CONNECTION_STATUS_DISCONNECTED):
            print("Error: Connection already exists. Disconnect First")
            pass
        try:
            from network import Sigfox
        except:
            print("This device does not support Sigfox connections")
            return
        sigfox_config = self.__conf.get('sigfox')
        if sigfox_config is not None and sigfox_config.get('RCZ') is not None:
            try:
                sf_rcz = int(sigfox_config.get('RCZ',0)) - 1
                if sf_rcz >= 1 and sf_rcz <= 4:
                    sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=sf_rcz)
                    self.__sigfox_socket = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW)
                    self.__sigfox_socket.setblocking(True)
                    self.__sigfox_socket.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, False)
                    self.__pybytes_library.set_network_type(constants.__NETWORK_TYPE_SIGFOX)
                    self.__network_type = constants.__NETWORK_TYPE_SIGFOX
                    self.__connection_status = constants.__CONNECTION_STATUS_CONNECTED_SIGFOX
                    print("Connected using Sigfox. Only upload stream is supported")
                else:
                    print('Invalid Sigfox RCZ specified in config!')
            except Exception as e:
                print('Exception in connect_sigfox: {}'.format(e))
        else:
            print('Invalid Sigfox RCZ specified in config!')

    # COMMON
    def disconnect(self):
        print_debug(1, 'self.__connection_status={} | self.__network_type={}'.format(self.__connection_status, self.__network_type))

        if (self.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED):
            print("Already disconnected")
            pass

        if (constants.__CONNECTION_STATUS_CONNECTED_MQTT_WIFI <= self.__connection_status <= constants.__CONNECTION_STATUS_CONNECTED_MQTT_LTE):
            print_debug(1, 'MQTT over WIFI||LTE... disconnecting MQTT')
            try:
                self.__connection.disconnect()
                self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_LORA):
            print_debug(1, 'Connected over LORA... closing socket and saving nvram')
            try:
                self.__lora_socket.close()
                self.lora.nvram_save()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_SIGFOX):
            print_debug(1, 'Connected over SIGFOX... closing socket')
            try:
                self.__sigfox_socket.close()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__network_type == constants.__NETWORK_TYPE_WIFI):
            print_debug(1, 'Connected over WIFI... disconnecting')
            try:
                self.wlan.deinit()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__network_type == constants.__NETWORK_TYPE_LTE):
            print_debug(1, 'Connected over LTE... disconnecting')
            try:
                lte_cfg = self.__conf.get('lte')
                print_debug(1, 'lte.deinit(reset={})'.format(lte_cfg.get('reset', False)))
                self.lte.deinit(reset=lte_cfg.get('reset', False))
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        self.__network_type = None
        self.__connection_status = constants.__CONNECTION_STATUS_DISCONNECTED

    def is_connected(self):
        return not (self.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED)
