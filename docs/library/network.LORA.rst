.. currentmodule:: network

class LoRa
==========

This class provides a driver for the LoRa network processor in the module. Example usage::

  from network import LoRa
  import socket

  # Initialize LoRa in LORAWAN mode.
  lora = LoRa(mode=LoRa.LORAWAN)
  # create an OTAA authentication tuple (AppKey, AppEUI, DevEUI)
  auth = (bytes([0,1,2,3,4,5,6,7,8,9,2,3,4,5,6,7]), bytes([1,2,3,4,5,6,7,8]), lora.mac()))
  # join a network using OTAA (Over the Air Activation)
  lora.join(activation=LoRa.OTAA, auth=auth, timeout=0)

  # wait until the module has joined the network
  while not lora.has_joined():
      time.sleep(2.5)
      print('Not yet joined...')

  # create a LoRa socket
  s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
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
      params will be ignored as theiy are handled by the LoRaWAN stack directly. On the other hand, these same 3
      params are ignored in ``LoRa.LORA`` mode as they are only relevant for the LoRaWAN stack.

   For example, you can do::

      # create and configure as an access point
      lora.init(mode=LoRa.LORA, tx_power=14, sf=12)

   or::

      # configure as an station
      lora.init(mode=LoRa.LORAWAN)

.. method:: lora.add_channel(index, \*, frequency, dr_min, dr_max, duty_cycle)

    Add a LoRaWAN channel on the specified index. If there's already a channel with that index it will be replaced with the new one.

    The arguments are:

      - ``index``: Index of the channel to add. Accepts values between 0 and 15 for EU and between 0 and 71 for US.
      - ``frequency``: Center frequency in Hz of the channel.
      - ``dr_min``: Minimmum data rate of the channel (0-7).
      - ``dr_min``: Maximum data rate of the channel (0-7).
      - ``duty_cycle``: Need to be always zero for now.

.. method:: lora.remove_channel(index)

     Removes the channel from the specified index. Channels 0 to 2 cannot be removed, they can only be replaced by other channels using the ``lora.add_channel`` method.

.. method:: lora.mac()

   Returns a byte object with the 8-Byte MAC address of the LoRa radio.