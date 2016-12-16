****************************************
:mod:`network` --- network configuration
****************************************

.. module:: network
   :synopsis: network configuration

This module provides network drivers and routing configuration.  Network
drivers for specific hardware are available within this module and are
used to configure a hardware network interface.  Configured interfaces
are then available for use via the :mod:`socket` module. To use this module
the network build of firmware must be installed.

For example::

    # configure a specific network interface
    # see below for examples of specific drivers
    import network
    nic = network.Driver(...)
    print(nic.ifconfig())

    # now use socket as usual
    import socket
    addr = socket.getaddrinfo('micropython.org', 80)[0][-1]
    s = socket.socket()
    s.connect(addr)
    s.send(b'GET / HTTP/1.1\r\nHost: micropython.org\r\n\r\n')
    data = s.recv(1000)
    s.close()

.. only:: port_lopy or port_pycom_esp32

  Classes
  -------

  .. toctree::
     :maxdepth: 1

     network.Server.rst
     network.WLAN.rst
     network.LORA.rst
     network.Bluetooth.rst

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
