.. currentmodule:: network

class WLAN
==========

This class provides a driver for the WiFi network processor in the module. Example usage::

    import network
    import time
    # setup as a station
    wlan = network.WLAN(mode=network.WLAN.STA)
    wlan.connect('your-ssid', auth=(network.WLAN.WPA2, 'your-key'))
    while not wlan.isconnected():
        time.sleep_ms(50)
    print(wlan.ifconfig())

    # now use socket as usual
    ...



Quick usage example
-------------------

    ::

        import machine
        from network import WLAN

        # configure the WLAN subsystem in station mode (the default is AP)
        wlan = WLAN(mode=WLAN.STA)
        # go for fixed IP settings (IP, Subnet, Gateway, DNS)
        wlan.ifconfig(config=('192.168.0.107', '255.255.255.0', '192.168.0.1', '192.168.0.1'))
        wlan.scan()     # scan for available networks
        wlan.connect(ssid='mynetwork', auth=(WLAN.WPA2, 'my_network_key'))
        while not wlan.isconnected():
            pass
        print(wlan.ifconfig())

Constructors
------------

.. class:: WLAN(id=0, ...)

   Create a WLAN object, and optionally configure it. See ``init`` for params of configuration.

.. note::

   The ``WLAN`` constructor is special in the sense that if no arguments besides the id are given,
   it will return the already existing ``WLAN`` instance without re-configuring it. This is
   because ``WLAN`` is a system feature of the WiPy. If the already existing instance is not
   initialized it will do the same as the other constructors an will initialize it with default
   values.

Methods
-------

.. method:: wlan.init(mode, \*, ssid, auth, channel, antenna)

   Set or get the WiFi network processor configuration.

   Arguments are:

     - ``mode`` can be either ``WLAN.STA``, ``WLAN.AP`` or WLAN.STA_AP.
     - ``ssid`` is a string with the ssid name. Only needed when mode is ``WLAN.AP``.
     - ``auth`` is a tuple with (sec, key). Security can be ``None``, ``WLAN.WEP``,
       ``WLAN.WPA`` or ``WLAN.WPA2``. The key is a string with the network password.
       If ``sec`` is ``WLAN.WEP`` the key must be a string representing hexadecimal
       values (e.g. 'ABC1DE45BF'). Only needed when mode is ``WLAN.AP``.
     - ``channel`` a number in the range 1-11. Only needed when mode is ``WLAN.AP``.
     - ``antenna`` selects between the internal and the external antenna. Can be either
       ``WLAN.INT_ANT`` or ``WLAN.EXT_ANT``.

   For example, you can do::

      # create and configure as an access point
      wlan.init(mode=WLAN.AP, ssid='wipy-wlan', auth=(WLAN.WPA2,'www.wipy.io'), channel=7, antenna=WLAN.INT_ANT)

   or::

      # configure as an station
      wlan.init(mode=WLAN.STA)

.. method:: wlan.deinit()

    Disables the WiFi radio.

.. method:: wlan.connect(ssid, \*, auth=None, bssid=None, timeout=None)

   Connect to a wifi access point using the given SSID, and other security
   parameters.

      - ``auth`` is a tuple with (sec, key). Security can be ``None``, ``WLAN.WEP``,
        ``WLAN.WPA`` or ``WLAN.WPA2``. The key is a string with the network password.
        If ``sec`` is ``WLAN.WEP`` the key must be a string representing hexadecimal
        values (e.g. 'ABC1DE45BF').
      - ``bssid`` is the MAC address of the AP to connect to. Useful when there are several
        APs with the same ssid.
      - ``timeout`` is the maximum time in milliseconds to wait for the connection to succeed.

.. method:: wlan.scan()

   Performs a network scan and returns a list of named tuples with (ssid, bssid, sec, channel, rssi).
   Note that channel is always ``None`` since this info is not provided by the WiPy.

.. method:: wlan.disconnect()

   Disconnect from the wifi access point.

.. method:: wlan.isconnected()

   In case of STA mode, returns ``True`` if connected to a wifi access point and has a valid IP address.
   In AP mode returns ``True`` when a station is connected, ``False`` otherwise.

.. method:: wlan.ifconfig(id=0, config=['dhcp' or configtuple])

   When ``id`` is 0, the configuration will be get/set on the **Station** interface. When ``id`` is 1 the configuration will be done for the **AP** interface.

   With no parameters given eturns a 4-tuple of ``(ip, subnet_mask, gateway, DNS_server)``.

   if ``'dhcp'`` is passed as a parameter then the DHCP client is enabled and the IP params
   are negotiated with the AP.

   If the 4-tuple config is given then a static IP is configured. For instance::

      wlan.ifconfig(config=('192.168.0.4', '255.255.255.0', '192.168.0.1', '8.8.8.8'))

.. method:: wlan.mode([mode])

   Get or set the WLAN mode.

.. method:: wlan.ssid([ssid])

   Get or set the SSID when in AP mode.

.. method:: wlan.auth([auth])

   Get or set the authentication type when in AP mode.

.. method:: wlan.channel([channel])

   Get or set the channel (only applicable in AP mode).

.. method:: wlan.antenna([antenna])

   Get or set the antenna type (external or internal).

.. only:: port_wipy

    .. method:: wlan.mac([mac_addr])

       Get or set a 6-byte long bytes object with the MAC address.

    .. method:: wlan.irq(\*, handler, wake)

        Create a callback to be triggered when a WLAN event occurs during ``machine.SLEEP``
        mode. Events are triggered by socket activity or by WLAN connection/disconnection.

            - ``handler`` is the function that gets called when the irq is triggered.
            - ``wake`` must be ``machine.SLEEP``.

        Returns an irq object.

.. only:: port_2wipy or port_lopy or port_pycom_esp32

    .. method:: wlan.mac()

       Get a 6-byte long ``bytes`` object with the WiFI MAC address.

Constants
---------

.. data:: WLAN.STA
          WLAN.AP
          WLAN.STA_AP

   selects the WLAN mode

.. data:: WLAN.WEP
          WLAN.WPA
          WLAN.WPA2

   selects the network security

.. data:: WLAN.INT_ANT
          WLAN.EXT_ANT

   selects the antenna type
