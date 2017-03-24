
.. _wlan_step_by_step:

WLAN step by step
-----------------

The WLAN is a system feature of the LoPy, therefore it is enabled by default.

In order to retrieve the current WLAN instance, do::

   >>> from network import WLAN
   >>> wlan = WLAN() # we call the constructor without params

You can check the current mode (which is normally ``WLAN.AP`` after power up)::

   >>> wlan.mode()

.. warning::
    When you change the WLAN mode following the instructions below, your WLAN
    connection to the LoPy will be broken. This means you will not be able
    to run these commands interactively over WiFi.

    There are two ways around this::
     1. put this setup code into your :ref:`boot.py file<pycom_filesystem>` so that it gets executed automatically after reset.
     2. :ref:`duplicate the REPL on UART <pycom_uart>`, so that you can run commands via USB.

Connecting to your home router
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The WLAN network card always boots in ``WLAN.AP`` mode, so we must first configure
it as a station::

   from network import WLAN
   wlan = WLAN(mode=WLAN.STA)


Now you can proceed to scan for networks::

    nets = wlan.scan()
    for net in nets:
        if net.ssid == 'mywifi':
            print('Network found!')
            wlan.connect(net.ssid, auth=(net.sec, 'mywifikey'), timeout=5000)
            while not wlan.isconnected():
                machine.idle() # save power while waiting
            print('WLAN connection succeeded!')
            break

Assigning a static IP address when booting
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want your LoPy to connect to your home router after boot-up, and with a fixed
IP address so that you can access it via telnet or FTP, use the following script as /flash/boot.py::

   import machine
   from network import WLAN
   wlan = WLAN() # get current object, without changing the mode

   if machine.reset_cause() != machine.SOFT_RESET:
       wlan.init(mode=WLAN.STA)
       # configuration below MUST match your home router settings!!
       wlan.ifconfig(config=('192.168.178.107', '255.255.255.0', '192.168.178.1', '8.8.8.8'))

   if not wlan.isconnected():
       # change the line below to match your network ssid, security and password
       wlan.connect('mywifi', auth=(WLAN.WPA2, 'mywifikey'), timeout=5000)
       while not wlan.isconnected():
           machine.idle() # save power while waiting

.. note::

   Notice how we check for the reset cause and the connection status, this is crucial in order
   to be able to soft reset the LoPy during a telnet session without breaking the connection.


Multiple networks with static IP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following script holds a list with nets and an optional list of wlan_config to set a fixed IP

::

  import os
  import machine

  uart = machine.UART(0, 115200)
  os.dupterm(uart)

  known_nets = {
      '<net>': {'pwd': '<password>'}, 
      '<net>': {'pwd': '<password>', 'wlan_config':  ('10.0.0.114', '255.255.0.0', '10.0.0.1', '10.0.0.1')}, # (ip, subnet_mask, gateway, DNS_server)
  }

  if machine.reset_cause() != machine.SOFT_RESET:
      from network import WLAN
      wl = WLAN()
      wl.mode(WLAN.STA)
      original_ssid = wl.ssid()
      original_auth = wl.auth()

      print("Scanning for known wifi nets")
      available_nets = wl.scan()
      nets = frozenset([e.ssid for e in available_nets])

      known_nets_names = frozenset([key for key in known_nets])
      net_to_use = list(nets & known_nets_names)
      try:
          net_to_use = net_to_use[0]
          net_properties = known_nets[net_to_use]
          pwd = net_properties['pwd']
          sec = [e.sec for e in available_nets if e.ssid == net_to_use][0]
          if 'wlan_config' in net_properties:
              wl.ifconfig(config=net_properties['wlan_config']) 
          wl.connect(net_to_use, (sec, pwd), timeout=10000)
          while not wl.isconnected():
              machine.idle() # save power while waiting
          print("Connected to "+net_to_use+" with IP address:" + wl.ifconfig()[0])
          
      except Exception as e:
          print("Failed to connect to any known network, going into AP mode")
          wl.init(mode=WLAN.AP, ssid=original_ssid, auth=original_auth, channel=6, antenna=WLAN.INT_ANT)