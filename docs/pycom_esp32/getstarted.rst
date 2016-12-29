***************************
1. Quickstart guide
***************************

Let's start with a quick description of what to do after you unpack your brand
new toy. Our aim is not to bug you with complex details about it, but instead
get you as quick as possible to the point you can start doing useful things
with your device.


This chapter will get you unboxed and ready to code in no time. 

- :ref:`Unboxing and the expansion board <unboxing>` Intro to the hardware and how to use the expansion board
- :ref:`Connecting over USB <connecting_over_usb>` How to connect your board
- :ref:`Firmware Upgrades <firmware_upgrades>` Where to download the upload tool
- :ref:`Connecting your board using Pymakr <connecting_using_pymakr>` Connecting and coding with Pymakr

If anyting goes wrong, there is a :ref:`Troubleshooting` section that addresses the most common issues. In case of any doubts you can always ask questions in our `community forum <http://forum.pycom.io>`_.

.. _unboxing:

1.1 Unboxing and the expansion board
================================

The only way to learn how to use something new is by doing. First, we need to
put together the basic pieces:

1. Look for the reset button on your module (located at a corner of the board).
2. Look for the USB connector on your expansion board.
3. Insert the module on the expansion board with the reset button pointing in the same direction as the USB connector.

It's that simple! If you want to confirm your work, here's a picture showing
how to place your board properly on the expansion board:

.. image:: images/placement.png
    :alt: Correct placement
    :align: center

If you prefer video tutorials, here is a
`video <https://www.youtube.com/embed/wUxsgls9Ymw>`_ showing these steps.
It applies to all our modules.

.. raw:: html

    <div style="text-align:center;margin:0 auto;">
    <object style="margin:0 auto;" width="480" height="385"><param name="movie"
    value="https://www.youtube.com/v/wUxsgls9Ymw"></param><param
    name="allowFullScreen" value="true"></param><param
    name="allowscriptaccess" value="always"></param><embed
    src="http://www.youtube.com/v/wUxsgls9Ymw"
    type="application/x-shockwave-flash" allowscriptaccess="always"
    allowfullscreen="true" width="480"
    height="385"></embed></object>
    </div>
    
    
.. note::
    Some modules like the LoPy will be big enough to cover the USB connector.
    This is normal as long as you keep the orientation as shown.


.. _connecting_over_usb:

1.2 Connecting over USB
===================

Once you’re sure everything is in place, the fun begins. It is time to turn
your device on. Just plug it into any powered USB cable (your computer or a
battery charger).

In a few seconds, the LED should start blinking every 4 seconds. This means
that everything is fine! If you cannot see the blinking, please disconnect the
power supply and re-check the boards position on the expansion board.

.. image:: images/blinking.gif
    :alt: LED blinking
    :align: center
    :scale: 60 %


.. _firmware_upgrades:

1.3 Firmware Upgrades
=================

The next suggested step will be upgrading the firmware, as we are constantly
making improvements and adding new features to it.
Download the upgrader tool :ref:`upgrader tool<firmware_upgrade>`
and follow the instructions on screen.


We strongly recommend you to upgrade your firmware to the latest version. Here
are the download links to the update tool. Please download the appropriate one for 
your OS and follow the instructions on the screen.

- `Windows <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=win32&redirect=true>`_.
- `MacOS <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=macos&redirect=true>`_ (10.11 or higher).
- `Linux <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=unix&redirect=true>`_ (requires dialog package).

After you’re done with the upgrade, you can :ref:`use Pymakr <pymakr>` to upload and run
programs in your device. 

.. warning::

    Make sure the TX jumper is present on your expansion board, as the jumpers sometimes come loose in the box during transport. Without this jumper, the updater will fail.


.. #todo: add support for people without expansion boards

.. _micropython_intro:

1.4 Micropython Introduction
============================

Our boards work with `Micropython <https://micropython.org/>`_; a Python 3 implementation that is optimised to run on micocontrollers. This allows for much faster and easier development than using C. 

When booting, two files are executed automatically: first boot.py and then main.py. These are placed in the /flash folder on the board. Any other files or libraries can be placed here as well, and included or used from boot.py or main.py. 

The folder structure in /flash looks like the picture below. The files can be managed either using :ref:`FTP <pycom_filesystem>` or using :ref:`Pymakr <pymakr_ide>`.

.. image:: images/wipy-files-ftp.png
    :alt: File structure
    :align: center


.. _connecting_using_pymakr:

1.5 Connecting your board using Pymakr
==================================

To make it as easy as possible, we developed Pymakr, a tool that will allow you
to connect to and program your Pycom devices. We’re going to use it in this
section to give you a quick taste of how you can work with your device. You can
download Pymakr from `here <https://www.pycom.io/solutions/pymakr/>`_.

More extended info on pymakr can be found under Tools & Features in chapter :ref:`2.3 Pymakr <pymakr_ide>`

Initial configuration
---------------------

