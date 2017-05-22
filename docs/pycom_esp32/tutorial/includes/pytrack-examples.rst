.. _pytrack_examples:

Examples
--------

3-Axis Accelerometer
====================

::

 from machine import I2C
 from lis2hh12 import LIS2HH12
 import time

 i2c = I2C(0, baudrate=100000, pins=('P22', 'P21'))      # Initialize the I2C bus

 acc = LIS2HH12(i2c=i2c)

 while True:
     print('----------------------------------')
     print('X, Y, Z:', acc.read())
     print('Roll:', acc.roll())
     print('Pitch:', acc.pitch())
     print('Yaw:', acc.yaw())
     time.sleep(1)

Digital Ambient Light Sensor
============================

Coming soon.

Humidity and Temperature Sensor
===============================

Coming soon.

Barometric Pressure Sensor with Altimeter
=========================================

Coming soon.
