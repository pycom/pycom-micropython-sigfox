.. currentmodule:: network

class Bluetooth
===============

This class provides a driver for the Bluetooth radio in the module.
Currently, only basic BLE functionality is available.

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
                  print("Error while connecting or reading from the BLE device")
                  break
          else:
              time.sleep(0.050)

Bluetooth Low Energy (BLE)
--------------------------

Bluetooth low energy (BLE) is a subset of classic Bluetooth, designed for easy connecting and communicating between devices (in particular mobile platforms). BLE uses a methodology known as Generic Access Profile (GAP) to control connections and advertising.

GAP allows for devices to take various roles but generic flow works with devices that are either a **Server** (low power, resource constrained, sending small payloads of data) or a **Client** device (commonly a mobile device, PC or Pycom Device with large resources and processing power). Pycom devices can act as both a Client and a Server.




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

   Initializes and enables the Bluetooth radio in BLE mode. ::

    bluetooth.init()

.. method:: bluetooth.deinit()

   Disables the Bluetooth radio. ::

    bluetooth.deinit()

.. method:: bluetooth.start_scan(timeout)

   Starts performing a scan listening for BLE devices sending advertisements. This function always returns inmmediatelly, the scanning will be performed on the background. The return value is ``None``. After starting the scan the function ``get_adv()`` can be used to retrieve the advertisements messages from the FIFO. The internal FIFO has space to cache 8 advertisements.

   The arguments are:

      - ``timeout`` specifies the amount of time in seconds to scan for advertisements, cannot be zero. If timeout is > 0, then the BLE radio will listen for advertisements until the specified value in seconds elapses. If timeout < 0, then there's no timeout at all, and ``stop_scan()`` needs to be called to cancel the scanning process.

   Examples::

        bluetooth.start_scan(10)        # starts scanning and stop after 10 seconds
        bluetooth.start_scan(-1)        # starts scanning indefenitely until bluetooth.stop_scan() is called

.. method:: bluetooth.stop_scan()

   Stops an ongoing scanning process. Returns ``None``. ::

    bluetooth.stop_scan()

.. method:: bluetooth.isscanning()

   Returns ``True`` if a Bluetooth scan is in progress. ``False`` otherwise. ::

    bluetooth.isscanning()

.. method:: bluetooth.get_adv()

   Gets an named tuple with the advertisement data received during the scanning. The tuple has the following structure: ``(mac, addr_type, adv_type, rssi, data)``

     - ``mac`` is the 6-byte ling mac address of the device that sent the advertisement.
     - ``addr_type`` is the address type. See the constants section below for more details.
     - ``adv_type`` is the advertisement type received. See the constants section below fro more details.
     - ``rssi`` is signed integer with the signal strength of the advertisement.
     - ``data`` contains the complete 31 bytes of the advertisement message. In order to parse the data and get the specific types, the method ``resolve_adv_data()`` can be used.

   Example for getting ``mac`` address of an advertiser:
   ::

    import binascii

    bluetooth = Bluetooth()
    bluetooth.start_scan(20) # scan for 20 seconds

    adv = bluetooth.get_adv() #
    binascii.hexlify(adv.mac) # convert hexidecimal to ascii


.. method:: bluetooth.resolve_adv_data(data, data_type)

    Parses the advertisement data and returns the requested data_type if present. If the data type is not present, the function returns ``None``.

    Arguments:

       - ``data`` is the bytes object with the complete advertisement data.
       - ``data_type`` is the data type to resolve from from the advertisement data. See constants section below for details.

    Example::

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


.. method:: bluetooth.connect(mac_addr)

    Opens a BLE connection with the device specified by the ``mac_addr`` argument. This function blocks until the connection succeeds or fails. If the connections succeeds it returns a object of type ``GATTCConnection``. ::

     bluetooth.connect('112233eeddff') # mac address is accepted as a string


