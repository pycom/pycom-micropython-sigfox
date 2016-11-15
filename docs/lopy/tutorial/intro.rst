Introduction to the LoPy
========================

To get the most out of your LoPy, there are a few basic things to
understand about how it works.

Caring for your LoPy and expansion board
----------------------------------------

  - Be gentle when plugging/unplugging the USB cable.  Whilst the USB connector
    is well soldered and is relatively strong, if it breaks off it can be very
    difficult to fix.

  - Static electricity can shock the components on the LoPy and destroy them.
    If you experience a lot of static electricity in your area (eg dry and cold
    climates), take extra care not to shock the LoPy.  If your LoPy came
    in a ESD bag, then this bag is the best way to store and carry the
    LoPy as it will protect it against static discharges.

As long as you take care of the hardware, you should be okay.  It's almost
impossible to break the software on the LoPy, so feel free to play around
with writing code as much as you like. If the filesystem gets corrupt, see
below on how to reset it. In the worst case you might need to do a safe boot,
which is explained in detail :ref:`here <pycom_boot_modes>`.

Plugging into the expansion board and powering on
-------------------------------------------------

The expansion board can power the LoPy via USB. When inserting the LoPy into the
expansion board, align the RGB LED with the USB connector.

Expansion board hardware guide
------------------------------

The document explaining the hardware details of the expansion board can be found
`here <https://github.com/WiPy/WiPy/blob/master/docs/User_manual_exp_board.pdf>`_.

Powering by an external power source
------------------------------------

The LoPy can be powered by a battery or other external power source.

**Be sure to connect the positive lead of the power supply to VIN, and
ground to GND.**

- When powering via ``VIN``:

   **The input voltage must be between 3.4V and 5.5V.**

- When powering via ``3V3``:

   **The input voltage must be exactly 3V3, ripple free and from a supply capable
   of sourcing at least 500mA of current**

Performing firmware upgrades
----------------------------

For detailed instructions see :ref:`OTA How-To <LoPy_firmware_upgrade>`.
