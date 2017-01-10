
LED
-----

By default the heartbeat LED flashes in blue color once every 4s to signal that
the system is alive. This can be overridden through the :mod:`pycom` module::

   >>> import pycom
   >>> pycom.heartbeat(False)
   >>> pycom.rgbled(0xff00)           # turn on the RGB LED in green color

The heartbeat LED is also used to indicate that an error was detected.