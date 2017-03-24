.. currentmodule:: machine

class SD -- Secure digital memory card
======================================


.. only:: port_wipy

    The SD card class allows to configure and enable the memory card
    module of the WiPy and automatically mount it as ``/sd`` as part
    of the file system. There are several pin combinations that can be
    used to wire the SD card socket to the WiPy and the pins used can
    be specified in the constructor. Please check the `pinout and alternate functions
    table. <https://raw.githubusercontent.com/wipy/wipy/master/docs/PinOUT.png>`_ for
    more info regarding the pins which can be remapped to be used with a SD card.

    Example usage::

        from machine import SD
        import os
        # clk cmd and dat0 pins must be passed along with
        # their respective alternate functions
        sd = machine.SD(pins=('GP10', 'GP11', 'GP15'))
        os.mount(sd, '/sd')
        # do normal file operations

    Constructors
    ------------

    .. class:: SD(id,... )

       Create a SD card object. See ``init()`` for parameters if initialization.

    Methods
    -------

    .. method:: SD.init(id=0, pins=('GP10', 'GP11', 'GP15'))

       Enable the SD card. In order to initalize the card, give it a 3-tuple:
       ``(clk_pin, cmd_pin, dat0_pin)``.

    .. method:: SD.deinit()

       Disable the SD card.

.. only:: port_pycom_esp32

    The SD card class allows to configure and enable the memory card module of your Pycom module and automatically mount it as ``/sd`` as part of the file system. There is a single pin combination that can be used for the SD card, and the current implementation only works in 1-bit mode. The pin connections are as follows:

    ``P8: DAT0, P23: SCLK and P4: CMD`` no external pull-up resistors are needed.

    If you have one of the Pycom expansion boards, then simply insert the card into
    the micro SD socket and run your script.

    .. note::

        Make sure your SD card is formatted either as FAT16 or FAT32.

    Example usage::

        from machine import SD
        import os

        sd = SD()
        os.mount(sd, '/sd')

        # check the content
        os.listdir('/sd')

        # try some standard file operations
        f = open('/sd/test.txt', 'w')
        f.write('Testing SD card write operations')
        f.close()
        f = open('/sd/test.txt', 'r')
        f.readall()
        f.close()

    Constructors
    ------------

    .. class:: SD(id, ...)

       Create a SD card object. See ``init()`` for parameters if initialization.

    Methods
    -------

    .. method:: sd.init(id=0)

       Enable the SD card.

    .. method:: sd.deinit()

       Disable the SD card.
