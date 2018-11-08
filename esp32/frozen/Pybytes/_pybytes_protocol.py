try:
    from pybytes_library import PybytesLibrary
except:
    from _pybytes_library import PybytesLibrary
try:
    from mqtt import MQTTClient
except:
    from _mqtt import MQTTClient
from machine import Pin
from machine import ADC
from machine import PWM
from machine import Timer
from machine import reset
try:
    from terminal import Terminal
except:
    from _terminal import Terminal
try:
    from OTA import WiFiOTA
except:
    from _OTA import WiFiOTA
try:
    from flash_control_OTA import FCOTA
except:
    from _flash_control_OTA import FCOTA

import os
import _thread
import time
import socket
import struct
import binascii

__TYPE_PING = 0x00
__TYPE_INFO = 0x01
__TYPE_NETWORK_INFO = 0x02
__TYPE_SCAN_INFO = 0x03
__TYPE_BATTERY_INFO = 0x04
__TYPE_OTA = 0x05
__TYPE_FCOTA = 0x06
__TYPE_PYBYTES = 0x0E

__NETWORK_INFO_MASK = 0x30
__NETWORK_TYPE_WIFI = 0
__NETWORK_TYPE_LORA = 1
__NETWORK_TYPE_SIGFOX = 2
__NETWORK_TYPE_NBIOT = 3

__TERMINAL_PIN = 255

__CONNECTION_STATUS_DISCONNECTED = 0
__CONNECTION_STATUS_CONNECTED_MQTT = 1
__CONNECTION_STATUS_CONNECTED_COAP = 2
__CONNECTION_STATUS_CONNECTED_LORA = 3
__CONNECTION_STATUS_CONNECTED_SIGFOX = 4

__COMMAND_PIN_MODE = 0
__COMMAND_DIGITAL_READ = 1
__COMMAND_DIGITAL_WRITE = 2
__COMMAND_ANALOG_READ = 3
__COMMAND_ANALOG_WRITE = 4
__COMMAND_CUSTOM_METHOD = 5
__COMMAND_CUSTOM_LOCATION = 6

__FCOTA_COMMAND_HIERARCHY_ACQUISITION = 0x00
__FCOTA_COMMAND_FILE_ACQUISITION = 0x01
__FCOTA_COMMAND_FILE_UPDATE = 0x02
__FCOTA_PING = 0x03
__FCOTA_COMMAND_FILE_DELETE = 0x04

__USER_SYSTEM = 1

