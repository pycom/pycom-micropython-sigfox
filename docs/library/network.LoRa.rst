.. currentmodule:: network

class LoRa
==========

This class provides a driver for the LoRa network processor in the **LoPy**. Example usage::

    from network import LoRa
    import socket
    import binascii
    import struct

    # Initialize LoRa in LORAWAN mode.
    lora = LoRa(mode=LoRa.LORAWAN)

    # create an ABP authentication params
    dev_addr = struct.unpack(">l", binascii.unhexlify('00 00 00 05'.replace(' ','')))[0]
    nwk_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))
    app_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))

    # join a network using ABP (Activation By Personalization)
    lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

    # create a LoRa socket
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)

    # set the LoRaWAN data rate
    s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)

    # make the socket non-blocking
    s.setblocking(False)

    # send some data
    s.send(bytes([0x01, 0x02, 0x03]))

    # get any data received...
    data = s.recv(64)
    print(data)


Additional examples
-------------------

Check here for :ref:`aditional examples<lora_examples>`.

Constructors
------------

.. class:: LoRa(id=0, ...)

   Create and configure a LoRa object. See ``init`` for params of configuration.

Methods
-------

.. method:: lora.init(mode, \*, frequency=868000000, tx_power=14, bandwidth=LoRa.868000000, sf=7, preamble=8, coding_rate=LoRa.CODING_4_5, power_mode=LoRa.ALWAYS_ON, tx_iq=false, rx_iq=false, adr=false, public=true, tx_retries=1)

   Set the LoRa subsystem configuration

   The arguments are:

     - ``mode`` can be either ``LoRa.LORA`` or ``LoRa.LORAWAN``.
     - ``frequency`` accepts values between 863000000 and 870000000 in the 868 band, or between 902000000 and 928000000 in the 915 band.
     - ``tx_power`` is the transmit power in dBm. It accepts between 2 and 14 for the 868 band, and between 5 and 20 in the 915 band.
     - ``bandwidth`` is the channel bandwidth in KHz. In the 868 band the accepted values are ``LoRa.BW_125KHZ`` and ``LoRa.BW_250KHZ``. In the 915 band the accepted values are ``LoRa.BW_125KHZ`` and ``LoRa.BW_500KHZ``.
     - ``sf`` sets the desired spreading factor. Accepts values between 7 and 12.
     - ``preamble`` configures the number of pre-amble symbols. The default value is 8.
     - ``coding_rate`` can take the following values: ``LoRa.CODING_4_5``, ``LoRa.CODING_4_6``,
       ``LoRa.CODING_4_7`` or ``LoRa.CODING_4_8``.
     - ``power_mode`` can be either ``LoRa.ALWAYS_ON``, ``LoRa.TX_ONLY`` or ``LoRa.SLEEP``. In ``ALWAYS_ON`` mode, the radio is always listening for incoming packets whenever a transmission is not taking place. In ``TX_ONLY`` the radio goes to sleep as soon as the transmission completes. In ``SLEEP`` mode the radio is sent to sleep permanently and won't accept any commands until the power mode is changed.
     - ``tx_iq`` enables TX IQ inversion.
     - ``rx_iq`` enables RX IQ inversion.
     - ``adr`` enables Adaptive Data Rate.
     - ``public`` selects wether the network is public or not.
     - ``tx_retries`` sets the number of TX retries in ``LoRa.LORAWAN`` mode.

    .. note:: In ``LoRa.LORAWAN`` mode, only ``adr``, ``public`` and ``tx_retries`` are used. All the other
      params will be ignored as theiy are handled by the LoRaWAN stack directly. On the other hand, these same 3 params are ignored in ``LoRa.LORA`` mode as they are only relevant for the LoRaWAN stack.

   For example, you can do::

      # initialize in raw LoRa mode
      lora.init(mode=LoRa.LORA, tx_power=14, sf=12)

   or::

      # initialize in LoRaWAN mode
      lora.init(mode=LoRa.LORAWAN)

.. method:: lora.join(activation, auth, \*, timeout=None)

    Join a LoRaWAN network. The parameters are:

      - ``activation``: can be either ``LoRa.OTAA`` or ``LoRa.ABP``.
      - ``auth``: is a tuple with the authentication data.

      In the case of ``LoRa.OTAA`` the authentication tuple is: ``(app_eui, app_key)``. Example::

          from network import LoRa
          import socket
          import time
          import binascii

          # Initialize LoRa in LORAWAN mode.
          lora = LoRa(mode=LoRa.LORAWAN)

          # create an OTAA authentication parameters
          app_eui = binascii.unhexlify('AD A4 DA E3 AC 12 67 6B'.replace(' ',''))
          app_key = binascii.unhexlify('11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'.replace(' ',''))

          # join a network using OTAA (Over the Air Activation)
          lora.join(activation=LoRa.OTAA, auth=(app_eui, app_key), timeout=0)

          # wait until the module has joined the network
          while not lora.has_joined():
              time.sleep(2.5)
              print('Not yet joined...')

      In the case of ``LoRa.ABP`` the authentication tuple is: ``(dev_addr, nwk_swkey, app_swkey)``. Example::

          from network import LoRa
          import socket
          import binascii
          import struct

          # Initialize LoRa in LORAWAN mode.
          lora = LoRa(mode=LoRa.LORAWAN)

          # create an ABP authentication params
          dev_addr = struct.unpack(">l", binascii.unhexlify('00 00 00 05'.replace(' ','')))[0]
          nwk_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))
          app_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))

          # join a network using ABP (Activation By Personalization)
          lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

