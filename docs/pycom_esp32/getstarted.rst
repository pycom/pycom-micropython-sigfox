***************************
Quickstart guide
***************************

Let's start with a quick description of what to do after you unpack your brand
new toy. Our aim is not to bug you with complex details about it, but instead
get you as quick as possible to the point you can start doing useful things
with your device.


This chapter will get you unboxed and ready to code in no time. 

- :ref:`Unboxing and the expansion board <unboxing>` Intro to the hardware and how to use the expansion board
- :ref:`Connecting over USB <connecting_over_usb>` How to connect your board
- :ref:`Firmware Upgrades <firmware_upgrades>` Where to download the upload tool
- :ref:`Connecting your board using Pymakr <connecting_using_pymakr>` First steps on using Pymakr with your board
- :ref:`Examples <examples>` Basic code snippets to get you started

If anyting goes wrong, there is a :ref:`Troubleshooting` section that addresses
 the most common issues. In case of any doubts you can always ask questions in 
 our `community forum <http://forum.pycom.io>`_.

.. _unboxing:
Unboxing and the expansion board
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
    
    <object style="margin:0 auto;" width="480" height="385"><param name="movie"
    value="https://www.youtube.com/v/wUxsgls9Ymw"></param><param
    name="allowFullScreen" value="true"></param><param
    name="allowscriptaccess" value="always"></param><embed
    src="http://www.youtube.com/v/wUxsgls9Ymw"
    type="application/x-shockwave-flash" allowscriptaccess="always"
    allowfullscreen="true" width="480"
    height="385"></embed></object>
    
    
.. note::
    Some modules like the LoPy will be big enough to cover the USB connector.
    It is normal as long as you keep the orientation shown.


.. _connecting_over_usb:

Connecting over USB
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

Firmware Upgrades
=================

The next suggested step will be upgrading the firmware, as we are constantly
making improvements and adding new features to it.
Download the upgrader tool :ref:`upgrader tool<firmware_upgrade>`
and follow the instructions on screen.


.. warning::

    Until further notice, :ref:`firmware upgrade<firmware_upgrade>` is required. 

    
After you’re done with the upgrade, you can :ref:`use Pymakr <pymakr>` to upload and run
programs in your device. 

We strongly recommend you to upgrade your firmware to the latest version. Here
are the download links to the update tool:

- `Windows <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=win32&redirect=true>`_.
- `MacOS <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=macos&redirect=true>`_ (10.11 or higher).
- `Linux <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=unix&redirect=true>`_ (requires dialog package).

Please download the appropriate one and follow the instructions on the screen.

.. #todo: add support for people without expansion boards


.. _connecting_using_pymakr:
Connecting your board using Pymakr
==================================

To make it as easy as possible, we developed Pymakr, a tool that will allow you
to connect to and program your Pycom devices. We’re going to use it in this
section to give you a quick taste of how you can work with your device. You can
download Pymakr from `here <https://www.pycom.io/solutions/pymakr/>`_.

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

.. image:: images/pymakr-wifi-reset.png
    :align: center
    :scale: 50 %
    :alt: Pymakr WiFi settings

.. note::
    Pymakr also supports wired connections. Instead of typing the IP address, you 
    can click on the combo box arrow and select the proper serial port from the list. 
    Our boards don’t require any username or password for the serial connection, so you
    can leave those fields empty.

That’s it for the first time configuration. In the lower portion of the screen,
you should see the console, with the connection process taking place. At the
end of it, you’ll get a colored ``>>>`` prompt, indicating that you are connected:

.. image:: images/pymakr-repl.png
    :alt: Pymakr REPL
    :align: center
    :scale: 50 %

.. tip::
    `There is also a video <https://www.youtube.com/embed/bL5nn2lgaZE>`_ that
    explains these steps on macOS (it is similar for other operating systems).


Pycom Console
-------------

To start coding, simply go to the Pycom Console and type your code. Lets try to make the LED light up

.. code:: python

    import pycom # we need this module to control the LED
    pycom.heartbeat(False) # disable the blue blinking
    pycom.rgbled(0x00ff00) # make the LED light up in green color


Change the color by 

.. code:: python

    pycom.rgbled(0xff0000) # now make the LED light up in red color


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


.. _examples:

Examples
========

A few very basic examples to get you started coding.

Blink an LED
------------

.. code:: python
    :name: trafficlight-py

    import pycom
    import time

    pycom.heartbeat(False)

    while True:
        pycom.rgbled(0x007f00) # green
        time.sleep(5)
        pycom.rgbled(0x7f7f00) # yellow
        time.sleep(1.5)
        pycom.rgbled(0x7f0000) # red
        time.sleep(3.5)


WLAN connection
---------------

This code sets up a basic connection to your home router. 

.. code:: python
    nets = wlan.scan()
    for net in nets:
        if net.ssid == 'mywifi':
            print('Network found!')
            wlan.connect(net.ssid, auth=(net.sec, 'mywifikey'), timeout=5000)
            while not wlan.isconnected():
                machine.idle() # save power while waiting
            print('WLAN connection succeeded!')
            break

More advanced WLAN examples, like fixed IP and multiple networks, can be found in the :ref:`Wifi Examples <wlan_step_by_step>` chapter. 

.. #TODO: make link work



See also
========
- :ref:`Connecting without Pymakr <pycom_telnet_repl>`
- :ref:`Troubleshooting`
