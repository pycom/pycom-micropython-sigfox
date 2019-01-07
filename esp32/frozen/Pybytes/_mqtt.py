import time
try:
    import mqtt_core
except:
    import _mqtt_core as mqtt_core


class MQTTClient:

    def __init__(self, client_id, server, mqtt_download_topic, port=0, user=None, password=None, keepalive=0, ssl=False,
                 ssl_params={}, reconnect=True):
        self.__reconnect = reconnect
        self.__reconnect_count = 0
        self.__reconnecting = False
        self.__server = server
        self.__mqtt_download_topic = mqtt_download_topic
        self.__mqtt = mqtt_core.MQTTClient(client_id, server, port, user, password, keepalive,
                                           ssl, ssl_params)

    def getError(self, x):
        """Return a human readable error instead of its code number"""

        # This errors are thrown by connect function, I wouldn't be able to find
        # anywhere a complete list of these error codes
        ERRORS = {
            '-1': 'MQTTClient: Can\'t connect to MQTT server: "{}"'.format(self.__server),
            '-4': 'MQTTClient: Bad credentials when connecting to MQTT server: "{}"'.format(self.__server),
            '-9984': 'MQTTClient: Invalid certificate validation when connecting to MQTT server: "{}"'.format(self.__server)
        }
        message = str(x)
        return ERRORS.get(str(x), 'Unknown error while connecting to MQTT server ' + message)

    def connect(self, clean_session=True):
        i = 0
        while 1:
            try:
                return self.__mqtt.connect(clean_session)
            except OSError as e:
                print(self.getError(e))
                if (not self.__reconnect):
                    raise Exception('Reconnection Disabled.')
                i += 1
                time.sleep(i)

    def set_callback(self, f):
        self.__mqtt.set_callback(f)

    def subscribe(self, topic, qos=0):
        self.__mqtt.subscribe(topic, qos)

    def check_msg(self):
        while 1:
            time_before_retry = 10
            if self.__reconnecting == False:
                try:
                    return self.__mqtt.check_msg()
                except OSError as e:
                    print("Error check_msg", e)

                if (not self.__reconnect):
                    raise Exception('Reconnection Disabled.')
                self.reconnect()
                break
            else:
                time.sleep(time_before_retry)


    def reconnect(self):
        if self.__reconnecting:
            return
        while True:
            self.__reconnect_count +=  1
            self.__reconnecting = True
            try:
                self.__mqtt.connect()
                self.subscribe(self.__mqtt_download_topic)
                self.__reconnect_count=0
                print('Reconnected to MQTT server: "{}"'.format(self.__server))
                self.__reconnecting = False
                break
            except OSError:
                time_before_retry = 10 + self.__reconnect_count
                print("Reconnecting failed, will retry in {} seconds".format(time_before_retry))
                time.sleep(time_before_retry)

    def publish(self, topic, msg, retain=False, qos=0):
        while 1:
            if self.__reconnecting == False:
                try:
                    return self.__mqtt.publish(topic, msg, retain, qos)
                except OSError as e:
                    print("Error publish", e)

                if (not self.__reconnect):
                    raise Exception('Reconnection Disabled.')
                self.reconnect()
                break
            else:
                time.sleep(10)

    def wait_msg(self):
        while 1:
            try:
                return self.__mqtt.wait_msg()
            except OSError as e:
                print("Error wait_msg {}".format(e))

            if (not self.__reconnect):
                raise Exception('Reconnection Disabled.')
            self.reconnect()

    def disconnect(self):
        self.__mqtt.disconnect()
