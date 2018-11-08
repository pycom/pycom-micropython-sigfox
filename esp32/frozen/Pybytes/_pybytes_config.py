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
            extra_config = None
            pybytes_autostart = True
            wlan_antenna = 0
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
                        extra_config = {
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
                    'wlan_antenna': wlan_antenna
                }
                if extra_config is not None:
                    pybytes_config.update(extra_config)
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
