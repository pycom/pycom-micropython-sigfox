'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

try:
    from pybytes_library import PybytesLibrary
except:
    from _pybytes_library import PybytesLibrary

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

try:
    from terminal import Terminal
except:
    from _terminal import Terminal

try:
    from OTA import WiFiOTA
except:
    from _OTA import WiFiOTA

try:
    from pybytes_pymesh_config import PybytesPymeshConfig
except:
    from _pybytes_pymesh_config import PybytesPymeshConfig

try:
    from pybytes_machine_learning import MlFeatures
except:
    from _pybytes_machine_learning import MlFeatures

try:
    from pybytes_config_reader import PybytesConfigReader
except:
    from _pybytes_config_reader import PybytesConfigReader

try:
    from flash_control_OTA import FCOTA
except:
    from _flash_control_OTA import FCOTA

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug
try:
    import urequest
except:
    import _urequest as urequest

from machine import Pin
from machine import ADC
from machine import PWM
from machine import Timer
from machine import reset
from binascii import hexlify
from network import Coap

import os
import _thread
import time
import struct
import machine
import ujson
import pycom


class PybytesProtocol:
    def __init__(self, config, message_callback, pybytes_connection):
        self.__conf = config
        self.__conf_reader = PybytesConfigReader(config)
        self.__thread_stack_size = 8192
        self.__device_id = config['device_id']
        self.__mqtt_download_topic = "d" + self.__device_id
        self.__mqtt_upload_topic = "u" + self.__device_id
        self.__pybytes_connection = pybytes_connection
        self.__pybytes_library = PybytesLibrary(
            pybytes_connection=pybytes_connection, pybytes_protocol=self)
        self.__user_message_callback = message_callback
        self.__pins = {}
        self.__pin_modes = {}
        self.__custom_methods = {}
        self.__terminal_enabled = False
        self.__battery_level = -1
        self.__connectionAlarm = None
        self.__terminal = Terminal(self)
        self.__FCOTA = FCOTA()

    def start_Lora(self, pybytes_connection):
        print_debug(5, "This is PybytesProtocol.start_Lora()")
        self.__pybytes_connection = pybytes_connection
        self.__pybytes_library.set_network_type(constants.__NETWORK_TYPE_LORA)
        _thread.stack_size(self.__thread_stack_size)
        _thread.start_new_thread(self.__check_lora_messages, ())

    def start_MQTT(self, pybytes_connection, networkType):
        print_debug(5, "This is PybytesProtocol.start_MQTT")
        self.__pybytes_connection = pybytes_connection
        self.__pybytes_library.set_network_type(networkType)
        self.__pybytes_connection.__connection.subscribe(
            self.__mqtt_download_topic)
        self.__connectionAlarm = Timer.Alarm(
            self.__keep_connection,
            constants.__KEEP_ALIVE_PING_INTERVAL,
            periodic=True
        )

    def start_Sigfox(self, pybytes_connection):
        print_debug(5, "This is PybytesProtocol.start_Sigfox()")
        self.__pybytes_library.set_network_type(
            constants.__NETWORK_TYPE_SIGFOX)
        self.__pybytes_connection = pybytes_connection

    def start_coap(self, pybytes_connection, networkType):
        print_debug(5, "This is PybytesProtocol.start_coap()")
        self.__pybytes_connection = pybytes_connection
        self.__pybytes_library.set_network_type(networkType)

    def __wifi_or_lte_connection(self):
        return self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_MQTT_WIFI or self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_MQTT_LTE # noqa

    def __keep_connection(self, alarm):
        print_debug(
            5,
            "This is PybytesProtocol.__keep_connection(alarm={})".format(alarm)
        )
        if self.__wifi_or_lte_connection():
            self.send_ping_message()

    def __check_lora_messages(self):
        print_debug(5, "This is PybytesProtocol.__check_lora_messages()")
        while(True):
            message = None
            with self.__pybytes_connection.lora_lock:
                try:
                    self.__pybytes_connection.__lora_socket.setblocking(False)
                    message = self.__pybytes_connection.__lora_socket.recv(256)
                except Exception as ex:
                    print_debug(5, "Exception in PybytesProtocol.__check_lora_messages: {}".format(ex))
            if (message):
                self.__process_recv_message(message)
            time.sleep(0.5)

    def __process_recv_message(self, message):
        print_debug(5, "This is PybytesProtocol.__process_recv_message()")

        if message.payload:
            network_type, message_type, body = self.__pybytes_library.unpack_message(message.payload)
        else:
            # for lora messages
            network_type, message_type, body = self.__pybytes_library.unpack_message(message)

        if self.__user_message_callback is not None:
            if (message_type == constants.__TYPE_PING):
                self.send_ping_message()

            elif message_type == constants.__TYPE_PONG and self.__conf.get('connection_watchdog', True):
                self.__pybytes_connection.__wifi_lte_watchdog.feed()

            elif (message_type == constants.__TYPE_INFO):
                self.send_info_message()

            elif (message_type == constants.__TYPE_NETWORK_INFO):
                self.send_network_info_message()

            elif (message_type == constants.__TYPE_SCAN_INFO):
                self.__send_message(
                    self.__pybytes_library.pack_scan_info_message(
                        self.__pybytes_connection.lora
                    )
                )

            elif (message_type == constants.__TYPE_BATTERY_INFO):
                self.send_battery_info()

            elif (message_type == constants.__TYPE_RELEASE_DEPLOY):
                self.deploy_new_release(body)

            elif (message_type == constants.__TYPE_DEVICE_NETWORK_DEPLOY):
                ota = WiFiOTA(
                    self.__conf['wifi']['ssid'],
                    self.__conf['wifi']['password'],
                    self.__conf['ota_server']['domain'],
                    self.__conf['ota_server']['port']
                )
                ota.update_device_network_config(self.__FCOTA, self.__conf)

            elif (message_type == constants.__TYPE_PYMESH):
                # create pymesh config file
                wmac = hexlify(machine.unique_id()).decode('ascii')
                pymeshConfig = PybytesPymeshConfig()
                pymeshConfig.write_config(wmac=wmac.upper(), pymeshSettings=pymeshConfig.get_config(token=self.__conf['device_id']))

                # start OTA update
                ota = WiFiOTA(
                    self.__conf['wifi']['ssid'],
                    self.__conf['wifi']['password'],
                    self.__conf['ota_server']['domain'],
                    self.__conf['ota_server']['port']
                )

                if (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED):
                    print_debug(5, 'Connecting to WiFi')
                    ota.connect()

                print_debug(5, "Performing OTA")
                result = ota.update(fwtype="pymesh", token=self.__conf['device_id'])
                self.send_ota_response(result=result, topic='mesh')
                time.sleep(1.5)
                if (result == 2):
                    # Reboot the device to run the new decode
                    machine.reset()

            elif (message_type == constants.__TYPE_OTA):
                ota = WiFiOTA(
                    self.__conf['wifi']['ssid'],
                    self.__conf['wifi']['password'],
                    self.__conf['ota_server']['domain'],
                    self.__conf['ota_server']['port']
                )

                if (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED):
                    print_debug(5, 'Connecting to WiFi')
                    ota.connect()

                print_debug(5, "Performing OTA")
                result = ota.update()
                self.send_ota_response(result=result, topic='ota')
                time.sleep(1.5)
                if (result == 2):
                    # Reboot the device to run the new decode
                    machine.reset()

            elif (message_type == constants.__TYPE_FCOTA):
                print_debug(2, 'receiving FCOTA request')
                if (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED):
                    print_debug(5, 'Not connected, Re-Connecting ...')
                    ota.connect()

                command = body[0]
                if (command == constants.__FCOTA_COMMAND_HIERARCHY_ACQUISITION):
                    self.send_fcota_ping('acquiring hierarchy...')
                    hierarchy = self.__FCOTA.get_flash_hierarchy()
                    self.send_fcota_hierarchy(hierarchy)

                elif (command == constants.__FCOTA_COMMAND_FILE_ACQUISITION):
                    path = body[1:len(body)].decode()
                    if (path[len(path) - 2:len(path)] == '.py'):
                        self.send_fcota_ping('acquiring file...')
                    content = self.__FCOTA.get_file_content(path)
                    size = self.__FCOTA.get_file_size(path)
                    self.send_fcota_file(content, path, size)

                elif (command == constants.__FCOTA_COMMAND_FILE_UPDATE):
                    bodyString = body[1:len(body)].decode()
                    splittedBody = bodyString.split(',')
                    if (len(splittedBody) >= 2):
                        path = splittedBody[0]
                        print_debug(2, path[len(path) - 7:len(path)])
                        if (path[len(path) - 7:len(path)] != '.pymakr'):
                            self.send_fcota_ping('updating file...')
                        newContent = bodyString[len(path) + 1:len(body)]
                        if (self.__FCOTA.update_file_content(path, newContent) is True):
                            size = self.__FCOTA.get_file_size(path)
                            self.send_fcota_file(newContent, path, size)
                            if (path[len(path) - 7:len(path)] != '.pymakr'):
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

                elif (command == constants.__FCOTA_COMMAND_FILE_UPDATE_NO_RESET):
                    bodyString = body[1:len(body)].decode()
                    splittedBody = bodyString.split(',')
                    if (len(splittedBody) >= 2):
                        path = splittedBody[0]
                        print_debug(2, path[len(path) - 7:len(path)])
                        if (path[len(path) - 7:len(path)] != '.pymakr'):
                            self.send_fcota_ping('updating file...')
                        newContent = bodyString[len(path) + 1:len(body)]
                        if (self.__FCOTA.update_file_content(path, newContent) is True): # noqa
                            size = self.__FCOTA.get_file_size(path)
                            self.send_fcota_file(newContent, path, size)
                        else:
                            self.send_fcota_ping('file update failed!')
                    else:
                        self.send_fcota_ping("file update failed!")

                elif (command == constants.__FCOTA_PING):
                    self.send_fcota_ping('')

                elif (command == constants.__FCOTA_COMMAND_FILE_DELETE):
                    self.send_fcota_ping('deleting file...')
                    path = body[1:len(body)].decode()
                    success = self.__FCOTA.delete_file(path)
                    if (success is True):
                        self.send_fcota_ping('file deleted!')
                        self.send_fcota_hierarchy(
                            self.__FCOTA.get_flash_hierarchy()
                        )
                    else:
                        self.send_fcota_ping('deletion failed!')

                else:
                    print_debug(2, "Unknown FCOTA command received")

            elif (message_type == constants.__TYPE_PYBYTES):
                command = body[0]
                pin_number = body[1]
                value = 0

                if (len(body) > 3):
                    value = body[2] << 8 | body[3]

                if (command == constants.__COMMAND_START_SAMPLE):
                    parameters = ujson.loads(body[2: len(body)].decode("utf-8"))
                    sampling = MlFeatures(self, parameters=parameters)
                    sampling.start_sampling(pin=parameters["pin"])
                    self.send_ota_response(result=2, topic='sample')
                elif (command == constants.__COMMAND_DEPLOY_MODEL):
                    parameters = ujson.loads(body[2: len(body)].decode("utf-8"))
                    sampling = MlFeatures()
                    sampling.deploy_model(modelId=parameters["modelId"])
                    self.send_ota_response(result=2, topic='deploymlmodel')

                elif (command == constants.__COMMAND_PIN_MODE):
                    pass

                elif (command == constants.__COMMAND_DIGITAL_READ):
                    pin_mode = None
                    try:
                        pin_mode = self.__pin_modes[pin_number]
                    except Exception as ex:
                        print_debug(2, '{}'.format(ex))
                        pin_mode = Pin.PULL_UP

                    self.send_pybytes_digital_value(
                        False, pin_number, pin_mode
                    )

                elif (command == constants.__COMMAND_DIGITAL_WRITE):
                    if (pin_number not in self.__pins):
                        self.__configure_digital_pin(pin_number, Pin.OUT, None)
                    pin = self.__pins[pin_number]
                    pin(value)

                elif (command == constants.__COMMAND_ANALOG_READ):
                    self.send_pybytes_analog_value(False, pin_number)

                elif (command == constants.__COMMAND_ANALOG_WRITE):
                    if (pin_number not in self.__pins):
                        self.__configure_pwm_pin(pin_number)
                    pin = self.__pins[pin_number]
                    pin.duty_cycle(value * 100)

                elif (command == constants.__COMMAND_CUSTOM_METHOD):
                    if (pin_number == constants.__TERMINAL_PIN and self.__terminal_enabled):
                        original_dupterm = os.dupterm()
                        os.dupterm(self.__terminal)
                        self.__terminal.message_sent_from_pybytes_start()
                        terminal_command = body[2: len(body)]
                        terminal_command = terminal_command.decode("utf-8")

                        try:
                            out = eval(terminal_command)
                            if out is not None:
                                print(repr(out))
                            else:
                                print('\n')
                        except Exception:
                            try:
                                exec(terminal_command)
                                print('\n')
                            except Exception as e:
                                print('Exception:\n  ' + repr(e))
                        self.__terminal.message_sent_from_pybytes_end()
                        os.dupterm(original_dupterm)
                        return

                    if (self.__custom_methods[pin_number] is not None):
                        parameters = {}

                        for i in range(2, len(body), 3):
                            value = body[i: i + 2]
                            parameters[i / 3] = (value[0] << 8) | value[1]

                        method_return = self.__custom_methods[pin_number](parameters)

                        if (method_return is not None and len(method_return) > 0):
                            self.send_pybytes_custom_method_values(
                                signal_number, method_return
                            )

                    else:
                        print("WARNING: Trying to write to an unregistered Virtual Pin")

        else:
            try:
                self.__user_message_callback(message)
            except Exception as ex:
                print(ex)

    def __configure_digital_pin(self, pin_number, pin_mode, pull_mode):
        # TODO: Add a check for WiPy 1.0
        self.__pins[pin_number] = Pin(
            "P" + str(pin_number),
            mode=pin_mode,
            pull=pull_mode
        )

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
        self.__pins[pin_number] = pwm.channel(
            _PWMMap[pin_number][1], pin="P" + str(pin_number),
            duty_cycle=0
        )

    def __send_message(self, message, topic=None, priority=True):
        finalTopic = self.__mqtt_upload_topic if topic is None else self.__mqtt_upload_topic + "/" + topic
        if self.__conf_reader.get_communication_type() == constants.__SIMPLE_COAP:
            print_debug(1, 'CoAP Protocol')
            owner = self.__conf.get('username')
            self.__pybytes_connection.__connection.send_coap_message(
                message,
                Coap.REQUEST_GET,
                '{}/{}'.format(owner, finalTopic),
                "t{}".format(time.time()),
            )
        else:
            print_debug(1, 'MQTT Protocol')
            try:
                if self.__wifi_or_lte_connection():
                    self.__pybytes_connection.__connection.publish(
                        finalTopic, message, priority=priority
                    )
                elif (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_LORA):
                    with self.__pybytes_connection.lora_lock:
                        self.__pybytes_connection.__lora_socket.setblocking(True)
                        self.__pybytes_connection.__lora_socket.send(message)
                        self.__pybytes_connection.__lora_socket.setblocking(False)
                elif (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_CONNECTED_SIGFOX):
                    if (len(message) > 12):
                        print("WARNING: Message not sent, Sigfox only supports 12 Bytes messages")
                        return
                    self.__pybytes_connection.__sigfox_socket.send(message)

                else:
                    print_debug(2, "Warning: Sending without a connection")
                    pass
            except Exception as ex:
                print(ex)

    def send_user_message(self, message_type, body):
        self.__send_message(
            self.__pybytes_library.pack_user_message(
                message_type, body
            )
        )

    def send_ping_message(self):
        self.__send_message(self.__pybytes_library.pack_ping_message())

    def send_info_message(self):
        try:
            releaseVersion = self.__conf['application']['release']['version']
        except:
            releaseVersion = None
        self.__send_message(self.__pybytes_library.pack_info_message(releaseVersion))

    def send_network_info_message(self):
        self.__send_message(self.__pybytes_library.pack_network_info_message())

    def send_scan_info_message(self, lora):
        print('WARNING! send_scan_info_message is deprecated and should be called only from Pybytes.')

    def send_battery_info(self):
        self.__send_message(
            self.__pybytes_library.pack_battery_info(
                self.__battery_level
            )
        )

    def send_ota_response(self, result, topic):
        print_debug(2, 'Sending OTA result back {}'.format(result))
        self.__send_message(
            self.__pybytes_library.pack_ota_message(result),
            topic
        )

    def send_fcota_hierarchy(self, hierarchy):
        print_debug(2, 'Sending FCOTA hierarchy back')
        self.__send_message(
            self.__pybytes_library.pack_fcota_hierarchy_message(hierarchy),
            'fcota'
        )

    def send_fcota_file(self, content, path, size):
        print_debug(2, 'Sending FCOTA file back')
        self.__send_message(
            self.__pybytes_library.pack_fcota_file_message(
                content, path, size
            ),
            'fcota'
        )

    def send_fcota_ping(self, activity):
        print_debug(2, 'Sending FCOTA ping back: {}'.format(activity))
        self.__send_message(
            self.__pybytes_library.pack_fcota_ping_message(activity),
            'fcota'
        )

    def send_pybytes_digital_value(self, pin_number, pull_mode):
        if (pin_number not in self.__pins):
            self.__configure_digital_pin(pin_number, Pin.IN, pull_mode)
        pin = self.__pins[pin_number]
        self.send_pybytes_custom_method_values(signal_number, [pin()])

    def send_pybytes_analog_value(self, pin_number):
        if (pin_number not in self.__pins):
            self.__configure_analog_pin(pin_number)
        pin = self.__pins[pin_number]
        self.send_pybytes_custom_method_values(signal_number, [pin()])

    def send_pybytes_custom_method_values(self, method_id, parameters, topic=None):
        if(isinstance(parameters[0], int)):
            values = bytearray(struct.pack(">i", parameters[0]))
            values.append(constants.__INTEGER)
            self.__send_pybytes_message_variable(
                constants.__COMMAND_CUSTOM_METHOD, method_id, values, topic
            )
        elif(isinstance(parameters[0], float)):
            values = bytearray(struct.pack("<f", parameters[0]))
            values.append(constants.__FLOAT)
            self.__send_pybytes_message_variable(
                constants.__COMMAND_CUSTOM_METHOD, method_id, values, topic
            )
        elif(isinstance(parameters[0], tuple) or isinstance(parameters[0], list)):
            stringTuple = '[' + ', '.join(map(str, parameters[0])) + ']' + str(constants.__STRING)
            values = stringTuple.encode("hex")
            self.__send_pybytes_message_variable(
                constants.__COMMAND_CUSTOM_METHOD, method_id, values, topic
            )
        else:
            values = (parameters[0] + str(constants.__STRING)).encode("hex")
            self.__send_pybytes_message_variable(
                constants.__COMMAND_CUSTOM_METHOD, method_id, values, topic
            )

    def add_custom_method(self, method_id, method):
        self.__custom_methods[method_id] = method

    def __send_terminal_message(self, data):
        self.__send_pybytes_message_variable(
            constants.__COMMAND_CUSTOM_METHOD,
            constants.__TERMINAL_PIN,
            data,
            priority=True
        )

    def enable_terminal(self):
        self.__terminal_enabled = True
        # os.dupterm(self.__terminal)

    def __send_pybytes_message(self, command, pin_number, value):
        self.__send_message(
            self.__pybytes_library.pack_pybytes_message(
                command, pin_number, value
            )
        )

    def __send_pybytes_message_variable(
        self,
        command,
        pin_number,
        parameters,
        topic=None,
        priority=False,
    ):
        if (topic):
            message_type = '__TYPE_PYMESH'
        else:
            message_type = '__TYPE_PYBYTES'

        self.__send_message(
            self.__pybytes_library.pack_pybytes_message_variable(
                command, pin_number, parameters, message_type
            ),
            priority=priority,
            topic=topic
        )

    def set_battery_level(self, battery_level):
        self.__battery_level = battery_level

    def write_firmware(self, customManifest=None):
        ota = WiFiOTA(
            self.__conf['wifi']['ssid'],
            self.__conf['wifi']['password'],
            self.__conf['ota_server']['domain'],
            self.__conf['ota_server']['port']
        )

        if (self.__pybytes_connection.__connection_status == constants.__CONNECTION_STATUS_DISCONNECTED):
            print_debug(5, 'Connecting to WiFi')
            ota.connect()

        print_debug(5, "Performing OTA")
        result = ota.update(customManifest)
        self.send_ota_response(result=result, topic='ota')
        time.sleep(1.5)
        if (result == 2):
            # Reboot the device to run the new decode
            machine.reset()

    def get_application_details(self, body):
        application = self.__conf.get('application')
        if application is not None:
            if 'release' in application and 'codeFilename' in application['release']:
                currentReleaseID = application['release']['codeFilename']
            else:
                currentReleaseID = None
        else:
            currentReleaseID = None
            self.__conf['application'] = {
                "id": "",
                "release": {
                      "id": "",
                      "codeFilename": "",
                      "version": 0
                }
            }
        applicationID = body['applicationId']
        return (applicationID, currentReleaseID)

    def get_update_manifest(self, applicationID, newReleaseID, currentReleaseID):
        manifestURL = '{}://{}/manifest.json?'.format(constants.__DEFAULT_PYCONFIG_PROTOCOL, constants.__DEFAULT_PYCONFIG_DOMAIN)
        targetURL = '{}app_id={}&target_ver={}&current_ver={}&device_id={}'.format(
                    manifestURL,
                    applicationID,
                    newReleaseID,
                    currentReleaseID,
                    self.__conf['device_id']
        )
        print_debug(6, "manifest URL: {}".format(targetURL))
        try:
            pybytes_activation = urequest.get(targetURL, headers={'content-type': 'application/json'})
            letResp = pybytes_activation.json()
            pybytes_activation.close()
            print_debug(6, "letResp: {}".format(letResp))
            return letResp
        except Exception as ex:
            print_debug(1, "error while calling {}!: {}".format(targetURL, ex))
            return

        if 'errorMessage' in letResp:
            print_debug(1, letResp['errorMessage'])
            return

    def update_files(self, letResp, applicationID, newReleaseID):
        fileUrl = '{}://{}/files?'.format(constants.__DEFAULT_PYCONFIG_PROTOCOL, constants.__DEFAULT_PYCONFIG_DOMAIN)
        try:
            newFiles = letResp['newFiles']
            updatedFiles = letResp['updatedFiles']
            newFiles.extend(updatedFiles)
        except Exception as e:
            print_debug(1, "error getting files {}".format(e))
            newFiles = []

        for file in newFiles:
            targetFileLocation = '{}application_id={}&target_ver={}&target_path={}'.format(
                fileUrl,
                applicationID,
                newReleaseID,
                file['fileName']
            )
            try:
                getFile = urequest.get(targetFileLocation, headers={'content-type': 'text/plain'})
            except Exception as e:
                print_debug(1, "error getting {}! {}".format(targetFileLocation, e))
                continue

            fileContent = getFile.content
            self.__FCOTA.update_file_content(file['fileName'], fileContent)

    def delete_files(self, letResp):
        if 'deletedFiles' in letResp:
            deletedFiles = letResp['deletedFiles']
            for file in deletedFiles:
                self.__FCOTA.delete_file(file['fileName'])

    def update_application_config(self, letResp, applicationID):
        try:
            self.__conf['application']["id"] = applicationID
            self.__conf['application']['release']['id'] = letResp['target_version']['id']
            self.__conf['application']['release']['codeFilename'] = letResp['target_version']['codeFileName']
            try:
                self.__conf['application']['release']['version'] = int(letResp['target_version']['version'])
            except Exception as e:
                print_debug(1, "error while converting version: {}".format(e))

            json_string = ujson.dumps(self.__conf)
            print_debug(1, "json_string: {}".format(json_string))
            self.__FCOTA.update_file_content('/flash/pybytes_config.json', json_string)
        except Exception as e:
            print_debug(1, "error while updating pybytes_config.json! {}".format(e))

    def update_network_config(self, letResp):
        try:
            if 'networkConfig' in letResp:
                netConf = letResp['networkConfig']
                self.__conf['network_preferences'] = netConf['networkPreferences']
                if 'wifi' in netConf:
                    self.__conf['wifi'] = netConf['wifi']
                elif 'wifi' in self.__conf:
                    del self.__conf['wifi']

                if 'lte' in netConf:
                    self.__conf['lte'] = netConf['lte']
                elif 'lte' in self.__conf:
                    del self.__conf['lte']

                if 'lora' in netConf:
                    self.__conf['lora'] = {
                        'otaa': netConf['lora']['otaa'],
                        'abp': netConf['lora']['abp']
                    }
                elif 'lora' in self.__conf:
                    del self.__conf['lora']

                json_string = ujson.dumps(self.__conf)
                print_debug(1, "update_network_config : {}".format(json_string))
                self.__FCOTA.update_file_content('/flash/pybytes_config.json', json_string)
        except Exception as e:
            print_debug(1, "error while updating network config pybytes_config.json! {}".format(e))

    def update_firmware(self, body, applicationID, fw_type='pybytes'):
        if "firmware" not in body:
            print_debug(0, "no firmware to update")
            return

        if "version" in body['firmware']:
            version = body['firmware']["version"]
            print_debug(0, "updating firmware to {}".format(version))

            customManifest = {
                "firmware": {
                    "URL": "https://{}/manifest.json?sysname={}&wmac={}&ota_slot={}&fwtype={}&target_ver={}&download=true".format(
                        constants.__DEFAULT_SW_HOST,
                        os.uname().sysname,
                        hexlify(machine.unique_id()).decode('ascii'),
                        hex(pycom.ota_slot()),
                        fw_type,
                        version),
                }
            }
            print_debug(5, "Custom Manifest: {}".format(customManifest))
            self.write_firmware(customManifest)
        else:
            fileUrl = '{}://{}/firmware?'.format(constants.__DEFAULT_PYCONFIG_PROTOCOL, constants.__DEFAULT_PYCONFIG_DOMAIN)
            customFirmwares = body['firmware']["customFirmwares"]
            firmwareFilename = ''
            for firmware in customFirmwares:
                print_debug(1, "firmware['firmwareType']={} and os.uname().sysname.lower()={}".format(firmware['firmwareType'], os.uname().sysname.lower()))
                print_debug(1, "firmware={}".format(firmware))
                if (firmware['firmwareType'] == os.uname().sysname.lower()):
                    firmwareFilename = firmware['firmwareFilename']
            targetFileLocation = '{}application_id={}&target_ver={}&target_path={}'.format(
                fileUrl,
                applicationID,
                firmwareFilename,
                '/{}.bin'.format(firmwareFilename)
            )
            customManifest = {
                "firmware": {
                    "URL": targetFileLocation,
                }
            }
            self.write_firmware(customManifest)

    def deploy_new_release(self, body):
        try:
            body = ujson.loads(body.decode())
            print_debug(6, "body {}".format(body))
        except Exception as e:
            print_debug(0, "error while loading body {}".format(e))
            return

        newReleaseID = body["releaseId"]
        applicationID, currentReleaseID = self.get_application_details(body)

        letResp = self.get_update_manifest(applicationID, newReleaseID, currentReleaseID)

        if not letResp:
            return

        fwtype = 'pygate' if hasattr(os.uname(), 'pygate') else 'pybytes'
        fwtype = 'pymesh' if hasattr(os.uname(), 'pymesh') else fwtype
        self.update_files(letResp, applicationID, newReleaseID)
        self.delete_files(letResp)
        self.update_application_config(letResp, applicationID)
        self.update_network_config(letResp)
        self.update_firmware(letResp, applicationID, fw_type=fwtype)
        machine.reset()
