MicroPython libraries
=====================

This chapter describes modules (function and class libraries) which are built
into MicroPython. There are a few categories of modules:

* Modules which implement a subset of standard Python functionality and are not
  intended to be extended by the user.
* Modules which implement a subset of Python functionality, with a provision
  for extension by the user (via Python code).
* Modules which implement MicroPython extensions to the Python standard libraries.
* Modules specific to a particular port and thus not portable.

Note about the availability of modules and their contents: This documentation
in general aspires to describe all modules and functions/classes which are
implemented in MicroPython. However, MicroPython is highly configurable, and
each port to a particular board/embedded system makes available only a subset
of MicroPython libraries. For officially supported ports, there is an effort
to either filter out non-applicable items, or mark individual descriptions
with "Availability:" clauses describing which ports provide a given feature.
With that in mind, please still be warned that some functions/classes
in a module (or even the entire module) described in this documentation may be
unavailable in a particular build of MicroPython on a particular board. The
best place to find general information of the availability/non-availability
of a particular feature is the "General Information" section which contains
information pertaining to a specific port.

Beyond the built-in libraries described in this documentation, many more
modules from the Python standard library, as well as further MicroPython
extensions to it, can be found in the `micropython-lib repository
<https://github.com/micropython/micropython-lib>`_.

Pycom Modules
-------------

 .. toctree::
    :maxdepth: 1

    machine.rst
    machine.ADC.rst
    machine.DAC.rst
    machine.I2C.rst
    machine.Pin.rst
    machine.PWM.rst
    machine.RTC.rst
    machine.SPI.rst
    machine.UART.rst
    machine.WDT.rst
    network.rst
    network.WLAN.rst
    network.Server.rst
    machine.Timer.rst
    machine.SD.rst

- :class:`Interrupts` (coming soon)
- :class:`Bluetooth` (coming soon)
- :class:`AES encryption <.AES>`


General libraries
-----------------

The following list contains the standard Python libraries, MicroPython-specific 
libraries and Pycom specific modules that are available on our boards. 

The standard Python libraries have been "micro-ified" to fit in with the philosophy 
of MicroPython. They provide the core functionality of that module and are intended 
to be a drop-in replacement for the standard Python library.


.. only:: not port_unix and not port_pycom_esp32

    The modules are available by their u-name, and also by their non-u-name.  The
    non-u-name can be overridden by a file of that name in your package path.
    For example, ``import json`` will first search for a file ``json.py`` or
    directory ``json`` and load that package if it is found.  If nothing is found,
    it will fallback to loading the built-in ``ujson`` module.

.. only:: port_unix

    .. toctree::
       :maxdepth: 1

       array.rst
       builtins.rst
       cmath.rst
       gc.rst
       math.rst
       select.rst
       sys.rst
       ubinascii.rst
       ucollections.rst
       uhashlib.rst
       uheapq.rst
       uio.rst
       ujson.rst
       uos.rst
       ure.rst
       usocket.rst
       ustruct.rst
       utime.rst
       uzlib.rst

.. only:: port_pyboard

    .. toctree::
       :maxdepth: 1

       array.rst
       builtins.rst
       cmath.rst
       gc.rst
       math.rst
       select.rst
       sys.rst
       ubinascii.rst
       ucollections.rst
       uhashlib.rst
       uheapq.rst
       uio.rst
       ujson.rst
       uos.rst
       ure.rst
       usocket.rst
       ustruct.rst
       utime.rst
       uzlib.rst

.. only:: port_wipy

    .. toctree::
       :maxdepth: 1

       array.rst
       builtins.rst
       gc.rst
       select.rst
       sys.rst
       ubinascii.rst
       ujson.rst
       uos.rst
       ure.rst
       usocket.rst
       ussl.rst
       utime.rst

.. only:: port_lopy or port_2wipy or port_pycom_esp32



    .. toctree::
       :maxdepth: 1

       micropython.rst
       uctypes.rst
       sys.rst
       uos.rst
       array.rst
       cmath.rst
       math.rst
       gc.rst
       ubinascii.rst
       ujson.rst
       ure.rst
       usocket.rst
       select.rst
       utime.rst
       uhashlib.rst
       ussl.rst
       pycom.treading.rst
       pycom.rst
       ucrypto.rst
       builtins.rst


    .. only:: port_pycom_esp32

        .. note::

            Some modules are available by an u-name, and also by their non-u-name.  The
            non-u-name can be overridden by a file of that name in your package path.
            For example, ``import json`` will first search for a file ``json.py`` or
            directory ``json`` and load that package if it is found.  If nothing is found,
            it will fallback to loading the built-in ``ujson`` module.


.. only:: port_esp8266

    .. toctree::
       :maxdepth: 1

       array.rst
       builtins.rst
       gc.rst
       math.rst
       sys.rst
       ubinascii.rst
       ucollections.rst
       uhashlib.rst
       uheapq.rst
       uio.rst
       ujson.rst
       uos.rst
       ure.rst
       usocket.rst
       ussl.rst
       ustruct.rst
       utime.rst
       uzlib.rst

LoPy
----
.. toctree::
   :maxdepth: 1

   network.LORA.rst

.. - :class:`LoRa <.LoRa>`



.. only:: port_pyboard

   Libraries specific to the pyboard
   ---------------------------------

   The following libraries are specific to the pyboard.

   .. toctree::
      :maxdepth: 2

      pyb.rst

.. only:: port_wipy

   Libraries specific to the WiPy
   ---------------------------------

   The following libraries are specific to the WiPy.

   .. toctree::
      :maxdepth: 2

      wipy.rst


.. only:: port_esp8266

   Libraries specific to the ESP8266
   ---------------------------------

   The following libraries are specific to the ESP8266.

   .. toctree::
      :maxdepth: 2

      esp.rst
