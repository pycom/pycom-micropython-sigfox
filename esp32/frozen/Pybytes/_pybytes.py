'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import os
import json
import time
import pycom
import sys
from network import WLAN
from binascii import hexlify, a2b_base64
from machine import Timer, deepsleep, pin_sleep_wakeup, unique_id

try:
    from periodical_pin import PeriodicalPin
except:
    from _periodical_pin import PeriodicalPin

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

try:
    from pybytes_config_reader import PybytesConfigReader
except:
    from _pybytes_config_reader import PybytesConfigReader


class Pybytes:

    WAKEUP_ALL_LOW = const(0)   # noqa: F821
    WAKEUP_ANY_HIGH = const(1)  # noqa: F821

    def __init__(self, config, activation=False, autoconnect=False):
        self.__frozen = globals().get('__name__') == '_pybytes'
        self.__activation = activation
        self.__custom_message_callback = None
        self.__config_updated = False
        self.__pybytes_connection = None
        self.__smart_config = False
        self.__conf = {}
        self.__pymesh = None

        if not self.__activation:
            self.__conf = config
            self.__conf_reader = PybytesConfigReader(config)
            pycom.wifi_on_boot(False, True)

            self.__check_dump_ca()
            self.__create_pybytes_connection(self.__conf)
            self.start(autoconnect)
            if autoconnect:
                self.print_cfg_msg()
        else:
            if (hasattr(pycom, 'smart_config_on_boot') and pycom.smart_config_on_boot()):
                self.smart_config(True)

    def __create_pybytes_connection(self, conf):
        try:
            from pybytes_connection import PybytesConnection
        except:
            from _pybytes_connection import PybytesConnection

        self.__pybytes_connection = PybytesConnection(conf, self.__recv_message)

    def __check_config(self):
        try:
            print_debug(99, self.__conf)
            return (len(self.__conf.get('username', '')) > 4 and len(self.__conf.get('device_id', '')) >= 36 and len(self.__conf.get('server', '')) > 4)
        except Exception as e:
            print_debug(4, 'Exception in __check_config!\n{}'.format(e))
            return False

    def __check_init(self):
        if self.__pybytes_connection is None:
            raise OSError('Pybytes has not been initialized!')

    def __check_dump_ca(self):
        ssl_params = self.__conf.get(
            'ssl_params',
            {'ca_certs': '/flash/cert/pycom-ca.pem'}
        )
        print_debug(4, ' ssl_params={} '.format(ssl_params))
        if self.__conf.get('dump_ca', False):
            try:
                os.stat(ssl_params.get('ca_certs'))
            except:
                self.dump_ca(ssl_params.get('ca_certs'))

    def connect_wifi(self, reconnect=True, check_interval=0.5):
        self.__check_init()
        if self.__pybytes_connection.connect_wifi(reconnect, check_interval):
            self.__pybytes_connection.communication_protocol('wifi')
            return True
        return False

    def connect_lte(self):
        self.__check_init()
        if self.__pybytes_connection.connect_lte():
            self.__pybytes_connection.communication_protocol('lte')
            return True
        return False

    def connect_lora_abp(self, timeout, nanogateway=False):
        self.__check_init()
        return self.__pybytes_connection.connect_lora_abp(timeout, nanogateway)

    def connect_lora_otaa(self, timeout=120, nanogateway=False):
        self.__check_init()
        return self.__pybytes_connection.connect_lora_otaa(timeout, nanogateway)

    def connect_sigfox(self):
        self.__check_init()
        return self.__pybytes_connection.connect_sigfox()

    def disconnect(self, force=True):
        self.__check_init()
        self.__pybytes_connection.disconnect(force=force)

    def send_custom_message(self, persistent, message_type, message):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_user_message(message_type, message)

    def set_custom_message_callback(self, callback):
        self.__check_init()
        self.__custom_message_callback = callback

    def send_ping_message(self):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_ping_message()

    def send_info_message(self):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_info_message()

    def send_scan_info_message(self):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_scan_info_message(None)

    def send_digital_pin_value(self, persistent, pin_number, pull_mode):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_pybytes_digital_value(pin_number, pull_mode)

    def send_analog_pin_value(self, persistent, pin):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_pybytes_analog_value(pin)

    def send_node_signal(self, signal_number, value, token):
        self.__check_init()
        topic = 'br/{}'.format(token)
        self.__pybytes_connection.__pybytes_protocol.send_pybytes_custom_method_values(signal_number, [value], topic)

    def send_signal(self, signal_number, value):
        self.__check_init()
        if self.__pymesh:
            self.__pymesh.unpack_pymesh_message(signal_number, value)
        else:
            self.__pybytes_connection.__pybytes_protocol.send_pybytes_custom_method_values(signal_number, [value])

    def __periodical_pin_callback(self, periodical_pin):
        self.__check_init()
        if (periodical_pin.pin_type == PeriodicalPin.TYPE_DIGITAL):
            self.send_digital_pin_value(
                periodical_pin.persistent, periodical_pin.pin_number, None
            )
        elif (periodical_pin.pin_type == PeriodicalPin.TYPE_ANALOG):
            self.send_analog_pin_value(
                periodical_pin.persistent, periodical_pin.pin_number
            )

    def register_periodical_digital_pin_publish(self, persistent, pin_number, pull_mode, period):
        self.__check_init()
        self.send_digital_pin_value(pin_number, pull_mode)
        periodical_pin = PeriodicalPin(pin_number, None, None, PeriodicalPin.TYPE_DIGITAL)
        Timer.Alarm(
            self.__periodical_pin_callback, period, arg=periodical_pin,
            periodic=True
        )

    def register_periodical_analog_pin_publish(self, pin_number, period):
        self.__check_init()
        self.send_analog_pin_value(pin_number)
        periodical_pin = PeriodicalPin(
            pin_number, None, None, PeriodicalPin.TYPE_ANALOG
        )
        Timer.Alarm(
            self.__periodical_pin_callback, period, arg=periodical_pin,
            periodic=True
        )

    def add_custom_method(self, method_id, method):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.add_custom_method(method_id, method)

    def enable_terminal(self):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.enable_terminal()

    def send_battery_level(self, battery_level):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.set_battery_level(battery_level)
        self.__pybytes_connection.__pybytes_protocol.send_battery_info()

    def send_custom_location(self, pin, x, y):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_custom_location(pin, x, y)

    def __recv_message(self, message):
        self.__check_init()
        if self.__custom_message_callback is not None:
            self.__custom_message_callback(message)

    def __process_protocol_message(self):
        pass

    def is_connected(self):
        return self.isconnected()

    def isconnected(self):
        try:
            return self.__pybytes_connection.is_connected()
        except:
            return False

    def connect(self):
        try:
            lora_joining_timeout = 120  # seconds to wait for LoRa joining
            if self.__config_updated:
                if self.__check_config():
                    self.__create_pybytes_connection(self.__conf)
                    self.__config_updated = False
            self.__check_init()

            if not self.__conf.get('network_preferences'):
                print("network_preferences are empty, set it up in /flash/pybytes_config.json first")

            for net in self.__conf['network_preferences']:
                print_debug(3, 'Attempting to connect with network {}'.format(net))
                if net == 'lte' or net == 'nbiot':
                    if self.connect_lte():
                        break
                elif net == 'wifi':
                    if self.connect_wifi():
                        break
                elif net == 'lora_abp':
                    if self.connect_lora_abp(lora_joining_timeout):
                        break
                elif net == 'lora_otaa':
                    if self.connect_lora_otaa(lora_joining_timeout):
                        break
                elif net == 'sigfox':
                    if self.connect_sigfox():
                        break

            time.sleep(.1)
            if self.is_connected():
                if self.__frozen:
                    print('Pybytes connected successfully (using the built-in pybytes library)')
                else:
                    print('Pybytes connected successfully (using a local pybytes library)')

                # SEND DEVICE'S INFORMATION
                if self.__conf_reader.send_info():
                    self.send_info_message()

                # ENABLE TERMINAL
                if self.__conf_reader.enable_terminal():
                    self.enable_terminal()

                # CHECK PYMESH FIRMWARE VERSION
                if hasattr(os.uname(), 'pymesh'):
                    try:
                        from pybytes_pymesh_config import PybytesPymeshConfig
                    except:
                        from _pybytes_pymesh_config import PybytesPymeshConfig
                    self.__pymesh = PybytesPymeshConfig(self)
                    self.__pymesh.pymesh_init()
            else:
                print('ERROR! Could not connect to Pybytes!')

        except Exception as ex:
            print("Unable to connect to Pybytes: {}".format(ex))

    def write_config(self, file='/flash/pybytes_config.json', silent=False):
        try:
            f = open(file, 'w')
            f.write(json.dumps(self.__conf))
            f.close()
            if not silent:
                print("Pybytes configuration written to {}".format(file))
        except Exception as e:
            if not silent:
                print("Exception: {}".format(e))

    def print_cfg_msg(self):
        if self.__conf.get('cfg_msg') is not None:
            time.sleep(.1)
            print(self.__conf['cfg_msg'])
            time.sleep(.1)

    def print_config(self):
        for key in self.__conf.keys():
            print('{} = {}'.format(key, self.__conf.get(key)))

    def get_config(self, key=None):
        if key is None:
            return self.__conf
        else:
            return self.__conf.get(key)

    def set_config(
            self,
            key=None,
            value=None,
            permanent=True,
            silent=False,
            reconnect=False
    ):
        if key is None and value is not None:
            self.__conf = value
            self.__conf_reader = PybytesConfigReader(value)
        elif key is not None:
            self.__conf[key] = value
        else:
            raise ValueError('You need to either specify a key or a value!')
        self.__config_updated = True
        if permanent:
            self.write_config(silent=silent)
        if reconnect:
            self.reconnect()

    def update_config(self, key, value=None, permanent=True, silent=False, reconnect=False):
        try:
            if isinstance(self.__conf[key], dict):
                self.__conf[key].update(value)
            elif type(self.__conf[key]) is list:
                # set new list
                self.__conf[key] = []
                values = list(value.split(","))
                # removes leading whitespaces on array itens
                values = [item.strip() for item in values]
                self.__conf[key] = values
            else:
                self.__conf[key] = value
            self.__config_updated = True
            if permanent:
                self.write_config(silent=silent)
            if reconnect:
                self.reconnect()
        except Exception as ex:
            print('Error updating configuration!')
            sys.print_exception(ex)

    def read_config(self, file='/flash/pybytes_config.json', reconnect=False):
        try:
            f = open(file, 'r')
            jfile = f.read()
            f.close()
            try:
                config_from_file = json.loads(jfile.strip())
                self.__conf = config_from_file
                self.__conf_reader = PybytesConfigReader(config_from_file)
                self.__config_updated = True
                print("Pybytes configuration read from {}".format(file))
                if reconnect:
                    self.reconnect()
            except Exception as ex:
                print("JSON error in configuration file {}!\n Exception: {}".format(file, ex))
        except Exception as ex:
            print("Cannot open file {}\nException: {}".format(file, ex))

    def reconnect(self):
        self.__check_init()
        try:
            self.disconnect()
            time.sleep(1)
            self.connect()
        except Exception as ex:
            print('Error trying to reconnect... {}'.format(ex))

    def export_config(self, file='/flash/pybytes_config.json'):
        self.__check_init()
        try:
            f = open(file, 'w')
            f.write(json.dumps(self.__conf))
            f.close()
            print("Pybytes configuration exported to {}".format(file))
        except Exception as e:
            print("Error writing to file {}\nException: {}".format(file, e))

    def enable_ssl(self, ca_file='/flash/cert/pycom-ca.pem', dump_ca=True):
        self.__check_init()
        self.set_config('dump_ca', dump_ca, permanent=False)
        if ca_file is not None:
            self.set_config(
                'ssl_params',
                {'ca_certs': ca_file},
                permanent=False
            )
        self.set_config('ssl', True, silent=True)
        print('Please reset your module to apply the new settings.')

    def enable_lte(self, carrier=None, cid=None, band=None, apn=None, type=None, reset=None, fallback=False):
        nwpref = None
        self.__check_init()
        self.set_config('lte', {"carrier": carrier, "cid": cid, "band": band, "apn": apn, "type": type, "reset": reset}, permanent=False)
        if fallback:
            nwpref = self.__conf.get('network_preferences', [])
            nwpref.extend(['lte'])
        else:
            nwpref = ['lte']
            nwpref.extend(self.__conf.get('network_preferences', []))
        print_debug(1, 'nwpref: {}'.format(nwpref))
        if nwpref is not None:
            self.set_config('network_preferences', nwpref, reconnect=True)

    def deepsleep(self, ms, pins=None, mode=None, enable_pull=None):
        self.__check_init()
        if pins is not None:
            if mode is None or type(mode) != int:
                raise ValueError('You must specify a mode as integer!')
            pin_sleep_wakeup(pins, mode, enable_pull)
        self.disconnect()
        deepsleep(ms)

    def dump_ca(self, ca_file='/flash/cert/pycom-ca.pem'):
        try:
            from _pybytes_ca import PYBYTES_CA
        except:
            from pybytes_ca import PYBYTES_CA

        try:
            f = open(ca_file, 'w')
            f.write(PYBYTES_CA)
            f.close()
            print("Successfully created {}".format(ca_file))
        except Exception as e:
            print("Error creating {}\nException: {}".format(file, e))

    def start(self, autoconnect=True):
        if self.__conf is not None:
            self.__check_dump_ca()
            self.__config_updated = True

            print("WMAC: {}".format(hexlify(unique_id()).decode('ascii').upper()))
            if hasattr(os.uname(), 'pybytes'):
                print("Firmware: %s\nPybytes: %s" % (os.uname().release, os.uname().pybytes))
            else:
                print("Firmware: %s" % os.uname().release)
            if autoconnect:
                self.connect()

    def activate(self, activation_string):
        self.__smart_config = False
        if self.__pybytes_connection is not None:
            print('Disconnecting current connection!')
            self.__pybytes_connection.disconnect(keep_wifi=True, force=True)
        try:
            jstring = json.loads(a2b_base64(activation_string))
        except Exception as ex:
            print('Error decoding activation string!')
            print(ex)
        try:
            from pybytes_config import PybytesConfig
        except:
            from _pybytes_config import PybytesConfig
        try:
            self.__create_pybytes_connection(None)
            conf_from_pybytes_conf = PybytesConfig().cli_config(activation_info=jstring, pybytes_connection=self.__pybytes_connection)
            self.__conf = conf_from_pybytes_conf
            self.__conf_reader = PybytesConfigReader(conf_from_pybytes_conf)
            if self.__conf is not None:
                self.start()
            else:
                print('Activation failed!')
        except Exception as ex:
            print('Activation failed! Please try again...')
            sys.print_exception(ex)

    if hasattr(pycom, 'smart_config_on_boot'):
        def smart_config(self, status=None, reset_ap=False):
            if status is None:
                return self.__smart_config
            if status:
                if reset_ap:
                    pycom.wifi_ssid_sta(None)
                    pycom.wifi_pwd_sta(None)
                if self.__pybytes_connection is not None:
                    if self.isconnected():
                        print('Disconnecting current connection!')
                        self.disconnect()
                    self.__pybytes_connection = None
                if not self.__smart_config:
                    pycom.smart_config_on_boot(True)
                    self.__smart_config = True
                    self.__smart_config_setup()
            else:
                self.__smart_config = False
                pycom.smart_config_on_boot(False)

        def __smart_config_callback(self, wl):
            event = wl.events()
            print_debug(99, 'Received {} from wl.events()'.format(event))
            if event == WLAN.SMART_CONF_TIMEOUT and self.__smart_config and (not wl.isconnected()):
                print_debug(99, 'Restarting smartConfig()')
                wl.smartConfig()
            if event == WLAN.SMART_CONF_DONE and self.__smart_config:
                try:
                    from pybytes_config import PybytesConfig
                except:
                    from _pybytes_config import PybytesConfig
                print_debug(99, 'smartConfig done... activating')
                try:
                    conf_smart = PybytesConfig().smart_config()
                    self.__conf = conf_smart
                    self.__conf_reader = PybytesConfigReader(conf_smart)
                    self.__smart_config = False
                    self.start()
                except Exception as ex:
                    print_debug(99, ex)
                    print('Smart Config failed... restarting!')
                    self.__smart_config = True
                    self.__smart_config_setup()

        def __smart_config_setup(self):
            wl = WLAN(mode=WLAN.STA)
            wl.callback(trigger=WLAN.SMART_CONF_DONE | WLAN.SMART_CONF_TIMEOUT, handler=self.__smart_config_callback)
            if pycom.wifi_ssid_sta() is not None and len(pycom.wifi_ssid_sta()) > 0:
                print('Trying previous AP details for 60 seconds...')
                pycom.wifi_on_boot(True, True)
                try:
                    start_time = time.time()
                    while not wl.isconnected() and (time.time() - start_time < 60) and self.__smart_config:
                        time.sleep(1)
                except:
                    pass
                if wl.isconnected() and self.__smart_config:
                    try:
                        from pybytes_config import PybytesConfig
                    except:
                        from _pybytes_config import PybytesConfig
                    conf_smart = PybytesConfig().smart_config()
                    self.__conf = conf_smart
                    self.__conf_reader = PybytesConfigReader(conf_smart)
                    self.__smart_config = False
                    self.start()
            if self.__smart_config:
                wl.smartConfig()
                print("Smart Provisioning started in the background")
                print("See https://docs.pycom.io/smart for details")
