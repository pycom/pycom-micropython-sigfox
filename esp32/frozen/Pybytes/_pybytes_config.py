'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import pycom
import time
import os
import json
import binascii
import machine

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug


class PybytesConfig:

    def __init__(self):
        self.__pybytes_config = {}
        self.__pybytes_activation = None
        self.__pybytes_cli_activation = None
        self.__pybytes_sigfox_registration = None
        self.__force_update = False

    def __write_config(self, filename='/flash/pybytes_config.json'):
        print_debug(2, 'Writing configuration to {}'.format(filename))
        try:
            cf = open(filename, 'w')
            cf.write(json.dumps(self.__pybytes_config))
            cf.close()
            return True
        except Exception as e:
            print("Error saving configuration in json format!")
            print("Exception: {}".format(e))
            return False

    def __activation2config(self):
        try:
            self.__pybytes_config = {
                'username': self.__pybytes_activation.json().get('username'),  # Pybytes username
                'device_id': self.__pybytes_activation.json().get('deviceToken'),  # device token
                'server': 'mqtt.{}'.format(constants.__DEFAULT_DOMAIN),
                'network_preferences': ['wifi'],  # ordered list, first working network used
                'wifi': {
                    'ssid': pycom.wifi_ssid_sta(),
                    'password': pycom.wifi_pwd_sta()
                },
                'ota_server': {
                    'domain': constants.__DEFAULT_SW_HOST,
                    'port': 443
                },
                'pybytes_autostart': True,
                'wlan_antenna': 0,
                'ssl': True,
                'dump_ca': True
            }
        except Exception as e:
            print_debug(2, 'Exception in __activation2config\n{}'.format(e))

    def __check_config(self):
        try:
            print_debug(99, self.__pybytes_config)
            return (len(self.__pybytes_config['username']) > 4 and len(self.__pybytes_config['device_id']) >= 36 and len(self.__pybytes_config['server']) > 4)
        except Exception as e:
            print_debug(4, 'Exception in __check_config!\n{}'.format(e))
            return False

    def __check_cb_config(self, config):
        try:
            print_debug(99, self.__pybytes_config)
            return (len(config['userId']) > 4 and len(config['device_id']) >= 36 and len(config['server']) > 4)
        except Exception as e:
            print_debug(4, 'Exception in __check_cb_config!\n{}'.format(e))
            return False

    def __read_activation(self):
        try:
            import urequest
        except:
            import _urequest as urequest

        from uhashlib import sha512
        print('Wifi connection established... activating device!')
        self.__pybytes_activation = None
        data = {"deviceType": os.uname().sysname.lower(), "wirelessMac": binascii.hexlify(machine.unique_id()).upper()}
        try:
            data.update({"activation_hash": binascii.b2a_base64(sha512(data.get("wirelessMac") + '-' + '{}'.format(pycom.wifi_ssid_sta()) + '-' + '{}'.format(
                        pycom.wifi_pwd_sta())).digest()).decode('UTF-8').strip()}
                        )
        except:
            pass
        time.sleep(1)
        try:
            self.__pybytes_activation = urequest.post(
                'https://api.{}/esp-touch/register-device'.format(constants.__DEFAULT_DOMAIN),
                json=data, headers={'content-type': 'application/json'}
            )
            return True
        except Exception as ex:
            if self.__pybytes_activation is not None:
                self.__pybytes_activation.close()
            print('Failed to send activation request!')
            print_debug(2, ex)
            return False

    def __read_cli_activation(self, activation_token):
        try:
            import urequest
        except:
            import _urequest as urequest

        self.__pybytes_cli_activation = None
        data = {"activationToken": activation_token['a'], "deviceMacAddress": binascii.hexlify(machine.unique_id()).upper()}
        time.sleep(1)
        try:
            self.__pybytes_cli_activation = urequest.post(
                'https://api.{}/v2/quick-device-activation'.format(constants.__DEFAULT_DOMAIN), json=data, headers={'content-type': 'application/json'}
            )
        except Exception as ex:
            if self.__pybytes_cli_activation is not None:
                self.__pybytes_cli_activation.close()
            print('Failed to send activation request!')
            print_debug(2, ex)

    def __process_sigfox_registration(self, activation_token):
        try:
            import urequest
        except:
            import _urequest as urequest

        if hasattr(pycom, 'sigfox_info'):
            if not pycom.sigfox_info():
                try:
                    jsigfox = None
                    from network import LoRa
                    data = {
                        "activationToken": activation_token['a'],
                        "wmac": binascii.hexlify(machine.unique_id()).upper(),
                        "smac": binascii.hexlify(LoRa(region=LoRa.EU868).mac())
                    }
                    print_debug(99, 'sigfox_registration: {}'.format(data))
                    try:
                        self.__pybytes_sigfox_registration = urequest.post(
                            'https://api.{}/v2/register-sigfox'.format(constants.__DEFAULT_DOMAIN), json=data, headers={'content-type': 'application/json'}
                        )
                        jsigfox = self.__pybytes_sigfox_registration.json()
                    except:
                        jsigfox = None
                    start_time = time.time()
                    while jsigfox is None and time.time() - start_time < 300:
                        time.sleep(15)
                        try:
                            self.__pybytes_sigfox_registration = urequest.post(
                                'https://api.{}/v2/register-sigfox'.format(constants.__DEFAULT_DOMAIN), json=data, headers={'content-type': 'application/json'}
                            )
                            print_debug(2, '/v2/register-sigfox returned response: {}'.format(self.__pybytes_sigfox_registration.text))
                            jsigfox = self.__pybytes_sigfox_registration.json()
                        except:
                            jsigfox = None
                    if jsigfox is not None:
                        try:
                            self.__pybytes_sigfox_registration.close()
                        except:
                            pass
                        print_debug(99, 'Sigfox regisgtration response:\n{}'.format(jsigfox))
                        return pycom.sigfox_info(
                            id=jsigfox.get('sigfoxId'),
                            pac=jsigfox.get('sigfoxPac'),
                            public_key=jsigfox.get('sigfoxPubKey'),
                            private_key=jsigfox.get('sigfoxPrivKey'),
                            force=True
                        )
                    else:
                        try:
                            self.__pybytes_sigfox_registration.close()
                        except:
                            pass
                        return False
                except Exception as ex:
                    print('Failed to retrieve/program Sigfox credentials!')
                    print_debug(2, ex)
                    return False
        return True

    def __process_cli_activation(self, filename, activation_token):
        try:
            if not self.__pybytes_cli_activation.status_code == 200:
                print_debug(3, 'Activation request returned {} with text: "{}".'.format(
                            self.__pybytes_cli_activation.status_code, self.__pybytes_cli_activation.text)
                            )
            else:
                print_debug(99, 'Activation response:\n{}'.format(self.__pybytes_cli_activation.json()))
                if self.__process_sigfox_registration(activation_token):
                    if self.__process_config(filename, self.__generate_cli_config()):
                        try:
                            self.__pybytes_cli_activation.close()
                        except:
                            pass
                        return self.__pybytes_config
                else:
                    try:
                        self.__pybytes_cli_activation.close()
                    except:
                        pass
                    print('Unable to provision Sigfox! Please try again.')
            return None

        except Exception as e:
            print('Exception during WiFi cli activation!')
            print('{}'.format(e))
            return None

    def __process_activation(self, filename):
        try:
            if not self.__pybytes_activation.status_code == 200:
                print_debug(3, 'Activation request returned {}. Trying again in 10 seconds...'.format(self.__pybytes_activation.status_code))
                self.__pybytes_activation.close()
                return False
            else:
                self.__activation2config()
                self.__pybytes_activation.close()
                print_debug(2, 'Checking and writing configuration in __process_activation')
                if self.__check_config() and self.__write_config(filename):
                    return True
            return False

        except Exception as e:
            print('Exception during WiFi esp-touch activation!\nPlease wait, retrying to activate your device...')
            print('{}'.format(e))
            return False

    def __read_cb_config(self):
        config_block = {}
        try:
            config_block = {
                'userId': pycom.pybytes_userId(),
                'device_token': pycom.pybytes_device_token(),
                'mqttServiceAddress': pycom.pybytes_mqttServiceAddress(),
                'network_preferences': pycom.pybytes_network_preferences().split(),
                'wifi_ssid': pycom.wifi_ssid_sta() if hasattr(pycom, 'wifi_ssid_sta') else pycom.wifi_ssid(),
                'wifi_pwd': pycom.wifi_pwd_sta() if hasattr(pycom, 'wifi_pwd_sta') else pycom.wifi_pwd(),
                'extra_preferences': pycom.pybytes_extra_preferences(),
                'carrier': pycom.pybytes_lte_config()[0],
                'apn': pycom.pybytes_lte_config()[1],
                'cid': pycom.pybytes_lte_config()[2],
                'band': pycom.pybytes_lte_config()[3],
                'protocol': pycom.pybytes_lte_config()[4],
                'reset': pycom.pybytes_lte_config()[5]
            }
        except:
            pass
        return config_block

    def __generate_cli_config(self):
        pybytes_config = self.__pybytes_cli_activation.json()
        cli_config = {
            'userId': pybytes_config.get('userId'),
            'device_token': pybytes_config.get('deviceToken'),
            'mqttServiceAddress': pybytes_config.get('mqttServiceAddress'),
            'network_preferences': pybytes_config.get('network_preferences'),
            'wifi_ssid': '',
            'wifi_pwd': '',
        }
        try:
            cli_config.update({'wifi_ssid': pybytes_config.get('wifi').get('ssid')})
        except:
            print_debug(3, '__generate_cli_config: config does not contain wifi_ssid')
        try:
            cli_config.update({'wifi_pwd': pybytes_config.get('wifi').get('password')})
        except:
            print_debug(3, '__generate_cli_config: config does not contain wifi_password')
        try:
            cli_config.update({
                'carrier': pybytes_config.get('lte').get('carrier').lower(),
                'apn': pybytes_config.get('lte').get('apn'),
                'cid': pybytes_config.get('lte').get('cid'),
                'band': pybytes_config.get('lte').get('band'),
                'reset': pybytes_config.get('lte').get('reset'),
                'protocol': pybytes_config.get('lte').get('protocol')
            })
        except:
            print_debug(3, '__generate_cli_config: config does not contain LTE configuration')
        try:
            cli_config.update({'extra_preferences': pybytes_config.get('extra_preferences', '')})
        except:
            print_debug(3, '__generate_cli_config: config does not contain extra_preferences')
        return cli_config

    def __process_config(self, filename, configuration):
        print_debug(99, 'process_config={}'.format(configuration))
        lora_config = None
        lte_config = None
        sigfox_config = None
        pybytes_autostart = True
        wlan_antenna = 0
        ssl = False
        dump_ca = False
        ssl_params = None

        try:
            extra_preferences = configuration['extra_preferences'].split(':')
            for extra_preference in extra_preferences:
                if extra_preference.startswith('lora'):
                    extra_preferences_type = extra_preference.split('_')
                    if extra_preferences_type[0] == 'lora':
                        if extra_preferences_type[1] == 'otaa':
                            f1 = 'app_device_eui'
                            f2 = 'app_eui'
                            f3 = 'app_key'
                        if extra_preferences_type[1] == 'abp':
                            f1 = 'dev_addr'
                            f2 = 'nwk_skey'
                            f3 = 'app_skey'
                    index = extra_preferences.index(extra_preference)
                    lora_config = {
                        extra_preferences_type[0]: {
                            extra_preferences_type[1]: {
                                f1: extra_preferences[index + 1],
                                f2: extra_preferences[index + 2],
                                f3: extra_preferences[index + 3]
                            }
                        }
                    }
                elif extra_preference == "no_start":
                    pybytes_autostart = False
                elif extra_preference == "ext_ant":
                    wlan_antenna = 1
                elif extra_preference == "ssl":
                    ssl = True
                elif extra_preference == "ca_certs":
                    index = extra_preferences.index(extra_preference)
                    if len(extra_preferences[index + 1]) > 0:
                        ssl_params = {"ssl_params": {"ca_certs": extra_preferences[index + 1]}}
                    else:
                        ssl_params = {"ssl_params": {"ca_certs": '/flash/cert/pycom-ca.pem'}}
                elif extra_preference == "dump_ca":
                    dump_ca = True
                elif extra_preference == "sigfox":
                    index = extra_preferences.index(extra_preference)
                    if len(extra_preferences[index + 1]) > 0 and extra_preferences[index + 1].isdigit():
                        rcz = int(extra_preferences[index + 1])
                        sigfox_config = {
                            'sigfox': {
                                "RCZ": rcz
                            }
                        }

        except Exception as e:
            print_debug(2, 'Exception __process_config[extra]\n{}'.format(e))

        try:
            lte_config = {
                'lte': {
                    'carrier': configuration.get('carrier'),
                    'cid': configuration.get('cid'),
                    'band': configuration.get('band'),
                    'apn': configuration.get('apn'),
                    'reset': configuration.get('reset', 'false') == 'true',
                    'type': configuration.get('protocol')
                }
            }
        except Exception as e:
            print_debug(2, 'Exception __process_config[lte]\n{}'.format(e))

        try:
            self.__pybytes_config = {
                'username': configuration['userId'],  # Pybytes username
                'device_id': configuration['device_token'],  # device token
                'server': configuration['mqttServiceAddress'],
                'network_preferences': configuration['network_preferences'] \
                    if type(configuration['network_preferences']) is list \
                    else configuration['network_preferences'].split(),  # noqa # ordered list, first working network used
                'wifi': {
                    'ssid': configuration['wifi_ssid'],
                    'password': configuration['wifi_pwd']
                },
                'ota_server': {
                    'domain': constants.__DEFAULT_SW_HOST,
                    'port': 443
                },
                'pybytes_autostart': pybytes_autostart,
                'wlan_antenna': wlan_antenna,
                'ssl': ssl,
                'dump_ca': dump_ca
            }
            if lora_config is not None:
                self.__pybytes_config.update(lora_config)
            if lte_config is not None:
                self.__pybytes_config.update(lte_config)
            if sigfox_config is not None:
                self.__pybytes_config.update(sigfox_config)
            if ssl_params is not None:
                self.__pybytes_config.update(ssl_params)
            print_debug(2, 'Checking and writing configuration in __process_config')
            if (len(self.__pybytes_config['username']) > 4 and len(self.__pybytes_config['device_id']) >= 36 and len(self.__pybytes_config['server']) > 4) and self.__write_config(filename): # noqa
                self.__pybytes_config['cfg_msg'] = "Configuration successfully converted to pybytes_config.json"
                return True
            return False
        except Exception as e:
            print_debug(2, 'Exception __process_config[generic]\n{}'.format(e))
            return False

    def __convert_legacy_config(self, filename):
        try:
            from config import config as pybytes_legacy_config
            if pybytes_legacy_config.get('username') is None or pybytes_legacy_config.get('device_id') is None or pybytes_legacy_config.get('server') is None or pybytes_legacy_config.get('network_preferences') is None: # noqa
                print("This config.py does not look like a Pybytes configuration... skipping...")
                del pybytes_legacy_config
                raise ValueError()
            else:
                self.__pybytes_config.update(pybytes_legacy_config)
                del pybytes_legacy_config
                print_debug(2, 'Checking and writing configuration in __convert_legacy_config')
                if self.__write_config(filename):
                    self.__pybytes_config['cfg_msg'] = 'Configuration successfully converted from config.py to {}'.format(filename)
                    self.__force_update = False
                else:
                    print("Error saving configuration in new json format!")
        except:
            self.__force_update = True

    def smart_config(self, filename='/flash/pybytes_config.json'):
        if self.__read_activation():
            print_debug(2, 'Activation request sent... checking result')
        while not self.__process_activation(filename):
            time.sleep(10)
        return self.__pybytes_config

    def cli_config(self, pybytes_connection, filename='/flash/pybytes_config.json', activation_info=None):
        print_debug(99, activation_info)
        if 's' in activation_info:
            print_debug(3, 'performing Wi-Fi activation')
            self.__cli_activation_over_wifi(activation_info)
            print('Wifi connection established... activating device!')
        if 'apn' in activation_info:
            print_debug(3, 'performing LTE activation')
            pybytes_connection.connect_lte(activation_info=activation_info, start_mqtt=False)

        self.__read_cli_activation(activation_info)
        return self.__process_cli_activation(filename, activation_info)

    def __cli_activation_over_wifi(self, activation_info):
        print('Please wait while we try to connect to {}'.format(activation_info.get('s')))
        from network import WLAN
        wlan = WLAN(mode=WLAN.STA)
        attempt = 0
        known_nets = [((activation_info['s'], activation_info['p']))] # noqa

        print_debug(3, 'WLAN connected? {}'.format(wlan.isconnected()))
        while not wlan.isconnected() and attempt < 10:
            attempt += 1
            print_debug(3, "Wifi connection attempt: {}".format(attempt))
            print_debug(3, 'WLAN connected? {}'.format(wlan.isconnected()))
            available_nets = None
            while available_nets is None:
                try:
                    available_nets = wlan.scan()
                    for x in available_nets:
                        print_debug(5, x)
                    time.sleep(3)
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
                    wlan.connect(net_to_use, timeout=10000)
                else:
                    wlan.connect(net_to_use, (sec, pwd), timeout=10000)
                start_time = time.time()
                while not wlan.isconnected():
                    if time.time() - start_time > 300:
                        raise TimeoutError('Timeout trying to connect via WiFi')  # noqa: F821
                    time.sleep(0.1)
            except Exception as e:
                if str(e) == "list index out of range" and attempt == 3:
                    print("Please review Wifi SSID and password!")
                    wlan.deinit()
                    return None
                elif attempt == 3:
                    print("Error connecting using WIFI: %s" % e)
                    return None

    def read_config(self, filename='/flash/pybytes_config.json'):
        try:
            self.__force_update = pycom.pybytes_force_update()
        except:
            pass

        if not self.__force_update:
            try:
                f = open(filename, 'r')
                jfile = f.read()
                f.close()
                try:
                    self.__pybytes_config = json.loads(jfile.strip())
                    self.__pybytes_config['cfg_msg'] = "Pybytes configuration read from {}".format(filename)
                except Exception as ex:
                    print("Error reading {} file!\n Exception: {}".format(filename, ex))
            except Exception as ex:
                self.__force_update = True

        if self.__force_update:
            if not self.__process_config(filename, self.__read_cb_config()):
                self.__pybytes_config['pybytes_autostart'] = False
                self.__pybytes_config['cfg_msg'] = None
            try:
                pycom.pybytes_force_update(False)
            except:
                pass

        # Check if we have a project specific configuration
        try:
            pf = open('/flash/pybytes_project.json', 'r')
            pjfile = pf.read()
            pf.close()
            try:
                pybytes_project = json.loads(pjfile.strip())
                self.__pybytes_config.update(pybytes_project)
                print("Custom project configuration applied successfully")
                if self.__pybytes_config.get('cfg_msg') is None:
                    self.__pybytes_config['cfg_msg'] = 'Using configuration from /flash/pybytes_project.json'
            except Exception as ex:
                print("Unable to apply project configuration!")
                print("Exception: {}".format(ex))
        except:
            pass
        if self.__pybytes_config.get('pybytes_autostart', False):
            self.__pybytes_config['pybytes_autostart'] = self.__check_config()

        return self.__pybytes_config
