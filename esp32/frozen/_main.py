print('Pybytes library: v0.9.0')

'''
This is the main.py file for Pybytes
This code and included libraries are intended for users wishing to fully
customise pybytes. It is the same code that is included in the Pybytes firmware
If you're planning to use the Pybytes firmware, please check out the
examples in the examples directory which are much easier to use.
If you make changes to any of the libraries in the lib directory,
you only need to upload the changed files if using the Pybytes firmware.
The other libraries will be loaded from the built-in code.
If you make changes above "Please put your USER code below this line" while
using a Pybytes enabled firmware, you need to disable auto-start.
You can disable auto-start by setting "pybytes_autostart": false in
pybytes_project.json or pybytes_config.json.
If using the Pybytes firmware, the configuration is already loaded and this
cannot be deactivated. However if you disable auto-start, you can modify the
configuration before connecting to Pybytes manually using pybytes.connect()
'''

# Load configuration, migrate to pybytes_config.json if necessary
if 'pybytes_config' not in globals().keys():
    try:
        from pybytes_config import PybytesConfig
        frozen = False
        try:
            from pybytes import Pybytes
        except:
            frozen = True
    except:
        from _pybytes_config import PybytesConfig
        frozen = True

    pybytes_config = PybytesConfig().read_config()

# Load Pybytes if it is not already loaded
if 'pybytes' not in globals().keys() and pybytes_config.get('pybytes_autostart', True):
    if frozen:
        from _pybytes import Pybytes

    pybytes = Pybytes(pybytes_config)
    pybytes.print_cfg_msg()
    pybytes.connect()

elif not pybytes_config.get('pybytes_autostart', True) and pybytes_config.get('cfg_msg') is not None:
    print(pybytes_config.get('cfg_msg'))
    print("Not starting Pybytes as auto-start is disabled")

if 'pybytes' in globals().keys():
    if pybytes.is_connected():

        print("Now starting user code in main.py")
        '''
        If Pybytes isn't connected at this time, it means you either deliberately
        disabled Pybytes auto-start, or something went wrong.
        This could be reading the configuration or establishing a connection.

        To connect to Pybytes manually when auto-start is disabled, please call:
        pybytes.connect()
        '''

        # Please put your USER code below this line

        # SEND VIRTUAL VALUE
        # You can currently send Strings, Int32, Float32 and Tuples to pybytes using this method.
        # Soon you will be able to send Coordinates, Longs, Unsigned numerical types, and more.
        # pybytes.send_virtual_pin_value(False, pin, value)

        # SEND SENSOR DATA THROUGH VIRTUAL PINS
        # # If you use a Pysense, some libraries are necessary to access its sensors
        # # you can find them here: https://github.com/pycom/pycom-libraries
        #
        # # Include the libraries in the lib folder then import the ones you want to use here:
        # from SI7006A20 import SI7006A20
        # si = SI7006A20()
        # from LTR329ALS01 import LTR329ALS01
        # ltr = LTR329ALS01()
        #
        # # Import what is necessary to create a thread
        # import _thread
        # from time import sleep
        # from machine import Pin
        #
        # # Define your thread's behaviour, here it's a loop sending sensors data every 10 seconds
        # def send_env_data():
        #     while (pybytes):
        #         pybytes.send_virtual_pin_value(False, 1, si.humidity())
        #         pybytes.send_virtual_pin_value(False, 2, si.temperature())
        #         pybytes.send_virtual_pin_value(False, 3, ltr.light());
        #         sleep(10)
        #
        # # Start your thread
        # _thread.start_new_thread(send_env_data, ())

        # SET THE BATTERY LEVEL
        # pybytes.send_battery_level(23)

        # SEND DIGITAL VALUE
        # pybytes.send_digital_pin_value(False, 12, Pin.PULL_UP)

        # SEND ANALOG VALUE
        # pybytes.send_analog_pin_value(False, 13)

        # REGISTER PERIODICAL DIGIAL VALUE SEND
        # pybytes.register_periodical_digital_pin_publish(False, PIN_NUMBER, Pin.PULL_UP, INTERVAL_SECONDS)

        # REGISTER PERIODICAL ANALOG VALUE SEND
        # pybytes.register_periodical_analog_pin_publish(False, PIN_NUMBER, INTERVAL_SECONDS)

        # CUSTOM METHOD EXAMPLE
        # def custom_print(params):
        #     print("Custom method called")
        #     return [255, 20]
        # pybytes.add_custom_method(0, custom_print)
