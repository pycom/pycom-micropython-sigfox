.. currentmodule:: network

class Bluetooth
===============

This class provides a driver for the Bluetooth radio in the module.
At the moment only basic BLE functionality is available.

Quick usage example
-------------------

    ::

          from network import Bluetooth
          import time
          bt = Bluetooth()
          bt.start_scan(-1)

          while True:
              adv = bt.get_adv()
              if adv and bt.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL) == 'Heart Rate':
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
              else:
                  time.sleep(0.050)


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

   Starts performing a scan listening for BLE devices sending advertisements. This function always returns inmmediatelly, the scanning will be performed on the background. The return value is ``None``. After starting the scan the function ``get_adv()`` can be used to retrieve the advertisements messages from the FIFO. The internal FIFO has space to cache 8 advertisements.

   The arguments are:

      - ``timeout`` specifies the amount of time in seconds to scan for advertisements, cannot be zero. If timeout is > 0, then the BLE radio will listen for advertisements until the specified value in seconds elapses. If timeout < 0, then there's no timeout at all, and ``stop_scan()`` needs to be called to cancel the scanning process.

   Examples::

        bluetooth.start_scan(10)        # starts scanning and stop after 10 seconds
        bluetooth.start_scan(-1)        # starts scanning indefenitely until bluetooth.stop_scan() is called

.. method:: bluetooth.stop_scan()

   Stops an ongoing scanning process. Returns ``None``.

.. method:: bluetooth.isscanning()

   Returns ``True`` if a Bluetooth scan is in progress. ``False`` otherwise.

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
                # try to the the complete name
                print(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL))

                # try to get the manufacturer data (Apple's iBeacon data is sent here)
                print(binascii.hexlify(bluetooth.resolve_adv_data(adv.data, Bluetooth.ADV_MANUFACTURER_DATA))

.. method:: bluetooth.connect(mac_addr)

    Opens a BLE connection with the device specified by the ``mac_addr`` argument. This function blocks until the connection succeeds or fails. If the connections succeeds it returns a object of type ``BluetoothConnection``.

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

.. data:: Bluetooth.PROP_BROADCAST
          Bluetooth.PROP_READ
          Bluetooth.PROP_WRITE_NR
          Bluetooth.PROP_WRITE
          Bluetooth.PROP_NOTIFY
          Bluetooth.PROP_INDICATE
          Bluetooth.PROP_AUTH
          Bluetooth.PROP_EXT_PROP

    Characteristic properties (bit values that can be combined)


class BluetoothConnection
=========================

.. method:: connection.disconnect()

    Closes the BLE connection. Returns ``None``.

.. method:: connection.isconnected()

    Returns ``True`` if the connection is still open. ``False`` otherwise.

    Example::

        from network import Bluetooth
        import binascii
        bluetooth = Bluetooth()

        # scan until we can connect to any BLE device around
        bluetooth.start_scan(-1)
        while True:
            adv = bluetooth.get_adv()
            if adv:
                try:
                    connection = bluetooth.connect(adv.mac)
                except:
                    continue
                if connection.isconnected()
                    break
        printf("Connected to device with addr = {}".format(binascii.hexlify(adv.mac)))

.. method:: connection.services()

    Performs a service search on the connected BLE peripheral a returns a list containing objects of the class ``BluetoothService`` if the search succeeds.

    Example::

      # assuming that a BLE connection is already open
      services = connection.services()
      print(services)
      for service in services:
          print(service.uuid())


class BluetoothService
======================

.. method:: service.isprimary()

    Returns ``True`` if the service is a primary one. ``False`` otherwise.

.. method:: service.uuid()

    Returns the UUID of the service. In the case of 16-bit or 32-bit long UUIDs, the value returned is an integer, but for 128-bit long UUIDs the value returned is a bytes object.

.. method:: service.instance()

    Returns the instance ID of the service.

.. method:: service.characteristics()

    Performs a get characteristics request on the connected BLE peripheral a returns a list containing objects of the class ``BluetoothCharacteristic`` if the request succeeds.


class BluetoothCharacteristic
=============================

.. method:: characteristic.uuid()

    Returns the UUID of the service. In the case of 16-bit or 32-bit long UUIDs, the value returned is an integer, but for 128-bit long UUIDs the value returned is a bytes object.

.. method:: characteristic.instance()

    Returns the instance ID of the service.

.. method:: characteristic.properties()

    Returns an integer indicating the properties of the characteristic. Properties are represented by bit values that can be ORed together. See the constants section for more details.

.. method:: characteristic.read()

    Read the value of the characteristic. For now it always returns a bytes object represetning the characteristic value. In the future a specific type (integer, string, bytes) will be returned depending on the characteristic in question.

.. method:: characteristic.write(value)

    Writes the given value on the characteristic. For now it only accepts bytes object representing the value to be written.
