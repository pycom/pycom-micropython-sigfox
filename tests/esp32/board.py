
import os
import machine

mch = os.uname().machine
if 'ESP32' in mch:
    print('ESP32')
