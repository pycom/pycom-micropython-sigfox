.. currentmodule:: network

class Bluetooth
===============

This class provides a driver for the Bluetooth radio in the module.
At the moment only basic BLE functionality is available.

Quick usage example
-------------------

    ::

        from network import Bluetooth
        bluetooth = Bluetooth()
        bluetooth.start_scan(-1)    # start scanning with no timeout
        while True:
            print(bluetooth.get_adv())

Constructors
------------

.. class:: Bluetooth(id=0, ...)

   Create a Bluetooth object, and optionally configure it. See ``init`` for params of configuration.

   Example::

        from network import Bluetooth
        bluetooth = Bluetooth()

Methods
-------

.. method:: bluetooth.init()

   Initialize the Bluetooth radio in BLE mode.

.. method:: bluetooth.start_scan(timeout)

   Starts performing a scan listening for BLE devices sending advertisements. This function always returns inmmediatelly, the scanning will be performed on the background. The return value is ``None``.

   The arguments are:

      - ``timeout`` specifies the amount of time in seconds to scan for advertisements, cannot be zero. If timeout is > 0, then the BLE radio will listen for advertisements until the specified value in seconds elapses. If timeout < 0, then there's no timeout at all, and ``stop_scan()`` needs to be called to cancel the scanning process.

   Examples::

        bluetooth.start_scan(10)        # starts scanning and stop after 10 seconds
        bluetooth.start_scan(-1)        # starts scanning indefenitely until bluetooth.stop_scan() is called

.. method:: bluetooth.stop_scan()

   Stops an ongoing scanning process. Returns ``None``.

.. method:: bluetooth.get_adv()

   Gets an named tuple with the advertisement data received during the scanning. The tuple has the following structure: ``(mac, addr_type, adv_type, rssi, data)``

   - ``mac`` is the 6-byte ling mac address of the device that sent the advertisement.
   - ``addr_type`` is the address type. See the constants section below fro more details.
   - ``adv_type`` is the advertisement type received. See the constants section below fro more details.
   - ``rssi`` is signed integer with the signal strength of the advertisement.
   - ``data`` contains the complete 31 bytes of the advertisement message. In order to parse the data and get the specific types, the method ``resolve_adv_data()`` can be used.

.. method:: bluetooth.resolve_adv_data(data, data_type)

    Parses the advertisement data and returns the requested data_type if present. If the data type is not present, the function returns ``None``.

    Arguments:

       - ``data`` is the bytes object with the complete advertisement data.
       - ``data_type`` is the data type to resolve from from the advertisement data. See constants section below for details.

    Example::

        import binascii

        bluetooth.start_scan(5)
        while True:
            adv = bluetooth.get_adv()
            if adv:

                # try to get the complete name
                print(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL))

                # try to get the manufacturer data (Apple's iBeacon data is sent here)
                print(binascii.hexlify(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_MANUFACTURER_DATA)))

.. method:: bluetooth.connect(mac_addr)

    Opens a BLE connection with the device specified by the ``mac_addr`` argument. This function blocks until the connection succeeds or fails.

.. method:: bluetooth.isconnected()

    Returns ``True`` if a connection with another BLE device is active. ``False`` otherwise.

    Example::

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

.. method:: bluetooth.disconnect()

    Closes the current active BLE connection.

.. method:: bluetooth.get_services()

    If a BLE connection is active, this method gets the list of services provided by the BLE peripheral. The list will contain the ``UUID`` of the services. These UUIDs can be 16-bit, 32-bit or 128-bit long. 16 and 32 bit long UUIDs are returned as integers, while 128-bit UUIDs are returned as bytes objects.

    Example::

        bluetooth.connect(some_mac_addr)
        if bluetooth.isconnected():
            bluetooth.get_services()

Constants
---------

.. data:: Bluetooth.CONN_ADV
          Bluetooth.CONN_DIR_ADV
          Bluetooth.DISC_ADV
          Bluetooth.NON_CONN_ADV
          Bluetooth.SCAN_RSP

    Advertisement type

.. data:: Bluetooth.PUBLIC_ADDR
          Bluetooth.RANDOM_ADDR
          Bluetooth.PUBLIC_RPA_ADDR
          Bluetooth.RANDOM_RPA_ADDR

    Address type

.. data:: Bluetooth.ADV_FLAG
          Bluetooth.ADV_16SRV_PART
          Bluetooth.ADV_T16SRV_CMPL
          Bluetooth.ADV_32SRV_PART
          Bluetooth.ADV_32SRV_CMPL
          Bluetooth.ADV_128SRV_PART
          Bluetooth.ADV_128SRV_CMPL
          Bluetooth.ADV_NAME_SHORT
          Bluetooth.ADV_NAME_CMPL
          Bluetooth.ADV_TX_PWR
          Bluetooth.ADV_DEV_CLASS
          Bluetooth.ADV_SERVICE_DATA
          Bluetooth.ADV_APPEARANCE
          Bluetooth.ADV_ADV_INT
          Bluetooth.ADV_32SERVICE_DATA
          Bluetooth.ADV_128SERVICE_DATA
          Bluetooth.ADV_MANUFACTURER_DATA

    Advertisement data type