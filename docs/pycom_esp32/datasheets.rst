.. _datasheets:

****************
5. Datasheets
****************


.. include:: datasheets/wipy2.rst

.. include:: datasheets/lopy.rst

.. include:: datasheets/sipy.rst

.. include:: datasheets/expansionboard.rst


5.5 Powering by an external power source
========================================

The modules can be powered by a battery or other external power source.

**Be sure to connect the positive lead of the power supply to VIN, and
ground to GND.**

- When powering via ``VIN``:

   **The input voltage must be between 3.4V and 5.5V.**

- When powering via ``3V3``:

   **The input voltage must be exactly 3V3, ripple free and from a supply capable
   of sourcing at least 500mA of current**


.. warning::

   The GPIO pins of the modules are NOT 5V tolerant, connecting them to voltages higher
   than 3.3V might cause irreparable damage to the board.

.. warning::
    Static electricity can shock the components on the module and destroy them. If you experience a lot of static electricity
    in your area (eg dry and cold climates), take extra care not to shock the module.  If your module came in a ESD bag, then
    this bag is the best way to store and carry the module as it will protect it against static discharges.
