***************************
1. Quickstart Guide
***************************

Let's start with a quick description of what to do after you unpack your brand
new toy. Our aim is not to bug you with complex details about it, but instead
get you as quick as possible to the point you can start doing useful things
with your board.

It is strongly recommended to update the firmware on your board before coding, this is described in section 1.3 below

This chapter will get you unboxed and ready to code in no time.

- :ref:`Unboxing and the expansion board <unboxing>`
- :ref:`Connecting over USB <connecting_over_usb>`
- :ref:`Firmware Upgrades <firmware_upgrades>`
- :ref:`Micropython introduction <micropython_intro>`
- :ref:`Connecting the board and coding using Pymakr <connecting_using_pymakr>`

If anyting goes wrong, there is a :ref:`Troubleshooting` section that addresses the most common issues. In case of any doubts you can always ask questions in our `community forum <http://forum.pycom.io>`_.

.. _unboxing:

1.1 Unboxing and the Expansion Board
================================

The best way to learn something new is by doing it! Lets get started setting your Pycom board. First, we'll need to
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

To extend the life of your expansion board, please be aware of the following:

  - Be gentle when plugging/unplugging the USB cable.  Whilst the USB connector
    is well soldered and is relatively strong, if it breaks off it can be very
    difficult to fix.

  - Static electricity can shock the components on the board and destroy them.
    If you experience a lot of static electricity in your area (eg dry and cold
    climates), take extra care not to shock the device.  If your device came
    in a ESD bag, then this bag is the best way to store and carry the
    device as it will protect it against static discharges.


Expansion Board Hardware Guide
------------------------------

The document explaining the hardware details of the expansion board can be found
`here <https://github.com/WiPy/WiPy/blob/master/docs/User_manual_exp_board.pdf>`_.

The pinout for the expansion board can be found in chapter :ref:`datasheets`

.. _connecting_over_usb:

1.2 Connecting Over USB
=======================

