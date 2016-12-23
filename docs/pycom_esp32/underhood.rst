*********************
What’s under the hood
*********************

All members in the current family of Pycom modules are powered by the ESP32, offering:

- 512 Kb available for the user as internal storage, (external SD card support available)
- Up to 128 Kb available for the user
- Hardware floating point unit
- Up to 24 GPIO :class:`Pins <.Pin>`
- 2x :class:`UARTs <.UART>`
- 2x :class:`SPIs <.SPI>`
- :class:`PWM <.PWM>`
- :class:`ADC <.ADC>`
- :class:`DAC <.DAC>`
- :class:`I2C <.I2C>`
- :class:`WiFi <.WLAN>`
- :class:`LoRa <.LoRa>` (only available in the LoPy)
- :class:`Bluetooth <.Bluetooth>`
- :mod:`hashlib <.uhashlib>` MD5, SHA1, SHA256, SHA384 and SHA512 hash algorithms
- :class:`AES encryption <.AES>`
- :mod:`SSL/TLS support <.ussl>`
- :class:`Interrupts` (coming soon)
- 4x :class:`Timers` (coming soon)
- :class:`RTC` (coming soon)
- :class:`SD` (coming soon)


.. #todo: add note in the next comment. Add links in the previous list. To the ones not yet in place, add a link to the “work in progress” section.

.. tip::
    Click in the links of the previous list to get more information about each module.

.. tip::
    If you want to find out how things are connected, visit the :ref:`hardware section<Hardware>`.