.. method:: bluetooth.callback(trigger=None, handler=None, arg=None)

    Creates a callback that will be executed when any of the triggers occurs. The arguments are:

       - ``trigger`` can be either ``Bluetooth.NEW_ADV_EVENT``, ``Bluetooth.CLIENT_CONNECTED`` or ``Bluetooth.CLIENT_DISCONNECTED``
       - ``handler`` is the function that will be executed when the callback is triggered.
       - ``arg`` is the argument that gets passed to the callback. If nothing is given the bluetoth object itself is used.

    An example of how this may be used can be seen in the :ref:`bluetooth.events() <bluetooth_events>` method.

.. _bluetooth_events:

.. method:: bluetooth.events()

    Returns a value with bit flags identifying the events that have occurred since the last call. Calling this function clears the events. Example of usage: ::

        from network import Bluetooth

        bluetooth = Bluetooth()
        bluetooth.set_advertisement(name='LoPy', service_uuid=b'1234567890123456')

        def conn_cb (bt_o):
            events = bt_o.events()   # this method returns the flags and clears the internal registry
            if events & Bluetooth.CLIENT_CONNECTED:
                print("Client connected")
            elif events & Bluetooth.CLIENT_DISCONNECTED:
                print("Client disconnected")

        bluetooth.callback(trigger=Bluetooth.CLIENT_CONNECTED | Bluetooth.CLIENT_DISCONNECTED, handler=conn_cb)

        bluetooth.advertise(True)


.. method:: bluetooth.set_advertisement(\*, name=None, manufacturer_data=None, service_data=None, service_uuid=None)

    Configure the data to be sent while advertising. If left with the default of ``None`` the data won't be part of
    the advertisement message.

    The arguments are:

       - ``name`` is the string name to be shown on advertisements.
       - ``manufacturer_data`` manufacturer data to be advertised (hint: use it for iBeacons).
       - ``service_data`` service data to be advertised.
       - ``service_uuid`` uuid of the service to be advertised.

    Example:

    ::

     bluetooth.set_advertisement(name="advert", manufacturer_data="lopy_v1")

.. method:: bluetooth.advertise([Enable])

    Start or stop sending advertisements. The ``.set_advertisement()`` method must have been called prior to this one. ::

     bluetooth.advertise(True) # enable advertisements

.. method:: bluetooth.service(uuid, \*, isprimary=True, nbr_chars=1, start=True)

    Create a new service on the internal GATT server. Returns a object of type ``BluetoothServerService``.

    The arguments are:

       - ``uuid`` is the UUID of the service. Can take an integer or a 16 byte long string or bytes object.
       - ``isprimary`` selects if the service is a primary one. Takes a bool value.
       - ``nbr_chars`` specifies the number of characteristics that the service will contain.
       - ``start`` if ``True`` the service is started immediatelly.

    ::

     bluetooth.service('abc123')

.. method:: bluetooth.disconnect_client()

    Closes the BLE connection with the client.

    ::

     bluetooth.disconnect_client()

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

.. data:: Bluetooth.CHAR_CONFIG_NOTIFY
          Bluetooth.CHAR_CONFIG_INDICATE

    Characteristic configurations representing the current value of the characteristic's configuration descriptor.

.. data:: Bluetooth.CHAR_READ_EVENT
          Bluetooth.CHAR_WRITE_EVENT
          Bluetooth.NEW_ADV_EVENT
          Bluetooth.CLIENT_CONNECTED
          Bluetooth.CLIENT_DISCONNECTED
          Bluetooth.CHAR_NOTIFY_EVENT
          Bluetooth.CHAR_SUBSCRIBE_EVENT

    Charactertistic callback events

Generic Attribute Profile (GATT)
--------------------------------

GATT stands for the Generic Attribute Profile and it defines the way that two Bluetooth Low Energy devices communicate between each other using concepts called Services and Characteristics. GATT uses a data protocol known as the Attribute Protocol (ATT), which is used to store/manage Services, Characteristics and related data in a lookup table.

GATT comes into use once a connection is established between two devices, meaning that you have already gone through the advertising process managed by GAP. It's important to remember that this connection is exclusive; i.e. that only **one client** is connected to **one server** at a time. This means that the client will stop advertising once a connection has been made. This remains the case, until the connection is broken or disconnected.

