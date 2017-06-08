2.8 Updating Pysense and Pytrack firmware
=========================================

The firmware of both Pysense and Pytrack can be updated via the USB port using DFU-util.

The latest firmware is version 2. The DFU file can be downloaded from the links below:

    - `Pytrack DFU <https://software.pycom.io/downloads/pytrack_0.0.2.dfu>`_
    - `Pysense DFU <https://software.pycom.io/downloads/pysense_0.0.2.dfu>`_

Installing DFU-util
-------------------

- **Mac OS**: If you have `homebrew <http://brew.sh/>`_ installed::

    brew install dfu-util

If you have `MacPorts <https://www.macports.org/>`_ installed::

    port install libusb dfu-util

- **Linux**: On Ubuntu and Debian run::

    sudo apt-get install dfu-util

Fedora::

    sudo yum install dfu-util

Arch::

    sudo pacman -Sy dfu-util


Using DFU-util with Pytrack and Pysense
---------------------------------------

In order to put Pyrack or Pysense in DFU mode the push button on the board must be pressed before powering the board.
First press the button, keep it hold, then plug-in the USB port and wait 1 second before releasing it. After this you
will have aproximately 7 seconds to run the DFU-util.

- On Mac OS and linux::

    dfu-util -D firmware_file.dfu

You should see a similar output to the one belwo on your terminal window::

    dfu-util 0.8

    Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
    Copyright 2010-2014 Tormod Volden and Stefan Schmidt
    This program is Free Software and has ABSOLUTELY NO WARRANTY
    Please report bugs to dfu-util@lists.gnumonks.org

    Match vendor ID from file: 04d8
    Match product ID from file: f014
    Deducing device DFU version from functional descriptor length
    Opening DFU capable USB device...
    ID 04d8:f014
    Run-time device DFU version 0100
    Claiming USB DFU Runtime Interface...
    Determining device status: state = dfuIDLE, status = 0
    Claiming USB DFU Interface...
    Setting Alternate Setting #0 ...
    Determining device status: state = dfuIDLE, status = 0
    dfuIDLE, continuing
    DFU mode device DFU version 0100
    Device returned transfer size 64
    Copying data from PC to DFU device
    Download	[=========================] 100%        16384 bytes
    Download done.
    state(2) = dfuIDLE, status(0) = No error condition is present
    Done!
