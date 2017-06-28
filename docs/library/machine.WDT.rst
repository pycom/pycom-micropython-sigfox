.. currentmodule:: machine

class WDT -- watchdog timer
===========================

.. only:: not port_pycom_esp32

    The WDT is used to restart the system when the application crashes and ends
    up into a non recoverable state. Once started it cannot be stopped or
    reconfigured in any way. After enabling, the application must "feed" the
    watchdog periodically to prevent it from expiring and resetting the system.

.. only:: port_pycom_esp32

    The WDT is used to restart the system when the application crashes and ends
    up into a non recoverable state. After enabling, the application must "feed"
    the watchdog periodically to prevent it from expiring and resetting the system.

Example usage::

    from machine import WDT
    wdt = WDT(timeout=2000)  # enable it with a timeout of 2 seconds
    wdt.feed()

Constructors
------------

.. class:: WDT(id=0, timeout)

   Create a WDT object and start it. The ``id`` can only be 0.
   See the ``init`` method for the parameters of initialisation.

Methods
-------

.. class:: init(timeout)

   Initializes the watchdog timer. The timeout must be given in milliseconds.
   Once it is running the WDT cannot be stopped but the timeout can be re-configured
   at any point in time.

.. method:: wdt.feed()

   Feed the WDT to prevent it from resetting the system. The application
   should place this call in a sensible place ensuring that the WDT is
   only fed after verifying that everything is functioning correctly.
