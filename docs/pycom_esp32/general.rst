***************************
Tools & Features
***************************

Intro - How to use this section
================


TODO: Write small intro on the contents of this page


Pymakr IDE
================

TODO: Intro to Pymakr, summary of features

Creating a project
------------------

Pymakr has a feature to sync and run your code on your device. This is mostly done using projects. The following steps will get you started.

#. In Pymakr, go to Project > New project.
#. Give it a name and select a folder for your project, either a new of existing one.
#. Now you are ready to place your own code. For fun, lets try again to build a traffic light. Add the following code to the main.py file:

::

    import pycom
    import time
    pycom.heartbeat(False)
    for cycles in range(10): # stop after 10 cycles 
        pycom.rgbled(0x007f00) # green
        time.sleep(5)
        pycom.rgbled(0x7f7f00) # yellow
        time.sleep(1.5)
        pycom.rgbled(0x7f0000) # red
        time.sleep(4)

#. Make sure the connection to your board is open in the Pycom Console
#. Press the sync button on the top toolbar. Any progress will be shown in the console.

Here is the expected result:

.. image:: images/traffic.gif
    :alt: Traffic light
    :align: center
    :scale: 60 %


You now have a traffic light in your hands! To stop it, just do a right click
on the console and press ``Reset`` or use ctrl-c.


.. Warning::

    While the module is busy executing code, Pymakr cannot control it. You can regain control of it by right clicking in the console and pressing Reset, or phisically press the reset button.
    If your board is running code at boot time, you might need to boot it in :ref:`safe mode <safeboot>`.

.. #todo: add link to safeboot


Without creating a project
--------------------------

If you just want to test some code on the module, you can create a new file or open an existing one and press the 'run' button.

.. Warning::
    
    The changes you make to your file won't be automatically saved to the device on execution.


Soft-resets
-----------

Using the checkboxes in the preferences screen, you can choose to do an automatic soft reset every time Pymakr connects to your board and/or when you run your code (using the green 'run' button). This can be useful when you want to make sure the board is in the same state every time you connect or run your code. If you are running an infinite in your main code, keep the soft-reset option disabled.

If you enabe soft-reset on connect, it's useful to add the following check to any wifi-connection scripts in your boot file, so the wifi connection doesn't re-initialize when pymakr connects.

.. code:: python

    if machine.reset_cause() != machine.SOFT_RESET:
        # wifi init code


.. _pycom_telnet_repl:

Telnet REPL
===========

To connect to the Telnet REPL we recommend the usage of Linux or OS X stock telnet, although
other tools like Putty are also compatible. The default credentials are: **user:** ``micro``,
**password:** ``python``.
See :ref:`network.server <network.server>` for info on how to change the defaults.
For instance, on a linux shell (when connected to the LoPy in AP mode)::

   $ telnet 192.168.4.1

.. _pycom_filesystem:

Local file system and FTP access
================================

There is a small internal file system (a drive) on the LoPy, called ``/flash``,
which is stored within the external serial flash memory.  If a micro SD card
is hooked-up and mounted, it will be available as well.

When the LoPy starts up, it always boots from the ``boot.py`` located in the
``/flash`` file system.

The file system is accessible via the native FTP server running in the LoPy.
Open your FTP client of choice and connect to:

**url:** ``ftp://192.168.4.1``, **user:** ``micro``, **password:** ``python``

See :ref:`network.server <network.server>` for info on how to change the defaults.
The recommended clients are: Linux stock FTP (also on OS X), Filezilla and FireFTP.
For example, on a linux terminal::

   $ ftp 192.168.4.1

The FTP server on the LoPy doesn't support active mode, only passive, therefore,
if using the native unix ftp client, just after logging in do::

    ftp> passive

Keep in mind that the FTP server on the LopY only supports one data connection at a time.
If you are using other FTP Clients check thier documentation to set the maximun allowed
connections accordingly.

FileZilla settings
------------------
Do not use the quick connect button, instead, open the site manager and create a new
configuration. In the ``General`` tab make sure that encryption is set to: ``Only use
plain FTP (insecure)``. In the Transfer Settings tab limit the max number of connections
to one, otherwise FileZilla will try to open a second command connection when retrieving
and saving files, and for simplicity and to reduce code size, only one command and one
data connections are possible. Other FTP clients might behave in a similar way.

.. _safeboot:

Boot modes and safe boot
========================

If you power up normally, or press the reset button, the LoPy will boot
into standard mode; the ``boot.py`` file will be executed first, then
``main.py`` will run.

You can override this boot sequence by pulling ``P12`` (``G28``) **up** (connect
it to the 3V3 output pin) during reset. This procedure also allows going
back in time to old firmware versions. The LoPy can hold up to 3 different
firmware versions, which are: the factory firmware plus 2 OTA images.

After reset, if ``P12`` is held high, the heartbeat LED will start flashing
slowly in orange color, if after 3 seconds the pin is still being held high,
the LED will start blinking a bit faster and the LoPy will select the previous
OTA image to boot. If the previous user update is the desired firmware image,
``P12`` must be released before 3 more seconds elapse. If after 3 seconds later,
the pin is still high the factory firmware will be selected, the LED will flash
quickly for 1.5 seconds and the LoPy will proceed to boot.
The firmware selection mechanism is as follows:


**Safe Boot Pin** ``P12`` **released during:**

+-------------------------+-------------------------+----------------------------+
| 1st 3 secs window       | 2nd 3 secs window       | Final 1.5 secs window      |
+=========================+=========================+============================+
| | Safe boot, *latest*   | | Safe boot, *previous* | | Safe boot, the *factory* |
| | firmware is selected  | | user update selected  | | firmware is selected     |
+-------------------------+-------------------------+----------------------------+

On all of the above 3 scenarios, safe boot mode is entered, meaning that
the execution of both ``boot.py`` and ``main.py`` is skipped. This is
useful to recover from crash situations caused by the user scripts. The selection
made during safe boot is not persistent, therefore after the next normal reset
the latest firmware will run again.

The heartbeat LED
=================

By default the heartbeat LED flashes in blue color once every 4s to signal that
the system is alive. This can be overridden through the :mod:`pycom` module::

   >>> import pycom
   >>> pycom.heartbeat(False)
   >>> pycom.rgbled(0xff00)           # turn on the RGB LED in green color

The heartbeat LED is also used to indicate that an error was detected:


... 
.. WLAN default behaviour
.. ----------------------

.. When the LoPy boots with the default factory configuration, it' starts in Access Point mode',
.. and it's ``ssid`` that starts with: ``lopy-wlan`` and ``key: www.pycom.io``.
.. Connect to this network and the LoPy will be reachable at ``192.168.4.1``. In order
.. to gain access to the interactive prompt, open a telnet session to that IP address on
.. the default port (23). You will be asked for credentials:
.. ``login: micro`` and ``password: python``

