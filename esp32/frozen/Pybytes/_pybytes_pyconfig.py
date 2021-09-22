'''
Copyright (c) 2020, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''
try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

try:
    import urequest
except:
    import _urequest as urequest


class Pyconfig:
    """
    A class used to represent communication with pybytes
    all pybytes communication should happen with one rest api
    called pyconfig microServices


    """

    def __init__(self, url, device_id):
        self.base_url = url
        self.device_id = device_id

    def get_gateway_config(self):
        target_url = '{}/device/gateway/{}'.format(self.base_url, self.device_id)
        headers = {
            'content-type': 'application/json'
        }
        try:
            request = urequest.get(target_url, headers=headers)
            configurations = request.json()
            request.close()
            print_debug(6, "Response Details: {}".format(configurations))
            return configurations
        except Exception as ex:
            print_debug(1, "error while calling {}!: {}".format(target_url, ex))