After installing Pymakr, you need to take a few seconds to configure it for the
first time. Please follow these steps:

    1. Connect your computer to the WiFi network named after your board (e.g. ``lopy-wlan-xxxx``, ``wipy-wlan-xxxx``). The password is ``www.pycom.io``
    2. Open Pymakr.
    3. In the menu, go to ``Settings > Preferences`` (``Pymakr > Preferences`` on macOS).
    4. In the left list look for Pycom Device.
    5. For device, type down ``192.168.4.1``. The default username and password are ``micro`` and ``python``, respectively.
    6. Click OK


.. note::
    Pymakr also supports wired connections. Instead of typing the IP address, you 
    can click on the combo box arrow and select the proper serial port from the list. 
    Our boards don’t require any username or password for the serial connection, so you
    can leave those fields empty.


.. image:: images/pymakr-wifi-reset.png
    :align: center
    :scale: 50 %
    :alt: Pymakr WiFi settings

That’s it for the first time configuration. In the lower portion of the screen,
you should see the console, with the connection process taking place. At the
end of it, you’ll get a colored ``>>>`` prompt, indicating that you are connected:

.. image:: images/pymakr-repl.png
    :alt: Pymakr REPL
    :align: center
    :scale: 100 %

`There is also a video <https://www.youtube.com/embed/bL5nn2lgaZE>`_ that explains 
these steps on macOS (it is similar for other operating systems).


.. raw:: html

    <div style="text-align:center;margin:0 auto;">
    <object style="margin:0 auto;" width="480" height="385"><param name="movie"
    value="https://www.youtube.com/v/bL5nn2lgaZE"></param><param
    name="allowFullScreen" value="true"></param><param
    name="allowscriptaccess" value="always"></param><embed
    src="http://www.youtube.com/v/bL5nn2lgaZE"
    type="application/x-shockwave-flash" allowscriptaccess="always"
    allowfullscreen="true" width="480"
    height="385"></embed></object>
    </div>
    

Pycom Console
-------------

To start coding, simply go to the Pycom Console and type your code. Lets try to make the LED light up.

.. code:: python

    import pycom # we need this module to control the LED
    pycom.heartbeat(False) # disable the blue blinking
    pycom.rgbled(0x00ff00) # make the LED light up in green color


Change the color by adjusting the hex RGB value

.. code:: python

    pycom.rgbled(0xff0000) # now make the LED light up in red color


The console can be used to run any python code, also functions or loops. Simply copy-paste it into the console or type it manually. Note that after writing or pasting any indented code like a function or a while loop, you’ll have to press enter up to three times to tell MicroPython that you’re closing the code (this is standard MicroPython behavior). 


.. image:: images/pymakr-repl-while.png
    :alt: Pymakr REPL while-loop
    :align: center
    :scale: 100 %


Use ``print()`` to output contents of variables to the console for you to read. Returned values from functions will also be displayed if they are not caught in a variable. This will not happen for code running from the main or boot files. Here you need to use ``print()`` to output to the console.

A few pycom-console features you can use:

- ``Input history``: use arrow up and arrow down to scroll through the history
- ``Tab completion``: press tab to auto-complete variables or module names
- ``Stop any running code``: with ctrl-c
- ``Copy/paste code or output``: ctrl-c and ctrl-v (cmd-c and cmd-v for mac)


Creating a project
------------------

Pymakr has a feature to sync and run your code on your device. This is mostly done using projects. The following steps will get you started.

- In Pymakr, go to Project > New project.
- Give it a name and select a folder for your project, either a new of existing one.
- Create two files: main.py and boot.py, if you don't already have those. 

.. note::
    You can also :ref:`use FTP <pycom_filesystem>` to download boot.py and main.py from the board to your project folder, after which you can right-click the project viewer and use the 'add source files' option to add them to your project.

The boot.py file should always have the following code on the top, so we can run our python scripts over serial or telnet:

.. code:: python
    
    from machine import UART
    import os
    uart = UART(0, 115200)
    os.dupterm(uart)


Most users, especially WiPy users, would want a wifi script in the boot.py file. A basic wifi script but also more advanced WLAN examples, like fixed IP and multiple networks, can be found in the :ref:`Wifi Examples <wlan_step_by_step>` chapter. 

Besides the neccesary main.py and boot.py files, you can create any folders and python files or libraries that you want to include in your main file. Pymakr will synchronize all files in the project to the board when using the sync button. 

Without creating a project
--------------------------

If you just want to test some code on the module, you can create a new file or open an existing one and press the 'run' button.


.. Warning::
    
    The changes you make to your file won't be automatically saved to the device on execution.


Coding basics
-------------

For fun, lets try again to build a traffic light. Add the following code to the main.py file:

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

- Make sure the connection to your board is open in the Pycom Console
- Press the sync button on the top toolbar. Any progress will be shown in the console.

Here is the expected result:

.. image:: images/traffic.gif
    :alt: Traffic light
    :align: center
    :scale: 60 %

You now have a traffic light in your hands! To stop a running program, use ctrl-c or do a right click
on the console and press ``Reset``. You can also reboot the board by 
pressing the physical reset button.

.. Warning::
    If your board is running code at boot time, you might need to boot it in :ref:`safe mode <safeboot>`.

