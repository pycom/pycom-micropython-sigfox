.. currentmodule:: machine

class SPI -- a master-driven serial protocol
============================================

SPI is a serial protocol that is driven by a master.  At the physical level
there are 3 lines: SCK, MOSI, MISO.

.. only:: port_wipy or port_2wipy or port_lopy or port_pycom_esp32

    See usage model of I2C; SPI is very similar.  Main difference is
    parameters to init the SPI bus::

        from machine import SPI
        spi = SPI(0, mode=SPI.MASTER, baudrate=1000000, polarity=0, phase=0, firstbit=SPI.MSB)

    Only required parameter is mode, must be SPI.MASTER.  Polarity can be 0 or
    1, and is the level the idle clock line sits at.  Phase can be 0 or 1 to
    sample data on the first or second clock edge respectively.


    Quick usage example
    -------------------

    ::

        from machine import SPI

        # configure the SPI master @ 2MHz
        # this uses the SPI default pins for CLK, MOSI and MISO (``P10``, ``P11`` and ``P12``)
        spi = SPI(0, mode=SPI.MASTER, baudrate=2000000, polarity=0, phase=0)
        spi.write(bytes([0x01, 0x02, 0x03, 0x04, 0x05]) # send 5 bytes on the bus
        spi.read(5) # receive 5 bytes on the bus
        rbuf = bytearray(5)
        spi.write_readinto(bytes([0x01, 0x02, 0x03, 0x04, 0x05], rbuf) # send a receive 5 bytes


Constructors
------------

.. only:: port_wipy or port_2wipy or port_lopy or port_pycom_esp32

    .. class:: SPI(id, ...)

       Construct an SPI object on the given bus. ``id`` can be only 0.
       With no additional parameters, the SPI object is created but not
       initialized (it has the settings from the last initialisation of
       the bus, if any).  If extra arguments are given, the bus is initialized.
       See ``init`` for parameters of initialisation.

Methods
-------

.. method:: spi.init(mode, baudrate=1000000, \*, polarity=0, phase=0, bits=8, firstbit=SPI.MSB, pins=(CLK, MOSI, MISO))

   Initialize the SPI bus with the given parameters:

     - ``mode`` must be ``SPI.MASTER``.
     - ``baudrate`` is the SCK clock rate.
     - ``polarity`` can be 0 or 1, and is the level the idle clock line sits at.
     - ``phase`` can be 0 or 1 to sample data on the first or second clock edge
       respectively.
     - ``bits`` is the width of each transfer, accepted values are 8, 16 and 32.
     - ``firstbit`` can be ``SPI.MSB`` only.
     - ``pins`` is an optional tuple with the pins to assign to the SPI bus. If the pins
       argument is not given the default pins will be selected (``P10`` as CLK, ``P11`` as MOSI and ``P12`` as MISO). If pins is passed as ``None`` then no pin assigment will be made.

.. method:: spi.deinit()

   Turn off the SPI bus.

.. method:: spi.write(buf)

    Write the data contained in ``buf``.
    Returns the number of bytes written.

.. method:: spi.read(nbytes, *, write=0x00)

    Read the ``nbytes`` while writing the data specified by ``write``.
    Return the number of bytes read.

.. method:: spi.readinto(buf, *, write=0x00)

    Read into the buffer specified by ``buf`` while writing the data specified by
    ``write``.
    Return the number of bytes read.

.. method:: spi.write_readinto(write_buf, read_buf)

    Write from ``write_buf`` and read into ``read_buf``. Both buffers must have the
    same length.
    Returns the number of bytes written

Constants
---------

.. data:: SPI.MASTER

   for initialising the SPI bus to master

.. data:: SPI.MSB

   set the first bit to be the most significant bit