.. method:: lora.bandwidth([bandwidth])

    Get or set the bandwidth in raw LoRa mode (``LoRa.LORA``). Can be either ``LoRa.BW_125KHZ``, ``LoRa.BW_250KHZ`` or ``LoRa.BW_500KHZ``.

.. method:: lora.frequency([frequency])

    Get or set the frequency in raw LoRa mode (``LoRa.LORA``). The allowed range is between 863000000 and 870000000 Hz for the 868MHz band version or between 902000000 and 928000000 Hz for the 915MHz abdn version.

.. method:: lora.coding_rate([coding_rate])

    Get or set the coding rate in raw LoRa mode (``LoRa.LORA``). The allowed values are: ``LoRa.CODING_4_5``, ``LoRa.CODING_4_6``, ``LoRa.CODING_4_7`` and ``LoRa.CODING_4_8``.

.. method:: lora.preamble([preamble])

    Get or set the number of preamble symbols in raw LoRa mode (``LoRa.LORA``).

.. method:: lora.sf([sf])

    Get or set the spreading factor value in raw LoRa mode (``LoRa.LORA``). The minimmum value is 7 and the maximum is 12.

.. method:: lora.power_mode([power_mode])

    Get or set the power mode in raw LoRa mode (``LoRa.LORA``). The accepted values are: ``LoRa.ALWAYS_ON``, ``LoRa.TX_ONLY`` and ``LoRa.SLEEP``.

.. method:: lora.rssi()

    Get the RSSI value from the last received LoRa or LoRaWAN packet.

.. method:: lora.has_joined()

    Returns ``True`` if a LoRaWAN network has been joined. ``False`` otherwise.

.. method:: lora.add_channel(index, \*, frequency, dr_min, dr_max)

    Add a LoRaWAN channel on the specified index. If there's already a channel with that index it will be replaced with the new one.

    The arguments are:

      - ``index``: Index of the channel to add. Accepts values between 0 and 15 for EU and between 0 and 71 for US.
      - ``frequency``: Center frequency in Hz of the channel.
      - ``dr_min``: Minimum data rate of the channel (0-7).
      - ``dr_max``: Maximum data rate of the channel (0-7).

.. method:: lora.remove_channel(index)

     Removes the channel from the specified index. On the 868MHz band the channels 0 to 2 cannot be removed, they can only be replaced by other channels using the ``lora.add_channel`` method. A way to remove all channels except for one is to add the same channel 3 times on indexes 0, 1 and 2.

     On the 915MHz band there are not restrictions around this.

.. method:: lora.mac()

   Returns a byte object with the 8-Byte MAC address of the LoRa radio.

Constants
---------

.. data:: LoRa.LORA
          LoRa.LORAWAN

    LoRa mode

.. data:: LoRa.OTAA
          LoRa.ABP

    LoRaWAN join procedure

.. data:: LoRa.ALWAYS_ON
          LoRa.TX_ONLY
          LoRa.SLEEP

    Raw LoRa power mode

.. data:: LoRa.BW_125KHZ
          LoRa.BW_250KHZ
          LoRa.BW_500KHZ

    Raw LoRa bandwidth

.. data:: LoRa.CODING_4_5
          LoRa.CODING_4_6
          LoRa.CODING_4_7
          LoRa.CODING_4_8

    Raw LoRa coding rate


Working with LoRa sockets
-------------------------

LoRa sockets are created in the following way::

   import socket
   s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)

And they must be created after initializing the LoRa network card.

LoRa sockets support the following standard methods from the :class:`socket <.socket>` module:

.. method:: socket.close()

   Usage: ``s.close()``

.. method:: socket.bind(port_number)

   Usage: ``s.bind(1)``

.. method:: socket.send(bytes)

   Usage: ``s.send(bytes([1, 2, 3]))`` or: ``s.send('Hello')``

.. method:: socket.recv(bufsize)

   Usage: ``s.recv(128)``

.. method:: socket.setsockopt(level, optname, value)

   Set the value of the given socket option. The needed symbolic constants are defined in the
   socket module (SO_* etc.). In the case of LoRa the values are always an integer. Examples::

      # configuring the data rate
      s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)

      # selecting non-confirmed type of messages
      s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, False)

      # selecting confirmed type of messages
      s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, True)

.. method:: socket.settimeout(value)

   Usage: ``s.settimeout(5.0)``

.. method:: socket.setblocking(flag)

   Usage: ``s.setblocking(True)``
