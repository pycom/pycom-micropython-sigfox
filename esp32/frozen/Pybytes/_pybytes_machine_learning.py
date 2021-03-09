'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import math
import json

try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

try:
    import urequest
except:
    import _urequest as urequest

try:
    from pybytes_constants import constants
except:
    from _pybytes_constants import constants

import pycom

try:
    from LIS2HH12 import *
except:
    print_debug(5, "LIS2HH12 not imported")

# 20 seconds, max window in time for recording
MAX_LEN_MSEC = const(20000)

# 350Hz, max frequency
MAX_FREQ_HZ = const(350)


class MlFeatures():
    def __init__(self, pybytes_protocol=None, parameters=None):
        if parameters is not None:
            self.__length = parameters["length"]
            self.__label = parameters["label"]
            self.__sampleName = parameters["sampleName"]
            self.__type = parameters["type"]
            self.__device = parameters["device"]
            self.__model = parameters["model"]
            self.__mlSample = parameters["mlSample"]
            self.__frequency = parameters["frequency"]
            self.__pybytes_protocol = pybytes_protocol
            self.__data = []

    def _debug_hack(self, pybytes):
        self.__pybytes = pybytes

    def start_sampling(self, pin):
        # here define the required libraries
        try:
            from pysense import Pysense
        except:
            print_debug(5, "pysense not imported")

        try:
            from pytrack import Pytrack
        except:
            print_debug(5, "pytrack not imported")

        lib = False
        try:
            py = Pysense()
            lib = True
        except NameError:
            print_debug(5, "Pysense not defined")

        if not lib:
            try:
                py = Pytrack()
            except NameError:
                print_debug(5, "Check if Pysense/Pytrack libraries are loaded")
                return

        try:
            li = LIS2HH12(py)
        except NameError:
            print_debug(5, "LIS2HH12 library are not loaded")
            return
        li.set_odr(ODR_400_HZ)

        # make the max record length to 20 seconds
        self.__length = min(MAX_LEN_MSEC, self.__length)

        # make the max frequency to 350Hz
        self.__frequency = min(MAX_FREQ_HZ, self.__frequency)

        # compute time interval between 2 consecutive samples
        delta_t_us = int(1000000.0 / self.__frequency)
        # compute the number of samples to be acquisition
        samples_num = math.ceil(self.__length * self.__frequency / 1000) + 1

        try:
            pycom.heartbeat(False)
            pycom.rgbled(0x7f7f00)
        except:
            pass
        time.sleep(0.5)

        self.__data = []
        index = 0
        print("Start acquisition data for %d msec, freq %d Hz" % (self.__length, self.__frequency))

        next_ts = time.ticks_us()
        ts_orig = next_ts
        while True:
            while time.ticks_diff(next_ts, time.ticks_us()) > 0:
                pass
            acc = li.acceleration()
            ts = next_ts
            self.__data.append((ts - ts_orig, acc))
            next_ts = ts + delta_t_us
            index += 1
            if index >= samples_num:
                break  # done

        print("Done acquisition %d samples, real freq %.1f Hz" % (index, index / (self.__length / 1000)))
        self._parse_data(pin)

    def _send_data(self, data, pin, acc, ts):
        if self.__pybytes_protocol is not None:
            if self.__type == 2:
                self.__label = self.__sampleName
            self.__pybytes_protocol.send_pybytes_custom_method_values(pin, [
                data],
                'sample/{}/{}/{}/{}/{}'.format(self.__label, self.__type, self.__model, self.__device, self.__mlSample))
        else:
            self.__pybytes.send_signal(pin & 0xFF, str((int(ts / 1000), acc)))

    def _parse_data(self, pin):
        print("_parse_data, %d samples" % len(self.__data))
        try:
            pycom.rgbled(0x8d05f5)
        except:
            pass
        data = ['{"data": "ml"}']
        for (ts, acc) in self.__data:
            data.append('{' + '"data": [{},{},{}], "ms": {}'.format(acc[0], acc[1], acc[2], int(ts / 1000)) + '}')
            if len(data) > 25:
                self._send_data(data, pin, acc, ts)
                data = ['{"data": "ml"}']
        self._send_data(data, pin, acc, ts)
        try:
            pycom.heartbeat(True)
        except:
            pass

    def deploy_model(self, modelId, silent=False):
        try:
            file = '/flash/model_definition.json'
            modelDefinition = {}
            url = '{}://{}/ml/{}'.format(
                constants.__DEFAULT_PYCONFIG_PROTOCOL,
                constants.__DEFAULT_PYCONFIG_DOMAIN,
                modelId
            )
            print_debug(2, '{}'.format(url))
            result = urequest.get(url, headers={'content-type': 'application/json'})
            modelDefinition = json.loads(result.content.decode())
            print_debug(2, 'modelDefinition: {}'.format(modelDefinition))
            f = open(file, 'w')
            f.write(json.dumps(modelDefinition).encode('utf-8'))
            f.close()
            print_debug(2, "Model definition written to {}".format(file))
        except Exception as e:
            if not silent:
                print_debug(2, "Exception: {}".format(e))
