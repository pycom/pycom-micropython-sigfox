

2.3 Pymakr
==========

Here are some basic tips on how to further use Pymakr to upload code to your modules. You can download Pymakr `here <https://www.pycom.io/solutions/pymakr/>`_.

You can find the code on github:

- `Pymakr sourcecode <https://github.com/pycom/Pymakr>`_.
- `Pymakr-kitchen build tool <https://github.com/pycom/Pymakr-kitchen>`_.

So far, one plugin has been created for Pymaker. We hope this list will expand in the future!

- `WakaTime plugin for Pymakr <https://github.com/wakatime/eric6-wakatime/>`_.

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

    While the module is busy executing code, Pymakr cannot control it. You can regain control of it by using ctrl-c (or right clicking in the console and pressing Reset) or physically press the reset button.
    If your board is running code at boot time, you might need to boot it in :ref:`safe mode <safeboot>`.


If you just want to test some code on the module without creating a Project, you can create a new file or open an existing one and press the 'run' button. 
Note that the changes you make to your file won't be automatically saved to the device on execution.

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



Connecting your board using Pymakr
----------------------------------

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
these steps on macOS (it is similar for other operating systems):

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
 

Expert interface
----------------

By default, Pymakr is configured in 'lite' interface. In this mode, a lot of features are hidden and only the basic functionality remains. This makes it very user friendly, but after you become familiar with the software, you might want to switch to 'expert' interface to get the most out of Pymakr. 

You can enable expert interface under Settings -> Switch to expert interface. After Pymakr restarts, you'll get access to a few new options:

- Full interface control over tabs and layout
- Control over keyboard shortcuts
- Export/import of preferences
- Preferenes for the editor
- Extra tabs besides the Pycom Console: A local python shell, a task viewer and a basic number converter.
- Bookmarks
- Plugin controls
- Lots of other extra's

To switch back to 'lite' mode, go back to Settings and choose Switch to Lite interface. 