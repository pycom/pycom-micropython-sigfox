The MicroPython project
=======================
<p align="center">
  <img src="https://pycom.io/wp-content/uploads/2018/08/fipySide.png" alt="The FiPy"/>
</p>

This is the MicroPython project, which aims to put an implementation
of Python 3.x on microcontrollers and small embedded systems.
You can find the official website at [micropython.org](http://www.micropython.org).

WARNING: this project is in beta stage and is subject to changes of the
code-base, including project-wide name changes and API changes.

MicroPython implements the entire Python 3.4 syntax (including exceptions,
"with", "yield from", etc., and additionally "async" keyword from Python 3.5).
The following core datatypes are provided: str (including basic Unicode
support), bytes, bytearray, tuple, list, dict, set, frozenset, array.array,
collections.namedtuple, classes and instances. Builtin modules include sys,
time, and struct, etc. Select ports have support for `_thread` module
(multithreading). Note that only subset of Python 3.4 functionality
implemented for the data types and modules.

See the repository www.github.com/micropython/pyboard for the MicroPython
board (PyBoard), the officially supported reference electronic circuit board.

The following components are actively maintained by Pycom:
- py/ -- the core Python implementation, including compiler, runtime, and
  core library.
- esp32/ -- a version of MicroPython that runs on the ESP32 based boards from Pycom.
- tests/ -- test framework and test scripts.

Additional components:
- ports/ -- versions of MicroPython that runs on other hardware
- drivers/ -- hardware drivers
- tools/ -- various tools, including the pyboard.py module.
- examples/ -- a few example Python scripts.
- docs/ -- user documentation in Sphinx reStructuredText format.
- mpy-cross/ -- micropython cross-compiler
- extmod/ -- external micropython modules

The subdirectories above may include READMEs with additional info.

"make" is used to build the components, or "gmake" on BSD-based systems.
You will also need bash and Python 3.

The ESP32 version
-----------------

The "esp32" port requires an xtensa gcc compiler, which can be downloaded from
the Espressif website:

- for 64-bit Linux::

    https://dl.espressif.com/dl/xtensa-esp32-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz

- for 32-bit Linux::

    https://dl.espressif.com/dl/xtensa-esp32-elf-gcc8_4_0-esp-2020r3-linux-i686.tar.gz

- for Mac OS:

    https://dl.espressif.com/dl/xtensa-esp32-elf-gcc8_4_0-esp-2020r3-macos.tar.gz


To use it, you will need to update your ``PATH`` environment variable in ``~/.bash_profile`` file. To make ``xtensa-esp32-elf`` available for all terminal sessions, add the following line to your ``~/.bash_profile`` file::

    export PATH=$PATH:$HOME/esp/xtensa-esp32-elf/bin

Alternatively, you may create an alias for the above command. This way you can get the toolchain only when you need it. To do this, add different line to your ``~/.bash_profile`` file::

    alias get_esp32="export PATH=$PATH:$HOME/esp/xtensa-esp32-elf/bin"

Then when you need the toolchain you can type ``get_esp32`` on the command line and the toolchain will be added to your ``PATH``.

You also need the ESP IDF along side this repository in order to build the ESP32 port.
To get it:

    $ git clone --recursive -b idf_v4.1 https://github.com/pycom/pycom-esp-idf.git

After cloning, if you did not specify the --recursive option, make sure to checkout all the submodules:

    $ cd pycom-esp-idf
    $ git submodule update --init
    

``` text
If you updated the repository from a previous revision and/or if switching between branches,<br>
make sure to also update the submodules with the command above.
```

Finally, before building, export the IDF_PATH variable

    $ export IDF_PATH=~/pycom-esp-idf

This repository contains submodules! Clone using the --recursive option:

    $ git clone --recursive https://github.com/pycom/pycom-micropython-sigfox.git
    
Alternatively checkout the modules manually:

    $ cd pycom-micropython-sigfox
    $ git submodule update --init

``` text
If you updated the repository from a previous revision and/or if switching between branches,<br>
make sure to also update the submodules with the command above.
```

Prior to building the main firmware, you need to build mpy-cross

	$ cd mpy-cross && make clean && make && cd ..

By default the firmware is built for the WIPY:

    $ cd esp32
    $ make clean
    $ make
    $ make flash

You can force the firmware to use LittleFS (the default is FatFS if not configured via pycom.bootmgr() or firmware updater):

    $ cd esp32
    $ make clean
    $ make FS=LFS
    $ make flash


By default, both bootloader and application are built. To build them separately:

    $ cd esp32
    $ make clean
    $ make release
    $ make flash

You can change the board type by using the BOARD variable:

    $ cd esp32
    $ make BOARD=GPY clean
    $ make BOARD=GPY
    $ make BOARD=GPY flash

We currently support the following BOARD types:

	WIPY LOPY GPY FIPY LOPY4
	
For OEM modules, please use the following BOARD type:

``` text
W01: WIPY
L01: LOPY
L04: LOPY4
G01: GPY
```

To specify a serial port other than /dev/ttyUSB0, use ESPPORT variable:

    $ # On MacOS
    $ make ESPPORT=/dev/tty.usbmodemPy8eaa911 flash
    $ # On Windows
    $ make ESPPORT=COM3 flash
    $ # On linux
    $ make ESPPORT=/dev/ttyUSB1 flash

To flash at full speed, use ESPSPEED variable:

	$ make ESPSPEED=921600 flash

Make sure that your board is placed into <b>programming mode</b>, otherwise <b>flashing will fail</b>.<br>
All boards except Expansion Board 2.0 will automatically switch into programming mode<br><br>
Expansion Board 2.0 users, please connect ``P2`` to ``GND`` and then reset the board.


To create a release package that can be flashed with the Pycom firmware tool:

    $ cd esp32
    $ make clean
    $ make release
    
To create a release package for all currently supported Pycom boards:

    $ cd esp32
    $ for BOARD in WIPY LOPY GPY FIPY LOPY4; do make BOARD=$BOARD clean && make BOARD=$BOARD release; done

To specify a directory other than the default build/ directory:

    $ cd esp32
    $ make clean
    $ make RELEASE_DIR=~/pycom-packages release
    
To create a release package for all currently supported Pycom boards in a directory other than the default build/ directory:

    $ cd esp32
    $ for BOARD in WIPY LOPY GPY FIPY LOPY4; do make BOARD=$BOARD clean && make BOARD=$BOARD RELEASE_DIR=~/pycom-packages release; done

To inclue a step for copying IDF libs from IDF_PATH specify the following variable in the make command

    COPY_IDF_LIB=1
    
To Buiild the firmware with Pybytes libs use the following make variable

    VARIANT=PYBYTES
    
To Disable RGB Led control use the following make variable

    RGB_LED=disable

## Steps for using Secure Boot and Flash Encryption

For Secure Boot and Flash Encryption please check: https://docs.pycom.io/advance/encryption/