The GATT Server, which holds the ATT lookup data and service and characteristic definitions, and the GATT Client (the phone/tablet), which sends requests to this server.


class GATTCConnection
=====================

The GATT Client is the device that requests data from the server, otherwise known as the **master** device (commonly this might be a phone/tablet/PC). All transactions are initiated by the master, which receives a response from the slave.


.. method:: connection.disconnect()

    Closes the BLE connection. Returns ``None``. ::

     connection.disconnect()

.. method:: connection.isconnected()

    Returns ``True`` if the connection is still open. ``False`` otherwise.

    Example: ::

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


.. method:: connection.services()

    Performs a service search on the connected BLE peripheral (server) a returns a list containing objects of the class ``GATTCService`` if the search succeeds.

    Example: ::

      # assuming that a BLE connection is already open
      services = connection.services()
      print(services)
      for service in services:
          print(service.uuid())


class GATTCService
==================

**Services** are used to categorise data up into specific chunks of data known as characteristics. A service may have multiple characteristics, and each service has a unique numeric ID called a UUID.

The following class allows control over **Client** services.

.. method:: service.isprimary()

    Returns ``True`` if the service is a primary one. ``False`` otherwise.

    ::

     service.isprimary()

.. method:: service.uuid()

    Returns the UUID of the service. In the case of 16-bit or 32-bit long UUIDs, the value returned is an integer, but for 128-bit long UUIDs the value returned is a bytes object. ::

     service.uuid()

.. method:: service.instance()

    Returns the instance ID of the service. ::

     service.instance()

.. method:: service.characteristics()

    Performs a get characteristics request on the connected BLE peripheral a returns a list containing objects of the class ``GATTCCharacteristic`` if the request succeeds. ::

     service.characteristics()


class GATTCCharacteristic
=========================

The smallest concept in GATT is the **Characteristic**, which encapsulates a single data point (though it may contain an array of related data, such as X/Y/Z values from a 3-axis accelerometer, longitude and latitude from a GPS, etc.).

The following class allows you to manage characteristics from a **Client**.

.. method:: characteristic.uuid()

    Returns the UUID of the service. In the case of 16-bit or 32-bit long UUIDs, the value returned is an integer, but for 128-bit long UUIDs the value returned is a bytes object. ::

     characteristics.uuid()

.. method:: characteristic.instance()

    Returns the instance ID of the service. ::

     characteristic.instance()

.. method:: characteristic.properties()

    Returns an integer indicating the properties of the characteristic. Properties are represented by bit values that can be ORed together. See the constants section for more details. ::

     characteristic.properties()

.. method:: characteristic.config()

    Returns an integer indicating the client configuration of the characteristic. Configurations are represented by bit values that can be ORed together. The configuration is often updated when the CHAR_SUBSCRIBE_EVENT is generated. See the constants section for more details. ::

     characteristic.config()

.. method:: characteristic.read()

    Read the value of the characteristic. For now it always returns a bytes object representing the characteristic value. In the future a specific type (integer, string, bytes) will be returned depending on the characteristic in question. ::

     characteristic.read()

.. method:: characteristic.write(value)

    Writes the given value on the characteristic. For now it only accepts bytes object representing the value to be written. ::

     characteristic.write(b'x0f')

.. method:: characteristic.callback(trigger=None, handler=None, arg=None)

    This method allows to register for notifications on the characteristic.

       - ``trigger`` can must be ``Bluetooth.CHAR_NOTIFY_EVENT``.
       - ``handler`` is the function that will be executed when the callback is triggered.
       - ``arg`` is the argument that gets passed to the callback. If nothing is given, the characteristic object that owns the callback will be used.


class GATTSService
==================

The GATT Server allows the device to act as a peripheral and hold its own ATT lookup data, server & characteristic definitions. In this mode, the device acts as a **slave** and a master must initiate a request.

**Services** are used to categorise data up into specific chunks of data known as characteristics. A service may have multiple characteristics, and each service has a unique numeric ID called a UUID.

