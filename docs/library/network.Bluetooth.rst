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
        bluetooth.scan()    # return a list of the advertised BLE devices nearby

Constructors
------------

.. class:: Bluetooth(id=0, ...)

   Create a Bluetooth object, and optionally configure it. See ``init`` for params of configuration.

Methods
-------

.. method:: bluetooth.init()

   Initialize the Bluetooth radio.

.. method:: bluetooth.scan()

   Performs a scan listening for BLE devices sendgin advertisements. Return a named tuple of the form: ``(name, mac)`` where ``name`` is a string with the name of the BLE device and ``mac`` is a bytes object of length 6 with the MAC address of the device.

   .. note::

     Currently this method blocks for 10s until the scan is completed. Parameters to configure the behaviour and the scan time will be added in future releases.
