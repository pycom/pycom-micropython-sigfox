
Bluetooth
---------

Currently, basic BLE functionality is available. More features will be implemented soon, like pairing. This page will be updated along the way.

Full info on bluetooth can be found on :class:`bluetooth page <.Bluetooth>` the Firmware API Reference.

A quick usage example that scans and prints the raw data from advertisements.

::

	from network import Bluetooth
	bluetooth = Bluetooth()
	bluetooth.start_scan(-1)    # start scanning with no timeout
	while True:
	    print(bluetooth.get_adv())

Connecting to a device that is sending advertisements.

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
                # start scanning again
                bluetooth.start_scan(-1)
                continue
            break
    print("Connected to device with addr = {}".format(binascii.hexlify(adv.mac)))


Connecting to a device named 'Heart Rate' and receiving data from it's services.

::

    from network import Bluetooth
    import time
    bt = Bluetooth()
    bt.start_scan(-1)

    while True:
      adv = bt.get_adv()
      if adv and bt.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL) == 'Heart Rate':
          try:
              conn = bt.connect(adv.mac)
              services = conn.services()
              for service in services:
                  time.sleep(0.050)
                  if type(service.uuid()) == bytes:
                      print('Reading chars from service = {}'.format(service.uuid()))
                  else:
                      print('Reading chars from service = %x' % service.uuid())
                  chars = service.characteristics()
                  for char in chars:
                      if (char.properties() & Bluetooth.PROP_READ):
                          print('char {} value = {}'.format(char.uuid(), char.read()))
              conn.disconnect()
              break
          except:
              pass
      else:
          time.sleep(0.050)


Using ``resolve_adv_data()`` to atempt to get the name and manufacturer data of the advertiser.

::

    import binascii
    from network import Bluetooth
    bluetooth = Bluetooth()

    bluetooth.start_scan(20)
    while bluetooth.isscanning():
        adv = bluetooth.get_adv()
        if adv:
            # try to get the complete name
            print(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL))

            mfg_data = bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_MANUFACTURER_DATA)

            if mfg_data:
                # try to get the manufacturer data (Apple's iBeacon data is sent here)
                print(binascii.hexlify(mfg_data))
