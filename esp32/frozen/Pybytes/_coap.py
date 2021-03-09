'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

from network import WLAN
from network import Coap
# import uselect
# import _thread
# import machine


class COAPClient():
    def __init__(self, target_server, port):
        self.__coap_server = target_server
        self.__coap_server_port = port

        wlan = WLAN(mode=WLAN.STA)
        self.__device_ip = wlan.ifconfig()[0]

        Coap.init(str(wlan.ifconfig()[0]), service_discovery=False)

    def send_coap_message(self, message, method, uri_path, token, include_options=True):
        message_id = Coap.send_request(
            self.__coap_server,
            method,
            uri_port=int(self.__coap_server_port),
            uri_path=uri_path,
            payload=message,
            token=token,
            include_options=include_options
        )
        return message_id