Once you’re sure everything is in place, the fun begins. It is time to turn
your board on. Just plug it into any powered USB cable (your computer or a
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
=====================

We **strongly recommend** you to upgrade your firmware to the latest version
as we are constantly making improvements and adding new features.

Here are the download links to the update tool. Please download the appropriate
one for your OS and follow the instructions on the screen.

- `Windows <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=win32&redirect=true>`_.
- `MacOS <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=macos&redirect=true>`_ (10.11 or higher).
- `Linux <https://software.pycom.io/findupgrade?product=pycom-firmware-updater&type=all&platform=unix&redirect=true>`_ (requires dialog package).

Previous versions of firmware are available for download on the `Pycom website
<https://www.pycom.io/support/supportdownloads/#firmware>`_.

.. image:: images/firmware-updater-screenshot.png
    :alt: Firmware upgrader
    :align: center
    :scale: 50 %

The instructions given by the updater tool should be followed carefully. The basic
procedure is like this:

- Disconnect your device from the PC.
- Connect wire G23+GND using a jumper cable.
- Connect the board to the USB.
- Run the upgrader
- Remove the G23+GND wire.
- Reboot the device (button or powercycle)

Connecting G23 and GND puts the device in 'update mode'. You won't need this for any
other task than using the firmware upgrader.

After you’re done with the upgrade, you can :ref:`use the Pymakr Plugins <pymakr>` to upload and run
programs in your device.

If you have your Telnet connection or Pymakr Plugin already setup, the version can be  with the
following code:

::

    import os
    os.uname().release

.. warning::

    Make sure the **TX jumper** is present on your expansion board, as the jumpers sometimes
    come loose in the box during transport. Without this jumper, the updater will fail.

.. _micropython_intro:

1.4 Micropython Introduction
============================

Our boards work with `Micropython <https://micropython.org/>`_; a Python 3.5 implementation
that is optimised to run on microcontrollers. This allows for much faster and more simple
development process than using C.

Booting into Micropython
------------------------

When booting, two files are executed automatically: first boot.py and then main.py. These
are placed in the /flash folder on the board. Any other files or libraries can be placed
here as well, and can be included or used from boot.py or main.py.

The folder structure in /flash looks like the picture below. The files can be managed either
using :ref:`FTP <pycom_filesystem>` or using the :ref:`Pymakr Plugin <pymakr_ide>`.

.. image:: images/wipy-files-ftp.png
    :alt: File structure
    :align: center
    :scale: 50 %

Tips & Tricks
-------------

Micropython shares majority of the same syntax as Python 3.5. The intention of this design is to provide compatibility upwards from Micropython to Python 3.5, meaning that code written for Micropython should work in a similar manner in Python 3.5. There are some minor variations and these should taken viewed as implementation differences.

Micropython also has a number of Micropython specific libraries for accessing hardware level features. Specifics relating to those libraries can be found in the Firmware API Reference section of this documentation.

.. note::

	Micropython, unlike C/C++ or Arduino, **does not use braces({})** to indicate blocks of code specified for class and function definitions or flow control. Blocks of code are denoted by line indentation, which is strictly enforced.

	The number of spaces in the indentation is variable but all statements within a block must be indented the same amount.


**Variable Assignment**

As with Python 3.5, variables can be assigned to and referenced. Below is an example of setting a variable equal to a string and then printing it to the console. ::

	variable = "Hello World"
	print(variable)

**Conditional Statements**

Conditional statements allow control over which elements of code run depending on specific cases. The example below shows how a temperature sensor might be implemented in code. ::

	temperature = 15
	target = 10

	if temperature > target:
	    print("Too High!")
	elif temperature < target:
	    print("Too Low!")
	else:
	    print("Just right!")

**Loops (For & While loop)**

Loops are another important feature of any programming language. This allows you to cycle your code and repeat functions/assignments/etc.

*For loops* allow you to control how many times a block of code runs for within a range. ::

	x = 0
	for y in range(0,9):
	    x += 1
	print(x)

*While loops* are similar to For loops, however they allow you to run a loop until a specific conditional is true/false. In this case, the loop checks if x is less than 9 each time the loop passes. ::

	x = 0
	while x < 9:
 	    x += 1
	print(x)

**Functions**

Functions are blocks of code that are referred to by name. Data can be passed into it to be operated on (i.e., the parameters) and can optionally return data (the return value). All data that is passed to a function is explicitly passed.

The function below takes two numbers and adds them together, outputting the result. ::

	def add(number1, number 2):
	    return number1 + number2

	add(1,2) # expect a result of 3


The next function takes an input name and returns a string containing a welcome phrase. ::

	def welcome(name):
	    welcome_phrase = "Hello, " + name + "!"
	    print(welcome_phrase)

	welcome("Alex") # expect "Hello, Alex!"

**Data Structures**

Python has a number of different data structures for storing and manipulating variables. The main difference (regarding data structures) between C and Python is that Python manages memory for you. This means there's no need to declare the sizes of lists, dictionaries, strings, etc. ::

	# lists - a data structure that holds an ordered collection (sequence) of items

	networks = ['lora', 'sigfox', 'wifi', 'bluetooth', 'lte-m']
	print(network[2]) # expect 'wifi'


	# dictionaries - a dictionary is like an address-book where you can find the address or contact details of a person by knowing only his/her name, i.e. keys (names) are associate with values (details)

	address_book = {'Alex':'2604 Crosswind Drive','Joe':'1301 Hillview Drive','Chris':'3236 Goldleaf Lane'}
	print(address_book['Alex']) # expect '2604 Crosswind Drive'


	# tuple - similar to lists but are  immutable, i.e. you cannot modify tuples after instantiation

	pycom_devices = ('wipy', 'lopy', 'sipy', 'gpy', 'fipy')
	print(pycom_devices[0]) # expect 'wipy'



.. note::
	For more Python examples, check out `these tutorials <https://www.tutorialspoint.com/python3/>`_. Be aware of the implementation differences between Micropython and Python 3.5.

.. _connecting_using_pymakr:


1.5 Connecting a Board using Pymakr Plugin
==========================================

To make it as easy as possible we developed a series of tools known as the **Pymakr Plugins**, which allow you
to connect to and program your Pycom devices. These Plugins have been built for a number of text editors and IDEs to allow for users to choose their favourite development enviroment.

Extended info about these Plugins, such as how to use the Pycom console and other features can be found under :ref:`Tools & Features <pymakr_ide>`

.. note::
    If you have any trouble connecting over USB using the Pymakr Plugins, make sure you have the
    correct `FTDI drivers <http://www.ftdichip.com/Drivers/D2XX.htm>`_ installed.

.. warning::

	**Please be aware that Pymakr IDE has been retired** and that plugins for `Atom <https://atom.io/>`_, `Sublime <https://www.sublimetext.com/>`_, `Visual Studio Code <https://code.visualstudio.com>`_ & `PyCharm <https://www.jetbrains.com/pycharm/>`_ are under development, with intention to replace Pymakr. Currently, users are advised to use an FTP client such as FileZilla to upload code/projects to their devices. You can find instructions on how to do this, in the :ref:`tools section <pycom_filesystem>` of the documentation. Please read this `forum <https://forum.pycom.io/topic/635/pymakr-time-of-death-09-02/41>`_ post for more information.

Installing Pymakr Plugin (Atom)
-------------------------------

For beginners, users getting started with MicroPython & Pycom as well as Atom text editor users, we recommend the **Pymakr Plugin for Atom**. This section will help you get started using the `Atom Text Editor <https://atom.io>`_ & `Pymakr Plugin <https://atom.io/packages/pymakr>`_.

.. image:: images/atom-text-editor.png
    :alt: Atom Text Editor
    :align: center
    :scale: 35 %

Please follow these steps to install the Pymakr Plugin:

	1. Ensure that you have `Atom <https://atom.io/>`_ installed and open.
	2. Navigate to the ``Install`` page, via ``Atom > Preferences > Install``
	3. Search for ``Pymakr`` and select the official Pycom Pymakr Plugin.
	4. You should now see and ``Install`` button. Click this to download and install the Pymakr Plugin.
	5. That's it! You've installed the Pymakr Plugin for Atom.


Initial Configuration (Atom)
----------------------------

After installing the Pymakr Plugin, you need to take a few seconds to configure it for the
first time. Please follow these steps:

    1. Connect your computer to the WiFi network named after your board (e.g. ``lopy-wlan-xxxx``, ``wipy-wlan-xxxx``). The password is ``www.pycom.io``
    2. Open Atom and ensure that the Pymakr Plugins are installed.
    3. In the menu, go to ``Atom > Preferences > Packages > Pymakr``.
    4. By default, the Address should be listed as ``192.168.4.1``. If not, change this to ``192.168.4.1``.
    5. The default username and password are ``micro`` and ``python``, respectively.
    6. You settings will be saved automatically.

.. image:: images/pymakr-plugin-settings.png
    :align: center
    :scale: 60 %
    :alt: Pymakr Plugin settings

.. note::
    The Pymakr Plugins also support wired connections (Serial USB). Instead of typing the IP address, you
    can enter the Serial USB address (Mac/Linux) or COM Port (Windows) of your device.
    Our boards don’t require any username or password for the serial connection, so you
    can leave those fields empty. For more information, please visit :ref:`Tools & Features <pymakr_ide>`.

That’s it for the first time configuration. In the lower portion of the screen,
you should see the console, with the connection process taking place. At the
end of it, you’ll get a 'Connecting on 192.168.4.1...' message and a ``>>>`` prompt,
indicating that you are connected:

.. image:: images/pymakr-plugin-repl.png
    :alt: Pymakr Plugin REPL
    :align: center
    :scale: 100 %

Creating a Project
------------------

The Pymakr Plugins have a feature to sync and run your code on your device. This can be used for both uploading code to your device as well as testing out scripts by running them live on the device. The following steps will get you started.

.. image:: images/pymakr-plugin-overview.png
    :alt: Pymakr Plugin Overview
    :align: center
    :scale: 100 %

- In Atom, go to File > Add Project Folder.
- Create a new folder within the prompt and give it a name. Then select `open` to initialise this as a project folder. You may also use an existing folder if you choose.
- Create two files: main.py and boot.py, if you don't already have those.

.. note::
    You can also :ref:`use FTP <pycom_filesystem>` to download boot.py and main.py from the board to your project folder. This is commonly used when copying large numbers of files to a Pycom board.

The boot.py file should always start with following code, so we can run our python scripts over Serial or Telnet. Newer Pycom boards have this code already in the boot.py file.

::

    from machine import UART
    import os
    uart = UART(0, 115200)
    os.dupterm(uart)


Many users, especially the WiPy users, will want a WiFi script in the boot.py file. A basic WiFi script but also more advanced WLAN examples, like fixed IP and multiple networks, can be found in the :ref:`WiFi Examples <wlan_step_by_step>` chapter. The script below connects to your network and prints out your device's local IP address.

::

    from machine import UART
    from network import WLAN
    import os
    uart = UART(0, 115200)
    os.dupterm(uart)

    wlan = WLAN(mode=WLAN.STA)
    wlan.scan()

    wlan.connect(ssid='Your Network SSID', auth=(WLAN.WPA2, 'Your Network Password'))

    while not wlan.isconnected():
        pass

    print(wlan.ifconfig()) # prints out local IP to allow for easy connection via Pymakr Plugin or FTP Client

Besides the neccesary main.py and boot.py files, you can create any folders and python files or libraries that you want to include in your main file. The Pymakr Plugin will synchronize all files in the project to the board when using the Sync button.


.. warning::

    When synchronizing your project to the board, ensure the REPL console is ready. If any programs are running or the board is still booting, synchronization may fail.



Running Your Code
-----------------

If you want to test some code on the module, you can create a new file or open an existing one and then press the ``Run`` button. This will run the code directly onto the Pycom board but it will not upload/sync to the board.

.. Warning::

    The changes you make to your file won't be automatically saved to the board upon restarting or exiting the ``Run`` feature, as the Pycom board will not store this code.


Coding Basics
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
- Press the Sync button at the top of the Pycom Console. Any progress will be shown in the console.

Here is the expected result:

.. image:: images/traffic.gif
    :alt: Traffic light
    :align: center
    :scale: 60 %

You now have a traffic light in your hands! To stop a running program, use ctrl-c or click ``Cancel``
on the console. You can also reboot the board by
pressing the physical reset button.

.. Warning::
    If your board is running code at boot time, you might need to boot it in :ref:`safe mode <safeboot>`.
