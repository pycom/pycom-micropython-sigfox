General information
===================



WLAN default behaviour
----------------------

When the LoPy boots with the default factory configuration, it' starts in Access Point mode',
and it's ``ssid`` that starts with: ``lopy-wlan`` and ``key: www.pycom.io``.
Connect to this network and the LoPy will be reachable at ``192.168.4.1``. In order
to gain access to the interactive prompt, open a telnet session to that IP address on
the default port (23). You will be asked for credentials:
``login: micro`` and ``password: python``

.. _pycom_telnet_repl:

Telnet REPL
-----------

To connect to the Telnet REPL we recommend the usage of Linux or OS X stock telnet, although
other tools like Putty are also compatible. The default credentials are: **user:** ``micro``,
**password:** ``python``.
See :ref:`network.server <network.server>` for info on how to change the defaults.
For instance, on a linux shell (when connected to the LoPy in AP mode)::

   $ telnet 192.168.4.1

.. _pycom_filesystem:

Local file system and FTP access
--------------------------------

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

.. _pycom_firmware_upgrade:

Upgrading the firmware Over The Air
-----------------------------------

OTA software updates can be performed through the FTP server. To do so, upload the ``appimg.bin`` file
to: ``/flash/sys/appimg.bin``. This process should take around 4s. You won't see the file being stored
inside ``/flash/sys/`` because it's actually saved bypassing the user file system, so it
ends up inside the internal **hidden** file system. The update image is signed with a MD5 checksum that the
bootloader uses to verify the integrity of the upgrade. After the file transfer completes, reset
the LoPy by pressing the switch on the board, or by typing::

    >>> import machine
    >>> machine.reset()

It's always recommended to update to the latest software, but make sure to
read the **release notes** before.

In order to check your software version, do::

   >>> import os
   >>> os.uname().release

Upgrading with Pymakr
---------------------

Coming soon.

Upgrading with the LoPy updater stand-alone tool
------------------------------------------------

1. Download the stand-alone tool from: https://www.pycom.io/support/supportdownloads/
2. Boot your LoPy in safe mode.
3. Connect to the ``lopy-wlan`` WLAN network (password: ``www.pycom.io``)
4. Run the stand-alone update tool and follow the on-screen instructions.

.. note::

   The TX jumper on the expansion board needs to be connected for the updater to work.


.. _pycom_boot_modes:

Boot modes and safe boot
------------------------

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
------------------

By default the heartbeat LED flashes in blue color once every 4s to signal that
the system is alive. This can be overridden through the :mod:`pycom` module::

   >>> import pycom
   >>> pycom.heartbeat(False)
   >>> pycom.rgbled(0xff00)           # turn on the RGB LED in green color

The heartbeat LED is also used to indicate that an error was detected:
