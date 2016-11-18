*********************
What’s under the hood
*********************

All members in the current family of Pycom modules are powered by the ESP32, offering:

- 512 Kb available for the user as internal storage, (external SD card support available)
- Up to 128 Kb available for the user
- Hardware floating point unit
- :ref:`GPIO <quickref>`
- :class:`Interrupts <.Interrupts>` (coming soon)
- 4 :class:`Timers <.TIMERS>`  (coming soon)
- 2 :class:`UARTs <.UART>`
- 2 :class:`SPIs <.SPI>`
- :class:`.I2C`
- :class:`WiFI <.WLAN>`
- :class:`RTC <noreference>` (coming soon)
- :class:`.Bluetooth` (coming soon)
- :class:`DES, AES encryption <.DESAES>` (coming soon)
- :class:`SSL/TLS support <.SSLTLS>` (coming soon)
- :class:`.LoRa` (only available in the LoPy)

.. #todo: add note in the next comment. Add links in the previous list. To the ones not yet in place, add a link to the “work in progress” section.

.. tip::
    Click in the links of the previous list to get more information about each module.

.. tip::
    If you want to find out how things are connected, visit the hardware section.

See also
========
.. #todo: add link to hardware section
- Hardware section