class PybytesProtocol:
    def __init__(self, config, message_callback):
        self.__conf = config
        self.__host = config['server']
        self.__ssl = config.get('ssl') and config['ssl'] == True
        if self.__ssl == True:
            self.__ssl_params = config.get('ssl_params')
        else:
            self.__ssl_params = {}
        self.__thread_stack_size = 8192
        self.__user_name = config['username']
        self.__device_id = config['device_id']
        self.__mqtt_download_topic = "d" + self.__device_id
        self.__mqtt_upload_topic = "u" + self.__device_id
        self.__mqtt_check_interval = 0.5
        self.__connection = None
        self.__connection_status = __CONNECTION_STATUS_DISCONNECTED
        self.__pybytes_library = PybytesLibrary()
        self.__user_message_callback = message_callback
        self.__pins = {}
        self.__pin_modes = {}
        self.__custom_methods = {}
        self.__terminal_enabled = False
        self.__battery_level = -1
        self.__lora_socket = None
        self.lora = None
        self.lora_lock = _thread.allocate_lock()
        self.__sigfox_socket = None
        self.__connectionAlarm = None
        self.__terminal = Terminal(self)
        self.__FCOTA = FCOTA()

    def send_keepalive(self, delay):
        while True:
            self.send_ping_message()
            time.sleep(delay)

    def print_pretty_response(self, rsp):
        lines = rsp.split('\r\n')
        for line in lines:
            if line:
                if line not in ['OK']:
                    print(line)

    def connect_nbiot(self, reconnect=True, check_interval=0.5):
        """Enstablishes a connection through NBIOT before connecting to mqtt server"""
        if (self.__connection_status != __CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_nbiot: Connection already exists. Disconnect First")
            return False
        if (os.uname()[0] not in ['FiPy', 'GPy']):
            print("You need a device with FiPy or GPy firmware to connect to NB-IOT")
            return False
        try:
            from network import LTE
            print("Trying to connect to Vodafone NB-IoT network...")
            lte = LTE()
            lte.send_at_cmd('AT+CFUN=0')
            lte.send_at_cmd('AT!="clearscanconfig"')
            lte.send_at_cmd('AT!="addscanfreq band=20 dl-earfcn=6300"')
            lte.send_at_cmd('AT!="zsp0:npc 1"')
            lte.send_at_cmd('AT+CGDCONT=1,"IP","nb.inetd.gdsp"')
            time.sleep(1)
            lte.send_at_cmd('AT+CFUN=1')
            while not lte.isattached():
                time.sleep(10)
                self.print_pretty_response(lte.send_at_cmd('AT+ceer'))
            print("Network attached.... trying to connect")
            lte.connect()
            while not lte.isconnected():
                pass
            print("LTE-M NB-IOT connection established")
            keep_alive_delay = 10
            _thread.start_new_thread(self.send_keepalive,[keep_alive_delay])
            print("LTE-M NB-IOT sending keep_alive every %d seconds" % keep_alive_delay)
            self.__mqtt_check_interval = check_interval
            try:
                self.__connection = MQTTClient(self.__device_id, self.__host, self.__mqtt_download_topic, user=self.__user_name,
                                               password=self.__device_id, reconnect=reconnect)
                self.__connection.connect()
                self.__connection_status = __CONNECTION_STATUS_CONNECTED_MQTT
                # Should I send type=WIFI or type=NBIOT ???
                self.__pybytes_library.set_network_type(__NETWORK_TYPE_WIFI)
                # self.__pybytes_library.set_network_type(__NETWORK_TYPE_NBIOT)
                self.__start_recv_mqtt()
                print("Connected to MQTT {}".format(self.__host))
                return True
            except Exception as ex:
                if '{}'.format(ex) == '4':
                    print('MQTT ERROR! Bad credentials when connecting to server: "{}"'.format(self.__host))
                else:
                    print("MQTT ERROR! {}".format(ex))
                return False
        except Exception as ex:
            print(ex)
            return False

    def connect_wifi(self, reconnect=True, check_interval=0.5):
        """Establish a connection through WIFI before connecting to mqtt server"""
        if (self.__connection_status != __CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_wifi: Connection already exists. Disconnect First")
            return False
        try:
            from network import WLAN
            antenna = self.__conf.get('wlan_antenna', WLAN.INT_ANT)
            known_nets = [((self.__conf['wifi']['ssid'], self.__conf['wifi']['password']))]
            if antenna == WLAN.EXT_ANT:
                print("WARNING! Using external WiFi antenna.")
            '''to connect it to an existing network, the WiFi class must be configured as a station'''
            wl = WLAN(mode=WLAN.STA, antenna=antenna)
            original_ssid = wl.ssid()
            original_auth = wl.auth()

            available_nets = wl.scan()
            nets = frozenset([e.ssid for e in available_nets])
            known_nets_names = frozenset([e[0]for e in known_nets])
            net_to_use = list(nets & known_nets_names)
            try:
                net_to_use = net_to_use[0]
                pwd = dict(known_nets)[net_to_use]
                sec = [e.sec for e in available_nets if e.ssid == net_to_use][0]
                wl.connect(net_to_use, (sec, pwd), timeout=10000)
                while not wl.isconnected():
                    time.sleep(0.1)
            except Exception as e:
                if (str(e) == "list index out of range"):
                    print("Please review Wifi SSID and password inside config")
                else:
                    print("Error connecting using WIFI: %s" % e)

                wl.init(mode=WLAN.AP, ssid=original_ssid, auth=original_auth,
                        channel=6, antenna=antenna)
                return False

            print("WiFi connection established")
            self.__mqtt_check_interval = check_interval
            try:
                self.__connection = MQTTClient(self.__device_id, self.__host, self.__mqtt_download_topic, user=self.__user_name,
                                               password=self.__device_id, reconnect=reconnect, ssl=self.__ssl, ssl_params = self.__ssl_params)
                self.__connection.connect()
                self.__connection_status = __CONNECTION_STATUS_CONNECTED_MQTT
                self.__pybytes_library.set_network_type(__NETWORK_TYPE_WIFI)
                self.__start_recv_mqtt()
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

    def __start_recv_mqtt(self):
        self.__connection.set_callback(self.__recv_mqtt)
        self.__connection.subscribe(self.__mqtt_download_topic)
        print('Using {} bytes as stack size'.format(self.__thread_stack_size))

        _thread.stack_size(self.__thread_stack_size)
        _thread.start_new_thread(self.__check_mqtt_message, ())
        self.__connectionAlarm = Timer.Alarm(self.__keep_connection, 60 * 10, periodic=True)

    def __check_mqtt_message(self):
        while(self.__connection_status == __CONNECTION_STATUS_CONNECTED_MQTT):
            try:
                self.__connection.check_msg()
                time.sleep(self.__mqtt_check_interval)
            except Exception as ex:
                print("Error receiving MQTT. Ignore this message if you disconnected")
                print(ex)

    def __keep_connection(self, alarm):
        if(self.__connection_status == __CONNECTION_STATUS_CONNECTED_MQTT):
            self.send_ping_message()
        else:
            print('The device disconnected from MQTT, auto reconnection: {}'.format(self.__reconnect))
            if (not self.__reconnect):
                raise Exception('Reconnection Disabled.')
            self.reconnect()

    # LORA
    def connect_lora_abp(self, lora_timeout, nanogateway):
        if (self.__connection_status != __CONNECTION_STATUS_DISCONNECTED):
            print("Error connect_lora_abp: Connection already exists. Disconnect First")
            return False
        try:
            from network import LoRa
        except Exception as ex:
            print("This device does not support LoRa connections: %s" % ex)
            return False

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
        if (self.__connection_status != __CONNECTION_STATUS_DISCONNECTED):
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

        self.__pybytes_library.set_network_type(__NETWORK_TYPE_LORA)
        self.__connection_status = __CONNECTION_STATUS_CONNECTED_LORA

        print("Connected using LoRa")

    def __check_lora_messages(self):
        print("__check_lora_messages started")
        while(True):
            message = None
            with self.lora_lock:
                self.__lora_socket.setblocking(False)
                message = self.__lora_socket.recv(256)
            if (message):
                self.__process_recv_message(message)
            time.sleep(0.5)

    # SIGFOX
    def connect_sigfox(self):
        if (self.__connection_status != __CONNECTION_STATUS_DISCONNECTED):
            print("Error: Connection already exists. Disconnect First")
            pass
        try:
            from network import Sigfox
        except:
            print("This device does not support Sigfox connections")
            return
        sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ1)
        self.__sigfox_socket = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW)
        self.__sigfox_socket.setblocking(True)
        self.__sigfox_socket.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, False)
        self.__pybytes_library.set_network_type(__NETWORK_TYPE_SIGFOX)
        self.__connection_status = __CONNECTION_STATUS_CONNECTED_SIGFOX
        print("Connected using Sigfox. Only upload stream is supported")

    def __recv_mqtt(self, topic, message):
        # print('Topic', topic, 'Message', message)
        self.__process_recv_message(message)

    def __process_recv_message(self, message):
        user, permanent, network_type, message_type, body = self.__pybytes_library.unpack_message(message)
        print('Recv message of type', message_type)

        # if (ttl > 0):
        #     print('time.time() - timestamp = {}'.format(time.time() - timestamp))
        #     if (time.time() - timestamp > ttl):
        #         print('message dropped')
        #         return

        if (user == __USER_SYSTEM):
            if (message_type == __TYPE_PING):
                self.send_ping_message()

            elif (message_type == __TYPE_INFO):
                self.send_info_message()

            elif (message_type == __TYPE_NETWORK_INFO):
                self.send_network_info_message()

            elif (message_type == __TYPE_SCAN_INFO):
                self.__send_message(self.__pybytes_library.pack_scan_info_message(self.lora))

            elif (message_type == __TYPE_BATTERY_INFO):
                self.send_battery_info()

            elif (message_type == __TYPE_OTA):
                # Perform OTA
                ota = WiFiOTA(self.__conf['wifi']['ssid'], self.__conf['wifi']['password'],
                              self.__conf['ota_server']['domain'], self.__conf['ota_server']['port'])

                if (self.__connection_status == __CONNECTION_STATUS_DISCONNECTED):
                    print('Connecting to WiFi')
                    ota.connect()

                print("Performing OTA")
                result = ota.update()
                self.send_ota_response(result)
                time.sleep(1.5)
                if (result == 2):
                    # Reboot the device to run the new decode
                    ota.reboot()

            elif (message_type == __TYPE_FCOTA):
                print('receiving FCOTA request')
                if (self.__connection_status == __CONNECTION_STATUS_DISCONNECTED):
                    print('Not connected, Re-Connecting to WiFi...')
                    ota.connect()

                command = body[0]
                if (command == __FCOTA_COMMAND_HIERARCHY_ACQUISITION):
                    self.send_fcota_ping('acquiring hierarchy...')
                    hierarchy = self.__FCOTA.get_flash_hierarchy()
                    self.send_fcota_hierarchy(hierarchy)

                elif (command == __FCOTA_COMMAND_FILE_ACQUISITION):
                    path = body[1:len(body)].decode()
                    if (path[len(path)-2:len(path)] == '.py'):
                        self.send_fcota_ping('acquiring file...')
                    content = self.__FCOTA.get_file_content(path)
                    size = self.__FCOTA.get_file_size(path)
                    self.send_fcota_file(content, path, size)

                elif (command == __FCOTA_COMMAND_FILE_UPDATE):
                    bodyString = body[1:len(body)].decode()
                    splittedBody = bodyString.split(',')
                    if (len(splittedBody) >= 2):
                        path = splittedBody[0]
                        print(path[len(path)-7:len(path)])
                        if (path[len(path)-7:len(path)] != '.pymakr'):
                            self.send_fcota_ping('updating file...')
                        newContent = bodyString[len(path)+1:len(body)]
                        if (self.__FCOTA.update_file_content(path, newContent) == True):
                            size = self.__FCOTA.get_file_size(path)
                            self.send_fcota_file(newContent, path, size)
                            if (path[len(path)-7:len(path)] != '.pymakr'):
                                time.sleep(2)
                                self.send_fcota_ping('board restarting...')
                                time.sleep(2)
                                reset()
                            else:
                                self.send_fcota_ping('pymakr archive updated!')
                        else:
                            self.send_fcota_ping('file update failed!')
                    else:
                        self.send_fcota_ping("file update failed!")

                elif (command == __FCOTA_PING):
                    self.send_fcota_ping('')

                elif (command == __FCOTA_COMMAND_FILE_DELETE):
                    self.send_fcota_ping('deleting file...')
                    path = body[1:len(body)].decode()
                    success = self.__FCOTA.delete_file(path)
                    if (success == True):
                        self.send_fcota_ping('file deleted!')
                        self.send_fcota_hierarchy(self.__FCOTA.get_flash_hierarchy())
                    else:
                        self.send_fcota_ping('deletion failed!')

                else:
                    print("Unknown FCOTA command received")

            elif (message_type == __TYPE_PYBYTES):
                command = body[0]
                pin_number = body[1]
                value = 0

                if (len(body) > 3):
                    value = body[2] << 8 | body[3]

                if (command == __COMMAND_PIN_MODE):
                    pass

                elif (command == __COMMAND_DIGITAL_READ):
                    pin_mode = None
                    try:
                        pin_mode = self.__pin_modes[pin_number]
                    except Exception as ex:
                        pin_mode = Pin.PULL_UP

                    self.send_pybytes_digital_value(False, pin_number, pin_mode)

                elif (command == __COMMAND_DIGITAL_WRITE):
                    if (not pin_number in self.__pins):
                        self.__configure_digital_pin(pin_number, Pin.OUT, None)
                    pin = self.__pins[pin_number]
                    pin(value)

                elif (command == __COMMAND_ANALOG_READ):
                    self.send_pybytes_analog_value(False, pin_number)

                elif (command == __COMMAND_ANALOG_WRITE):
                    if (not pin_number in self.__pins):
                        self.__configure_pwm_pin(pin_number)
                    pin = self.__pins[pin_number]
                    pin.duty_cycle(value * 100)

                elif (command == __COMMAND_CUSTOM_METHOD):
                    if (pin_number == __TERMINAL_PIN and self.__terminal_enabled):
                        self.__terminal.message_sent_from_pybytes_start()
                        terminal_command = body[2: len(body)]
                        terminal_command = terminal_command.decode("utf-8")

                        try:
                            out = eval(terminal_command)
                            if out is not None:
                                print(repr(out))
                            else:
                                print('\n')
                        except:
                            try:
                                exec(terminal_command)
                                print('\n')
                            except Exception as e:
                                print('Exception:\n  ' + repr(e))
                        self.__terminal.message_sent_from_pybytes_end()
                        return

                    if (self.__custom_methods[pin_number] is not None):
                        parameters = {}

                        for i in range(2, len(body), 3):
                            value = body[i: i + 2]
                            parameters[i / 3] = (value[0] << 8) | value[1]

                        method_return = self.__custom_methods[pin_number](parameters)

                        if (method_return is not None and len(method_return) > 0):
                            self.send_pybytes_custom_method_values(False, pin_number, method_return)

                    else:
                        print("WARNING: Trying to write to an unregistered Virtual Pin")

        else:
            try:
                self.__user_message_callback(message)
            except Exception as ex:
                print(ex)

    # COAP
    def connect_coap(self):
        # TODO: Implement Connection
        pass

    def recv_coap(self):
        # TODO: Start receiving COAP messages asynchronously
        pass

    # COMMON
    def disconnect(self):
        if (self.__connection_status == __CONNECTION_STATUS_DISCONNECTED):
            print("Already disconnected")
            pass

        if (self.__connection_status == __CONNECTION_STATUS_CONNECTED_MQTT):
            try:
                self.__connection.disconnect()
                self.__connection_status = __CONNECTION_STATUS_DISCONNECTED
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == __CONNECTION_STATUS_CONNECTED_LORA):
            try:
                self.__lora_socket.close()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__connection_status == __CONNECTION_STATUS_CONNECTED_SIGFOX):
            try:
                self.__sigfox_socket.close()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        if (self.__pybytes_library.get_network_type() == __NETWORK_TYPE_WIFI):
            try:
                from network import WLAN
                wl = WLAN()
                wl.deinit()
            except Exception as e:
                print("Error disconnecting: {}".format(e))

        self.__connection_status = __CONNECTION_STATUS_DISCONNECTED


    def __configure_digital_pin(self, pin_number, pin_mode, pull_mode):
        # TODO: Add a check for WiPy 1.0
        self.__pins[pin_number] = Pin("P" + str(pin_number), mode = pin_mode, pull=pull_mode)

    def __configure_analog_pin(self, pin_number):
        # TODO: Add a check for WiPy 1.0
        adc = ADC(bits=12)
        self.__pins[pin_number] = adc.channel(pin="P" + str(pin_number))

    def __configure_pwm_pin(self, pin_number):
        # TODO: Add a check for WiPy 1.0
        _PWMMap = {0: (0, 0),
                   1: (0, 1),
                   2: (0, 2),
                   3: (0, 3),
                   4: (0, 4),
                   8: (0, 5),
                   9: (0, 6),
                   10: (0, 7),
                   11: (1, 0),
                   12: (1, 1),
                   19: (1, 2),
                   20: (1, 3),
                   21: (1, 4),
                   22: (1, 5),
                   23: (1, 6)}
        pwm = PWM(_PWMMap[pin_number][0], frequency=5000)
        self.__pins[pin_number] = pwm.channel(_PWMMap[pin_number][1], pin="P" + str(pin_number),
                                              duty_cycle=0)

    def __send_message(self, message, topic=None):
        try:
            # print("__send_message {}".format(topic))
            finalTopic = self.__mqtt_upload_topic if topic is None else self.__mqtt_upload_topic + "/" + topic
            # print("finalTopic {}".format(finalTopic))

            # print('Sending %s', message)
            if (self.__connection_status == __CONNECTION_STATUS_CONNECTED_MQTT):
                # print("Publishing on {}".format(finalTopic))
                self.__connection.publish(finalTopic, message)
            elif (self.__connection_status == __CONNECTION_STATUS_CONNECTED_LORA):
                with self.lora_lock:
                    self.__lora_socket.setblocking(True)
                    self.__lora_socket.send(message)
                    self.__lora_socket.setblocking(False)
            elif (self.__connection_status == __CONNECTION_STATUS_CONNECTED_SIGFOX):
                if (len(message) > 12):
                    print ("WARNING: Message not sent, Sigfox only supports 12 Bytes messages")
                    return
                self.__sigfox_socket.send(message)

            else:
                # print("Error: Sending without a connection")
                pass
        except Exception as ex:
            print(ex)

    def send_user_message(self, persistent, message_type, body):
        self.__send_message(self.__pybytes_library.pack_user_message(persistent, message_type, body))

    def send_ping_message(self):
        self.__send_message(self.__pybytes_library.pack_ping_message())

    def send_info_message(self):
        self.__send_message(self.__pybytes_library.pack_info_message())

    def send_network_info_message(self):
        self.__send_message(self.__pybytes_library.pack_network_info_message())

    def send_scan_info_message(self, lora):
        print('WARNING! send_scan_info_message is deprecated and should be called only from Pybytes.')

    def send_battery_info(self):
        self.__send_message(self.__pybytes_library.pack_battery_info(self.__battery_level))

    def send_ota_response(self, result):
        print('Sending OTA result back {}'.format(result))
        self.__send_message(self.__pybytes_library.pack_ota_message(result), 'ota')

    def send_fcota_hierarchy(self, hierarchy):
        print('Sending FCOTA hierarchy back')
        self.__send_message(self.__pybytes_library.pack_fcota_hierarchy_message(hierarchy), 'fcota')

    def send_fcota_file(self, content, path, size):
        print('Sending FCOTA file back')
        self.__send_message(self.__pybytes_library.pack_fcota_file_message(content, path, size), 'fcota')

    def send_fcota_ping(self, activity):
        print('Sending FCOTA ping back: {}'.format(activity))
        self.__send_message(self.__pybytes_library.pack_fcota_ping_message(activity), 'fcota')

    def send_pybytes_digital_value(self, persistent, pin_number, pull_mode):
        if (not pin_number in self.__pins):
            self.__configure_digital_pin(pin_number, Pin.IN, pull_mode)
        pin = self.__pins[pin_number]
        self.__send_pybytes_message(persistent, __COMMAND_DIGITAL_WRITE, pin_number, pin())

    def send_pybytes_analog_value(self, persistent, pin_number):
        if (not pin_number in self.__pins):
            self.__configure_analog_pin(pin_number)
        pin = self.__pins[pin_number]

        self.__send_pybytes_message(persistent, __COMMAND_ANALOG_WRITE, pin_number, pin())

    def send_pybytes_custom_method_values(self, persistent, method_id, parameters):
        if(isinstance(parameters[0], int)):
            values = bytearray(struct.pack("i", parameters[0]))
            self.__send_pybytes_message_variable(persistent, __COMMAND_CUSTOM_METHOD, method_id, values)
        elif(isinstance(parameters[0], float)):
            values = bytearray(struct.pack("f", parameters[0]))
            self.__send_pybytes_message_variable(persistent, __COMMAND_CUSTOM_METHOD, method_id, values)
        elif(isinstance(parameters[0], tuple)):
            stringTuple = ', '.join(map(str, parameters[0]))
            values = stringTuple.encode("hex")
            self.__send_pybytes_message_variable(persistent, __COMMAND_CUSTOM_METHOD, method_id, values)
        else:
            values = parameters[0].encode("hex")
            self.__send_pybytes_message_variable(persistent, __COMMAND_CUSTOM_METHOD, method_id, values)

    def add_custom_method(self, method_id, method):
        self.__custom_methods[method_id] = method

    def __send_terminal_message(self, data):
        self.__send_pybytes_message_variable(False, __COMMAND_CUSTOM_METHOD, __TERMINAL_PIN, data)

    def enable_terminal(self):
        self.__terminal_enabled = True
        os.dupterm(self.__terminal)

    def __send_pybytes_message(self, persistant, command, pin_number, value):
        self.__send_message(self.__pybytes_library.pack_pybytes_message(persistant, command,
                                                                        pin_number, value))

    def __send_pybytes_message_variable(self, persistant, command, pin_number, parameters):
        self.__send_message(self.__pybytes_library.pack_pybytes_message_variable(persistant,
                                                                                 command,
                                                                                 pin_number,
                                                                                 parameters))

    def set_battery_level(self, battery_level):
        self.__battery_level = battery_level


    def is_connected(self):
        return not (self.__connection_status == __CONNECTION_STATUS_DISCONNECTED)
