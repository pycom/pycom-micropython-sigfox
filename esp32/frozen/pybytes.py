from pybytes_protocol import PybytesProtocol
from machine import Timer

__DEFAULT_HOST = "mqtt.pycom.io"


class __PERIODICAL_PIN:
    TYPE_DIGITAL = 0
    TYPE_ANALOG = 1
    TYPE_VIRTUAL = 2

    def __init__(self, persistent, pin_number, message_type, message, pin_type):
        self.persistent = persistent
        self.pin_number = pin_number
        self.message_type = message_type
        self.message = message
        self.pin_type = pin_type


class Pybytes:
    def __init__(self, user_name, device_id, host=__DEFAULT_HOST):
        self.__custom_message_callback = None
        self.__pybytes_protocol = PybytesProtocol(host, user_name, device_id, self.__recv_message)
        self.__custom_message_callback = None

    def connect_wifi(self, reconnect=True, check_interval=0.5):
        return self.__pybytes_protocol.connect_wifi(reconnect)

    def connect_lora_abp(self, dev_addr, nwk_swkey, app_swkey, nanogateway=False):
        return self.__pybytes_protocol.connect_lora_abp(dev_addr, nwk_swkey, app_swkey, nanogateway)

    def connect_lora_otta(self, conf, timeout=15, nanogateway=False):
        return self.__pybytes_protocol.connect_lora_otta(conf, timeout, nanogateway)

    def connect_sigfox(self):
        self.__pybytes_protocol.connect_sigfox()

    def disconnect(self):
        self.__pybytes_protocol.disconnect()

    def send_custom_message(self, persistent, message_type, message):
        self.__pybytes_protocol.send_user_message(persistent, message_type, message)

    def set_custom_message_callback(self, callback):
        self.__custom_message_callback = callback

    def send_ping_message(self):
        self.__pybytes_protocol.send_ping_message()

    def send_info_message(self):
        self.__pybytes_protocol.send_info_message()

    def send_digital_pin_value(self, persistent, pin_number, pull_mode):
        self.__pybytes_protocol.send_pybytes_digital_value(persistent, pin_number, pull_mode)

    def send_analog_pin_value(self, persistent, pin):
        self.__pybytes_protocol.send_pybytes_analog_value(persistent, pin)

    def send_virtual_pin_value(self, persistent, pin, value):
        self.__pybytes_protocol.send_pybytes_custom_method_values(persistent, pin, [value])

    def register_periodical_digital_pin_publish(self, persistent, pin_number, pull_mode, period):
        self.send_digital_pin_value(persistent, pin_number, pull_mode)
        periodical_pin = __PERIODICAL_PIN(persistent, pin_number, None, None,
                                          __PERIODICAL_PIN.TYPE_DIGITAL)
        Timer.Alarm(self.__periodical_pin_callback, period, arg=periodical_pin, periodic=True)

    def register_periodical_analog_pin_publish(self, persistent, pin_number, period):
        self.send_analog_pin_value(persistent, pin_number)
        periodical_pin = __PERIODICAL_PIN(persistent, pin_number, None, None,
                                          __PERIODICAL_PIN.TYPE_ANALOG)
        Timer.Alarm(self.__periodical_pin_callback, period, arg=periodical_pin, periodic=True)

    def add_custom_method(self, method_id, method):
        self.__pybytes_protocol.add_custom_method(method_id, method)

    def enable_terminal(self):
        self.__pybytes_protocol.enable_terminal()

    def send_battery_level(self, battery_level):
        self.__pybytes_protocol.set_battery_level(battery_level)
        self.__pybytes_protocol.send_battery_info()

    def send_custom_location(self, pin, x, y):
        self.__pybytes_protocol.send_custom_location(pin, x, y)

    def __periodical_pin_callback(self, periodical_pin):
        if (periodical_pin.pin_type == __PERIODICAL_PIN.TYPE_DIGITAL):
            self.send_digital_pin_value(periodical_pin.persistent, periodical_pin.pin_number, None)
        elif (periodical_pin.pin_type == __PERIODICAL_PIN.TYPE_ANALOG):
            self.send_analog_pin_value(periodical_pin.persistent, periodical_pin.pin_number)

    def __recv_message(self, message):
        if self.__custom_message_callback is not None:
            self.__custom_message_callback(message)

    def __process_protocol_message(self):
        pass
