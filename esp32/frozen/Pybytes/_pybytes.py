'''
Copyright (c) 2019, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import os, json, binascii
import time, pycom
from network import WLAN
from machine import Timer

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug


class __PERIODICAL_PIN:
    TYPE_DIGITAL = 0
    TYPE_ANALOG = 1
    TYPE_VIRTUAL = 2

    def __init__(
        self, persistent, pin_number, message_type, message, pin_type
    ):
        self.pin_number = pin_number
        self.message_type = message_type
        self.message = message
        self.pin_type = pin_type


class Pybytes:

    WAKEUP_ALL_LOW = const(0)
    WAKEUP_ANY_HIGH = const(1)

    def __init__(self, config, activation=False, autoconnect=False):
        self.__frozen = globals().get('__name__') == '_pybytes'
        self.__activation = activation
        self.__custom_message_callback = None
        self.__config_updated = False
        self.__pybytes_connection = None
        self.__smart_config = False
        self.__conf = {}

        if not self.__activation:
            self.__conf = config
            pycom.wifi_on_boot(False, True)

            self.__check_dump_ca()
            try:
                from pybytes_connection import PybytesConnection
            except:
                from _pybytes_connection import PybytesConnection
            self.__pybytes_connection = PybytesConnection(self.__conf, self.__recv_message)

            self.start(autoconnect)
            if autoconnect:
                self.print_cfg_msg()
        else:
            if (hasattr(pycom, 'smart_config_on_boot') and pycom.smart_config_on_boot()):
                self.smart_config(True)

    def __check_config(self):
        try:
            print_debug(99, self.__conf)
            return (len(self.__conf.get('username','')) > 4 and len(self.__conf.get('device_id', '')) >= 36 and len(self.__conf.get('server', '')) > 4)
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
                stat = os.stat(ssl_params.get('ca_certs')) # noqa
            except:
                self.dump_ca(ssl_params.get('ca_certs'))

    def connect_wifi(self, reconnect=True, check_interval=0.5):
        self.__check_init()
        return self.__pybytes_connection.connect_wifi(reconnect, check_interval)

    def connect_lte(self, reconnect=True, check_interval=0.5):
        self.__check_init()
        return self.__pybytes_connection.connect_lte(reconnect, check_interval)

    def connect_lora_abp(self, timeout, nanogateway=False):
        self.__check_init()
        return self.__pybytes_connection.connect_lora_abp(timeout, nanogateway)

    def connect_lora_otta(self, timeout=120, nanogateway=False):
        self.__check_init()
        return self.__pybytes_connection.connect_lora_otta(timeout, nanogateway)

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

    def send_signal(self, pin, value):
        self.__check_init()
        self.__pybytes_connection.__pybytes_protocol.send_pybytes_custom_method_values(pin, [value])

    def send_virtual_pin_value(self, persistent, pin, value):
        self.__check_init()
        print("This function is deprecated and will be removed in the future. Use send_signal(signalNumber, value)")
        self.send_signal(pin, value)

    def __periodical_pin_callback(self, periodical_pin):
         self.__check_init()
         if (periodical_pin.pin_type == __PERIODICAL_PIN.TYPE_DIGITAL):
            self.send_digital_pin_value(periodical_pin.persistent, periodical_pin.pin_number, None)
         elif (periodical_pin.pin_type == __PERIODICAL_PIN.TYPE_ANALOG):
             self.send_analog_pin_value(periodical_pin.persistent, periodical_pin.pin_number)

    def register_periodical_digital_pin_publish(self, persistent, pin_number, pull_mode, period):
        self.__check_init()
        self.send_digital_pin_value(pin_number, pull_mode)
        periodical_pin = __PERIODICAL_PIN(pin_number, None, None,
                                          __PERIODICAL_PIN.TYPE_DIGITAL)
        Timer.Alarm(
            self.__periodical_pin_callback, period, arg=periodical_pin,
            periodic=True
        )

    def register_periodical_analog_pin_publish(self, pin_number, period):
        self.__check_init()
        self.send_analog_pin_value(pin_number)
        periodical_pin = __PERIODICAL_PIN(
            pin_number, None, None, __PERIODICAL_PIN.TYPE_ANALOG
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
                    try:
                        from pybytes_connection import PybytesConnection
                    except:
                        from _pybytes_connection import PybytesConnection
                    self.__pybytes_connection = PybytesConnection(self.__conf, self.__recv_message)
                    self.__config_updated = False
            self.__check_init()

            if not self.__conf.get('network_preferences'):
                print("network_preferences are empty, set it up in /flash/pybytes_config.json first") # noqa

            for net in self.__conf['network_preferences']:
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
                    if self.connect_lora_otta(lora_joining_timeout):
                        break
                elif net == 'sigfox':
                    if self.connect_sigfox():
                        break

            import time
            time.sleep(.1)
            if self.is_connected():
                if self.__frozen:
                    print('Pybytes connected successfully (using the built-in pybytes library)') # noqa
                else:
                    print('Pybytes connected successfully (using a local pybytes library)') # noqa

                # SEND DEVICE'S INFORMATION
                self.send_info_message()

                # ENABLE TERMINAL
                self.enable_terminal()
            else:
                print('ERROR! Could not connect to Pybytes!')

        except Exception as ex:
            print("Unable to connect to Pybytes: {}".format(ex))

    def write_config(self, file='/flash/pybytes_config.json', silent=False):
        try:
            import json
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
            import time
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
            self.__conf[key].update(value)
            self.__config_updated = True
            if permanent: self.write_config(silent=silent)
            if reconnect:
                self.reconnect()
        except Exception as ex:
            print('Error updating configuration!')
            print('{}: {}'.format(ex.__name__, ex))


    def read_config(self, file='/flash/pybytes_config.json', reconnect=False):
        try:
            import json
            f = open(file, 'r')
            jfile = f.read()
            f.close()
            try:
                self.__conf = json.loads(jfile.strip())
                self.__config_updated = True
                print("Pybytes configuration read from {}".format(file))
                if reconnect:
                    self.reconnect()
            except Exception as ex:
                print("JSON error in configuration file {}!\n Exception: {}".format(file, ex)) # noqa
        except Exception as ex:
            print("Cannot open file {}\nException: {}".format(file, ex))

    def reconnect(self):
        self.__check_init()
        try:
            self.disconnect()
            import time
            time.sleep(1)
            self.connect()
        except Exception as ex:
            print('Error trying to reconnect... {}'.format(ex))

    def export_config(self, file='/flash/pybytes_config.json'):
        self.__check_init()
        try:
            import json
            f = open(file, 'w')
            f.write(json.dumps(self.__conf))
            f.close()
            print("Pybytes configuration exported to {}".format(file))
        except Exception as e:
            print("Error writing to file {}\nException: {}".format(file, e))

    def enable_ssl(self, ca_file='/flash/cert/pycom-ca.pem', dump_ca = True):
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
        self.set_config('lte', {"carrier": carrier, "cid": cid, "band": band, "apn": apn, "type": type, "reset": reset }, permanent=False)
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
        import machine
        if pins is not None:
            if mode is None or type(mode) != int:
                raise ValueError('You must specify a mode as integer!')
            machine.pin_sleep_wakeup(pins, mode, enable_pull)
        self.disconnect()
        machine.deepsleep(ms)

    def dump_ca(self, ca_file='/flash/cert/pycom-ca.pem'):
        try:
            try:
                from _pybytes_ca import PYBYTES_CA
            except:
                from pybytes_ca import PYBYTES_CA
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

            # START code from the old boot.py
            import machine
            import micropython
            from binascii import hexlify

            wmac = hexlify(machine.unique_id()).decode('ascii')
            print("WMAC: %s" % wmac.upper())
            try:
                print("Firmware: %s\nPybytes: %s" % (os.uname().release, os.uname().pybytes))
            except:
                print("Firmware: %s" % os.uname().release)
            if autoconnect:
                self.connect()



    def activate(self, activation_string):
        self.__smart_config = False
        if self.__pybytes_connection is not None:
            print('Disconnecting current connection!')
            self.__pybytes_connection.disconnect(keep_wifi=True, force=True)
        try:
            jstring = json.loads(binascii.a2b_base64(activation_string))
        except Exception as ex:
            print('Error decoding activation string!')
            print(ex)
        try:
            from pybytes_config import PybytesConfig
        except:
            from _pybytes_config import PybytesConfig
        try:
            self.__conf = PybytesConfig().cli_config(activation_info=jstring)
            if self.__conf is not None:
                self.start()
            else:
                print('Activation failed!')
        except Exception as ex:
            print('Activation failed! Please try again...')
            print_debug(1, ex)

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
                    self.__conf = PybytesConfig().smart_config()
                    self.__smart_config = False
                    self.start()
                except Exception as ex:
                    print('Smart Config failed... restarting!')
                    self.__smart_config = True
                    self.__smart_config_setup()

        def __smart_config_setup(self):
            wl = WLAN(mode=WLAN.STA)
            wl.callback(trigger= WLAN.SMART_CONF_DONE | WLAN.SMART_CONF_TIMEOUT, handler=self.__smart_config_callback)
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
                    self.__conf = PybytesConfig().smart_config()
                    self.__smart_config = False
                    self.start()
            if self.__smart_config:
                wl.smartConfig()
                print("Smart Provisioning started in the background")
                print("See https://docs.pycom.io/smart for details")
