'''
Copyright (c) 2019, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

class PybytesConfig:

    def read_config(self, file='/flash/pybytes_config.json'):
        pybytes_config = {}
        force_update = False
        try:
            import pycom
            force_update=pycom.pybytes_force_update()
        except:
            pass

        if not force_update:
            try:
                import json
                f = open(file,'r')
                jfile = f.read()
                f.close()
                try:
                    pybytes_config = json.loads(jfile.strip())
                    pybytes_config['cfg_msg'] = "Pybytes configuration read from {}".format(file)
                except Exception as ex:
                    print("Error reading {} file!\n Exception: {}".format(file, ex))
            except Exception as ex:
                force_update = True
        if force_update:
            try:
                from config import config as pybytes_legacy_config
                if pybytes_legacy_config.get('username') is None or pybytes_legacy_config.get('device_id') is None or pybytes_legacy_config.get('server') is None or pybytes_legacy_config.get('network_preferences') is None:
                    print("This config.py does not look like a Pybytes configuration... skipping...")
                    del pybytes_legacy_config
                    raise ValueError()
                else:
                    pybytes_config.update(pybytes_legacy_config)
                    del pybytes_legacy_config
                    try:
                        import json
                        cf = open(file, 'w')
                        cf.write(json.dumps(pybytes_config))
                        cf.close()
                        pybytes_config['cfg_msg'] = 'Configuration successfully converted from config.py to {}'.format(file)
                        force_update = False
                    except Exception as e:
                        print("Error saving configuration in new json format!")
                        print("Exception: {}".format(e))
            # If this also fails, try configuration written by firmware updater (requires pybytes firmware)
            except:
                force_update = True
        if force_update:
            import pycom
            lora_config = None
            lte_config = None
            sigfox_config = None
            pybytes_autostart = True
            wlan_antenna = 0
            ssl = False
            dump_ca = False
            ssl_params = None

            try:
                extra_preferences = pycom.pybytes_extra_preferences().split(':')
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
                            ssl_params = { "ssl_params": {"ca_certs": extra_preferences[index + 1] }}
                        else:
                            ssl_params = { "ssl_params": {"ca_certs": '/flash/cert/pycom-ca.pem' }}
                    elif extra_preference == "dump_ca":
                        dump_ca = True
                    elif extra_preference == "sigfox":
                        index = extra_preferences.index(extra_preference)
                        if len(extra_preferences[index + 1]) > 0 and extra_preferences[index + 1].isdigit():
                            rcz = int(extra_preferences[index + 1])
                            sigfox_config = { 'sigfox' :
                                                { "RCZ" : rcz }
                                            }
                    elif extra_preference == "lte":
                        index = extra_preferences.index(extra_preference)
                        carrier = 'standard'
                        cid = 1
                        band = None
                        apn = None
                        reset = False
                        type = None

                        if len(extra_preferences[index + 1]) > 0 and extra_preferences[index + 1].lower() != 'none':
                            carrier = extra_preferences[index + 1]
                        if len(extra_preferences[index + 2]) > 0 and extra_preferences[index + 2].isdigit():
                            cid = int(extra_preferences[index + 2])
                        if len(extra_preferences[index + 3]) > 0 and extra_preferences[index + 3].isdigit():
                            band = int(extra_preferences[index + 3])
                        if len(extra_preferences[index + 4]) > 0 and extra_preferences[index + 4].lower() != 'none':
                            apn = extra_preferences[index + 4]
                        if len(extra_preferences[index + 5]) > 0 and extra_preferences[index + 5].lower() == 'true':
                            reset = True
                        if len(extra_preferences[index + 6]) > 0 and extra_preferences[index + 6].lower() != 'none':
                            type = extra_preferences[index + 6]


                        lte_config = {  'lte':
                                        {   'carrier': carrier,
                                            'cid': cid,
                                            'band': band,
                                            'apn': apn,
                                            'reset': reset,
                                            'type': type
                                        }
                                     }
            except:
                pass

            try:
                pybytes_config = {
                    'username': pycom.pybytes_userId(),  # Pybytes username
                    'device_id': pycom.pybytes_device_token(),  # device token
                    'server': pycom.pybytes_mqttServiceAddress(),
                    'network_preferences': pycom.pybytes_network_preferences().split(),  # ordered list, first working network used
                    'wifi': {
                        'ssid': pycom.wifi_ssid(),
                        'password': pycom.wifi_pwd()
                    },
                    'ota_server': {
                        'domain': 'software.pycom.io',
                        'port': 443
                    },
                    'pybytes_autostart': pybytes_autostart,
                    'wlan_antenna': wlan_antenna,
                    'ssl': ssl,
                    'dump_ca': dump_ca
                }
                if lora_config is not None:
                    pybytes_config.update(lora_config)
                if lte_config is not None:
                    pybytes_config.update(lte_config)
                if sigfox_config is not None:
                    pybytes_config.update(sigfox_config)
                if ssl_params is not None:
                    pybytes_config.update(ssl_params)
                if (len(pybytes_config['username']) > 4 and len(pybytes_config['device_id']) >= 36 and len(pybytes_config['server']) > 4):
                    pybytes_config['cfg_msg'] = "Using configuration from config block, written with FW updater"
                    try:
                        import json
                        cf = open('/flash/pybytes_config.json','w')
                        cf.write(json.dumps(pybytes_config))
                        cf.close()
                        pybytes_config['cfg_msg'] = "Configuration successfully converted from config block to pybytes_config.json"
                    except Exception as e:
                        print("Error saving configuration in new json format!")
                        print("Exception: {}".format(e))
            except Exception as e:
                print("No configuration found, auto_start disabled")
                pybytes_config['pybytes_autostart'] = False
                pybytes_config['cfg_msg'] = None

            try:
                pycom.pybytes_force_update(False)
            except:
                pass
        # Check if we have a project specific configuration
        try:
            pf = open('/flash/pybytes_project.json','r')
            pjfile = pf.readall()
            pf.close()
            try:
                import json
                pybytes_project = json.loads(pjfile.strip())
                pybytes_config.update(pybytes_project)
                print("Custom project configuration applied successfully")
            except Exception as ex:
                print("Unable to apply project configuration!")
                print("Exception: {}".format(ex))
        except:
            pass

        if len(pybytes_config.get('username','')) < 4 and len(pybytes_config.get('device_id','')) < 36 and len(pybytes_config.get('server','')) < 4:
            pybytes_config['pybytes_autostart'] = False

        return pybytes_config
