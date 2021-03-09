'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

try:
    from mqtt_core import MQTTCore as mqtt_core
except:
    from _mqtt_core import MQTTCore as mqtt_core

try:
    from pybytes_constants import MQTTConstants as mqttConst
except:
    from _pybytes_constants import MQTTConstants as mqttConst

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

import time


class MQTTClient:

    def __init__(
            self, client_id, server, mqtt_download_topic,
            pybytes_protocol, port=0, user=None, password=None,
            keepalive=0, ssl=False,
            ssl_params={}, reconnect=True
    ):
        if port == 0:
            self.__port = 8883 if ssl else 1883
        else:
            self.__port = port
        self.__reconnect = reconnect
        self.__reconnect_count = 0
        self.__reconnecting = False
        self.__server = server
        self.__mqtt_download_topic = mqtt_download_topic
        self.__pybytes_protocol = pybytes_protocol
        self.__clientId = client_id
        self.__user = user
        self.__password = password
        self.init_mqtt_core()

    def init_mqtt_core(self):
        self.__mqtt = mqtt_core(
            self.__clientId,
            True,
            mqttConst.MQTTv3_1_1,
            receive_timeout=500,
            reconnectMethod=self.reconnect
        )
        self.__mqtt.configEndpoint(self.__server, self.__port)
        self.__mqtt._user = self.__user
        self.__mqtt._password = self.__password

    def getError(self, x):
        """Return a human readable error instead of its code number"""

        # This errors are thrown by connect function,
        # I wouldn't be able to find
        # anywhere a complete list of these error codes
        ERRORS = {
            '-1': 'MQTTClient: Can\'t connect to MQTT server: "{}"'.format(self.__server), # noqa
            '-4': 'MQTTClient: Bad credentials when connecting to MQTT server: "{}"'.format(self.__server), # noqa
            '-9984': 'MQTTClient: Invalid certificate validation when connecting to MQTT server: "{}"'.format(self.__server) # noqa
        }
        message = str(x)
        return ERRORS.get(
            str(x),
            'Unknown error while connecting to MQTT server {}'.format(message)
        )

    def connect(self, clean_session=True):
        i = 0
        while 1:
            try:
                return self.__mqtt.connect()
            except OSError:
                if (not self.__reconnect):
                    raise Exception('Reconnection Disabled.')
                i += 1
                time.sleep(i)

    def set_callback(self, mqtt_client, message):
        self.__pybytes_protocol.__process_recv_message(message)

    def subscribe(self, topic, qos=0):
        self.__mqtt.subscribe(topic, qos, self.set_callback)

    def unsubscribe(self, topic):
        return self.__mqtt.unsubscribe(topic)

    def reconnect(self):
        if self.__reconnecting:
            return

        self.init_mqtt_core()

        while True:
            self.__reconnect_count += 1
            self.__reconnecting = True
            try:
                if not self.__mqtt.connect():
                    time.sleep(self.__reconnect_count)
                    continue
                self.subscribe(self.__mqtt_download_topic)
                self.__reconnect_count = 0
                self.__reconnecting = False
                break
            except OSError:
                time.sleep(self.__reconnect_count)

    def publish(self, topic, msg, retain=False, qos=0, priority=False):
        while 1:
            if not self.__reconnecting:
                try:
                    # Disable retain for publish by now
                    return self.__mqtt.publish(
                        topic,
                        msg,
                        qos,
                        False,
                        priority=priority
                    )
                except OSError as e:
                    print_debug(2, "Error publish", e)

                if (not self.__reconnect):
                    raise Exception('Reconnection Disabled.')
                self.reconnect()
                raise Exception('Error publish.')
            else:
                time.sleep(10)

    def disconnect(self, force=False):
        self.__mqtt.disconnect(force=force)
