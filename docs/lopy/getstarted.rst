***************************
Quickstart guide
***************************

Let us start with a quick description of what to do after you unpack your brand new toy. Our aim is not to bog you with complex details about it, but instead get you as quick as possible to the point you can start doing useful things with the device.

.. tip::
    It should take you around 10 minutes to complete this section. Please follow it carefully. In case of any problems, there is a troubleshoot section that addresses the most typical problems. In case of any doubts you can always make questions in our community forum.

.. #todo: add link to Troubleshooting and forum.

Placement on the expansion board
================================

The only way to learn how to use something new is by doing. First, we need to put together the basic pieces:

1. Look for the reset button on your module (located at a corner of the board).
2. Look for the USB connector on your expansion board.
3. Insert the module on the expansion board with the reset button pointing in the same direction as the USB connector.

It's that simple! If you want to confirm your work, here are some pictures:

.. #todo: add pictures in here.

Once you’re sure everything is in place, the fun begins. It is time to turn it on. Just plug it into any powered USB cable (your computer or a battery charger).

In a few seconds, the LED should start blinking every 4 seconds. This means everything is fine. If this doesn’t happen, please disconnect the power supply and re-check.

.. #todo: add short video/animation here.
.. #todo: add support for people without expansion boards

First interaction with your module
==================================


To make it as easy as possible, we developed Pymakr, a tool that will allow you to connect to your Pycom devices. We’re going to use it in this section to give you a quick taste of how you can work with your device. You can download it from `here <https://www.pycom.io/solutions/pymakr/>`_
.

Initial configuration
------------------------

After installing it, you need to take a few seconds to configure it for the first time. Please follow these steps:

1. Connect your computer to the WiFi network named after your board (e.g. ``lopy-wlan-xxxx``, ``wipy-wlan-xxxx``). The password will be ``www.pycom.io``
2. Open Pymakr.
3. In the menu, go to ``Settings > Preferences`` (``Pymakr > Preferences`` on Mac).
4. In the left list look for Pycom Device.
5. For device, type down ``192.168.4.1``. The default username and password are ``micro`` and ``python``, respectively.
6. Click OK

.. #todo: add pymakr video here

That’s it for the first time configuration. In the lower portion of the screen, you should see the console, with the connection process taking place. At the end of it, you’ll get a colored ``>>>`` prompt, indicating that you are connected.

.. #todo: add screenshot here


Let’s get our hands in some coding
-----------------------------------


You can use your board as a calculator. Simply type down: ``6 * 7`` and press enter. You should get the answer to the universe: ``42``.

But math by itself can get boring pretty easily. Let’s get some color to our life. Type down line by line:

.. code:: python

    import pycom # we need this module to control the LED
    pycom.heartbeat(False) # disable the blue blinking
    pycom.rgbled(0x00ff00) # make the LED light up in green color


You can also copy this code, by right clicking at the console, and selecting paste. If you want to see another color:

.. code:: python

    pycom.rgbled(0xff0000) # now make the LED light up in red color

And white:

.. code:: python

    pycom.rgbled(0xffffff) # and now white

It's that simple. Now let’s write a more complex program. First, reset the board by doing right click on the console, and pressing Reset.

.. #todo: add video/animation here

Try to determine what the next code does:

.. code:: python

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

After writing the code, you’ll have to press enter up to three times to tell MicroPython that you’re closing the while loop (standard MicroPython behavior).

You now have a traffic light in your hands! To stop it, just do a right click on the console and press Reset.

This concludes the short intro on how to start playing with your Pycom device. The next suggested step will be upgrading the firmware, as we are constantly making improvements and adding new features to it. `Download the upgrader tool <https://www.pycom.io/support/supportdownloads/>`_ and follow the instructions on screen.

After you’re done with the upgrade, you can use Pymakr to upload and run programs in your device. Go to this section to learn how.

.. #todo: add link to Ralf's section

.. note::

    Just as a last note, Pymakr also supports wired connections. If you go back to Pycom Device in Preferences dialog, instead of typing the IP address, you can click on the combo box arrow and select the proper serial port from the list. Our boards don’t require any username or password for the serial connection, so you can leave those fields empty.


See also
========
- How to upgrade your firmware
- Connecting without Pymakr
- Troubleshooting

.. #todo: add links and check if we can remove the see also from the navigation menu
