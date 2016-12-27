
Bluetooth
---------

Full info on bluetooth can be found on :class:`bluetooth page <.Bluetooth>` the Firmware API Reference

A quick usage example that scans and prints the advertisements found.

::

	from network import Bluetooth
	bluetooth = Bluetooth()
	bluetooth.start_scan(-1)    # start scanning with no timeout
	while True:
	    print(bluetooth.get_adv())


Connecting to a device

::

	from network import Bluetooth
	import binascii
	bluetooth = Bluetooth()

	# scan until we can connect to any BLE device around
	bluetooth.start_scan(-1)
	adv = None
	while True:
	    adv = bluetooth.get_adv()
	    if adv:
	        try:
	            bluetooth.connect(adv.mac)
	        except:
	            pass
	        if bluetooth.isconnected()
	            break
	printf("Connected to device with addr = {}".format(binascii.hexlify(adv.mac)))

Using :ref:`resolve_adv_data <network.bluetooth.resolve_adv_data>` to get more info of a device

::

	import binascii

	bluetooth.start_scan(5)
	while True:
	    adv = bluetooth.get_adv()
	    if adv:
	        # try to get the complete name
	        print(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL))

	        # try to get the manufacturer data (Apple's iBeacon data is sent here)
	        print(binascii.hexlify(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_MANUFACTURER_DATA)))