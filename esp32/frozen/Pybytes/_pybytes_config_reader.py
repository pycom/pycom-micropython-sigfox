'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''


class PybytesConfigReader:

    def __init__(self, config):
        self.__pybytes_config = config

    def get_pybytes(self):
        return self.__pybytes_config.get(
            'pybytes', {}
        )

    def send_info(self):
        return self.get_pybytes().get(
            'send_info', True
        )

    def enable_terminal(self):
        return self.get_pybytes().get(
            'enable_terminal', True
        )

    def get_communication_obj(self):
        return self.get_pybytes().get(
            'communication', {}
        )

    def get_communication_type(self):
        return self.get_communication_obj().get(
            'type', 'mqtt'
        ).lower()

    def get_coap(self):
        return self.get_communication_obj().get(
            'servers', {}
        ).get(
            'coap', {}
        )

    def get_coap_host(self):
        return self.get_coap().get(
            'host', None
        )

    def get_coap_port(self):
        return self.get_coap().get(
            'port', None
        )
