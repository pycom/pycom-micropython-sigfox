.. currentmodule:: network

class Sigfox
============

This class provides a driver for the Sigfox network processor in the **SiPy**. Example usage::

   from network import Sigfox
   import socket

   # init Sigfox for RCZ1 (Europe)
   sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ1)

   # create a Sigfox socket
   s = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW)

   # make the socket blocking
   s.setblocking(True)

   # configure it as uplink only
   s.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, False)

   # send some bytes
   s.send(bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]))

Constructors
------------

.. class:: Sigfox(id=0, ...)

   Create and configure a Sigfox object. See ``init`` for params of configuration.

Methods
-------

.. method:: sigfox.init(mode, rcz=None)

   Set the Sigfox radio configuration.

   The arguments are:

     - ``mode`` can be either ``Sigfox.SIGFOX`` or ``Sigfox.FSK``. ``Sigfox.SIGFOX`` uses the Sigfox modulation and protocol while ``Sigfox.FSK`` allows to create point to point communication between 2 SiPy's using FSK modulation.
     - ``rcz`` takes the following values: ``Sigfox.RCZ1``, ``Sigfox.RCZ2``, ``Sigfox.RCZ3``, ``Sigfox.RCZ4``. The **rcz** argument is only required if the mode is ``Sigfox.SIGFOX``.

     .. warning::

        The SiPy comes in 2 different hardware flavors: a +14dBm Tx power version which can only work with RCZ1 and RCZ3 and a +22dBm version which works exclusively on RCZ2 and RCZ4.

.. method:: sigfox.mac()

   Returns a byte object with the 8-Byte MAC address of the Sigfox radio.

.. method:: sigfox.id()

   Returns a byte object with the 4-Byte bytes object with the Sigfox ID.

.. method:: sigfox.pac()

   Returns a byte object with the 8-Byte bytes object with the Sigfox PAC.

.. method:: sigfox.frequencies()

   Returns a tuple of the form: ``(uplink_frequency_hz, downlink_frequency_hz)``

.. method:: sigfox.public_key([public])

   Sets or gets the public key flag. When called passing a ``True`` value the Sigfox public key will be used to encrypt the packets. Calling it without arguments returns the state of the flag.


Constants
---------

.. data:: sigfox.SIGFOX
          sigfox.FSK

    Sigfox radio mode

.. data:: sigfox.RCZ1
          sigfox.RCZ2
          sigfox.RCZ3
          sigfox.RCZ4

    Sigfox zones


Working with Sigfox sockets
---------------------------

Sigfox sockets are created in the following way::

   import socket
   s = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW)

And they must be created after initializing the Sigfox network card.

Sigfox sockets support the following standard methods from the :class:`socket <.socket>` module:

.. method:: socket.close()

   Usage: ``s.close()``

.. method:: socket.send(bytes)

   Usage: ``s.send(bytes([1, 2, 3]))`` or: ``s.send('Hello')``

.. method:: socket.recv(bufsize)

   Usage: ``s.recv(32)``

.. method:: socket.setsockopt(level, optname, value)

   Set the value of the given socket option. The needed symbolic constants are defined in the
   socket module (SO_* etc.). In the case of Sigfox the values are always an integer. Examples::

      # wait for a downlink after sending the uplink packet
      s.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, True)

      # make the socket uplink only
      s.setsockopt(socket.SOL_SIGFOX, socket.SO_RX, False)

      # use the socket to send a Sigfox Out Of Band message
      s.setsockopt(socket.SOL_SIGFOX, socket.SO_OOB, True)

      # disable Out-Of-Band to use the socket normally
      s.setsockopt(socket.SOL_SIGFOX, socket.SO_OOB, False)

      # select the bit value when sending bit only packets
      s.setsockopt(socket.SOL_SIGFOX, socket.SO_BIT, False)

   Sending a Sigfox packet with a single bit is achieved by sending an empty string, i.e.::

     import socket
     s = socket.socket(socket.AF_SIGFOX, socket.SOCK_RAW)

     # send a 1 bit
     s.setsockopt(socket.SOL_SIGFOX, socket.SO_BIT, True)
     s.send('')

.. method:: socket.settimeout(value)

   Usage: ``s.settimeout(5.0)``

.. method:: socket.setblocking(flag)

   Usage: ``s.setblocking(True)``