The following class allows control over **Server** services.


.. method:: service.start()

    Starts the service if not already started.

.. method:: service.stop()

    Stops the service if previously started.


.. method:: service.characteristic(uuid, \*, permissions, properties, value)

    Creates a new characteristic on the service. Returns an object of the class ``GATTSCharacteristic``.
    The arguments are:

      - ``uuid`` is the UUID of the service. Can take an integer or a 16 byte long string or bytes object.
      - ``permissions`` configures the permissions of the characteristic. Takes an integer with a combination of the flags.
      - ``properties`` sets the properties. Takes an integer with an ORed combination of the flags.
      - ``value`` sets the initial value. Can take an integer, a string or a bytes object.

    ::

     service.characteristic('temp', value=25)


class GATTSCharacteristic
=========================

The smallest concept in GATT is the **Characteristic**, which encapsulates a single data point (though it may contain an array of related data, such as X/Y/Z values from a 3-axis accelerometer, longitude and latitude from a GPS, etc.).

The following class allows you to manage **Server** characteristics.

.. method:: characteristic.value([value])

    Gets or sets the value of the characteristic. Can take an integer, a string or a bytes object. ::

     characteristic.value(123) # set characteristic value to an integer with the value 123
     characteristic.value() # get characteristic value

.. method:: characteristic.callback(trigger=None, handler=None, arg=None)

    Creates a callback that will be executed when any of the triggers occurs. The arguments are:

       - ``trigger`` can be either ``Bluetooth.CHAR_READ_EVENT`` or ``Bluetooth.CHAR_WRITE_EVENT`` or ``Bluetooth.CHAR_SUBSCRIBE_EVENT``.
       - ``handler`` is the function that will be executed when the callback is triggered.
       - ``arg`` is the argument that gets passed to the callback. If nothing is given, the characteristic object that owns the callback will be used.

    An example of how this could be implemented can be seen in the :ref:`characteristic.events() <characteristic_events>` section.

.. _characteristic_events:

.. method:: characteristic.events()

    Returns a value with bit flags identifying the events that have occurred since the last call. Calling this function clears the events.


    An example of advertising and creating services on the device: ::

     from network import Bluetooth

     bluetooth = Bluetooth()
     bluetooth.set_advertisement(name='LoPy', service_uuid=b'1234567890123456')

     def conn_cb (bt_o):
         events = bt_o.events()
         if  events & Bluetooth.CLIENT_CONNECTED:
             print("Client connected")
         elif events & Bluetooth.CLIENT_DISCONNECTED:
             print("Client disconnected")

     bluetooth.callback(trigger=Bluetooth.CLIENT_CONNECTED | Bluetooth.CLIENT_DISCONNECTED, handler=conn_cb)

     bluetooth.advertise(True)

     srv1 = bluetooth.service(uuid=b'1234567890123456', isprimary=True)

     chr1 = srv1.characteristic(uuid=b'ab34567890123456', value=5)

     char1_read_counter = 0
     def char1_cb_handler(chr):
         global char1_read_counter
         char1_read_counter += 1

         events = chr.events()
         if  events & Bluetooth.CHAR_WRITE_EVENT:
             print("Write request with value = {}".format(chr.value()))
         else:
             if char1_read_counter < 3:
                 print('Read request on char 1')
             else:
                 return 'ABC DEF'

     char1_cb = chr1.callback(trigger=Bluetooth.CHAR_WRITE_EVENT | Bluetooth.CHAR_READ_EVENT, handler=char1_cb_handler)

     srv2 = bluetooth.service(uuid=1234, isprimary=True)

     chr2 = srv2.characteristic(uuid=4567, value=0x1234)
     char2_read_counter = 0xF0
     def char2_cb_handler(chr):
         global char2_read_counter
         char2_read_counter += 1
         if char2_read_counter > 0xF1:
             return char2_read_counter

     char2_cb = chr2.callback(trigger=Bluetooth.CHAR_READ_EVENT, handler=char2_cb_handler)

