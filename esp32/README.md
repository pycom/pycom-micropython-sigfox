MicroPython port for the ESP32 based boards from Pycom
======================================================

In order to build this project, a copy of the Espressif IDF is required and it's
path must be specified via the ESP_IDF_PATH variable. See the Makefile for details.

The modified Espressif IDF that we use to build this port can be found in:
https://github.com/pycom/pycom-esp-idf

Build instructions
------------------

First build the mpy-cross compiler:

    $ cd ../mpy-cross
    $ make all

After that, build the ESP32 port for one of Pycom boards (first the bootloader, then the app):

    $ cd ../esp32
    $ make BOARD=LOPY -j5 TARGET=boot
    $ make BOARD=LOPY -j5 TARGET=app clean

Flash the board (connect P2 to GND and reset before starting):

    $ make BOARD=LOPY flash

Using frozen modules
--------------------

Place all the python scripts that you'd like to be frozen into the flash memory of the board inside
the 'frozen' folder in the esp32 directory. Then build as indicated before.
