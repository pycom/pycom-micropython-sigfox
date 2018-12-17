import pycom
DEBUG = pycom.nvs_get('pybytes_debug')

def print_debug(level, msg):
    if DEBUG is not None and level <= DEBUG:
        print(msg)
