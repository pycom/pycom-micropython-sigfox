****************************************
:mod:`network` --- network configuration
****************************************

.. module:: network
   :synopsis: network configuration

This module provides network drivers and routing configuration.  Network
drivers for specific hardware are available within this module and are
used to configure a hardware network interface.

Click one of the links below to see more information and exmaples for
each network.

.. only:: port_lopy or port_pycom_esp32

  Classes
  -------

  .. toctree::
     :maxdepth: 1

     network.Server.rst
     network.WLAN.rst
     network.LoRa.rst
     network.Bluetooth.rst
     network.Sigfox.rst

.. only:: port_pyboard

  Classes
  -------

  .. toctree::
     :maxdepth: 1

     network.CC3K.rst

.. only:: port_wipy or port_2wipy

   Classes
  -------

  .. toctree::
     :maxdepth: 1

     network.Server.rst
     network.WLAN.rst
     network.Bluetooth.rst
