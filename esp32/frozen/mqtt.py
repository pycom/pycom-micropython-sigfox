import time
import mqtt_core


class MQTTClient:

    DELAY = 5
    # This errors are thrown by connect function, I wouldn't be able to find
    # anywhere a complete list of these error codes
    ERRORS = {
        '-1': 'MQTTClient: Can\'t connect to MQTT server',
        '-4': 'MQTTClient: Bad credentials'
    }

    def __init__(self, client_id, server, port=0, user=None, password=None, keepalive=0, ssl=False,
                 ssl_params={}, reconnect=True):
        self.__reconnect = reconnect
        self.__mqtt = mqtt_core.MQTTClient(client_id, server, port, user, password, keepalive,
                                           ssl, ssl_params)

    def getError(self, x):
        """Return a human readable error instead of its code number"""
        message = str(x)
        return self.ERRORS.get(str(x), 'Unknown error ' + message)

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
                self.delay(i)

    def set_callback(self, f):
        self.__mqtt.set_callback(f)

    def subscribe(self, topic, qos=0):
        self.__mqtt.subscribe(topic, qos)

    def check_msg(self):
        while 1:
            try:
                return self.__mqtt.check_msg()
            except OSError as e:
                print("Error check_msg", e)

            if (not self.__reconnect):
                raise Exception('Reconnection Disabled.')
            self.reconnect()

    def delay(self, i):
        time.sleep(self.DELAY)

    def reconnect(self):
        print("Reconnecting...")
        i = 0
        while 1:
            try:
                return self.__mqtt.connect(True)
            except OSError as e:
                print('reconnect error', e)
                i += 1
                self.delay(i)

    def publish(self, topic, msg, retain=False, qos=0):
        while 1:
            try:
                return self.__mqtt.publish(topic, msg, retain, qos)
            except OSError as e:
                print("Error publish", e)

            if (not self.__reconnect):
                raise Exception('Reconnection Disabled.')
            self.reconnect()

    def wait_msg(self):
        while 1:
            try:
                return self.__mqtt.wait_msg()
            except OSError as e:
                print("Error wait_msg {}".format(e))

            if (not self.__reconnect):
                raise Exception('Reconnection Disabled.')
            self.reconnect()
