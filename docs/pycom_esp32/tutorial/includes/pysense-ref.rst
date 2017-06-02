.. _pysense_ref:


API Reference
=============

This chapter describes the various libraries which are designed for the Pysense Board. This includes details about the various methods and classes available for each of the Pysense's sensors.

Accelerometer (LIS2HH12)
------------------------

Pysense has a 3-Axis Accelerometer that provides outputs for XYZ coordinates as well as roll, pitch and yaw.

Constructors
^^^^^^^^^^^^

.. class:: LIS2HH12(i2c=None, sda=None, scl=None)

   Create an Accelerometer object, that will let you to return values for position, roll, pitch and yaw. Constructor must be passed an I2C object to successfully construct. For more info check the hardware section.

Methods
^^^^^^^

.. method:: LIS2HH12.read()

   Read the XYZ coordinates from the 3-Axis Accelerometer. Returns a ``tuple`` with the 3 values of position.

.. method:: LIS2HH12.roll()

   Read the current roll from the 3-Axis Accelerometer. Returns a value in degrees.

.. method:: LIS2HH12.pitch()

   Read the current pitch from the 3-Axis Accelerometer. Returns a value in degrees.

.. method:: LIS2HH12.yaw()

   Read the current yaw from the 3-Axis Accelerometer. Returns a value in degrees.


Digital Ambient Light Sensor (LTR-329ALS-01)
--------------------------------------------

Coming soon.

Humidity and Temperature Sensor
-------------------------------

Coming soon.

Barometric Pressure Sensor with Altimeter
-----------------------------------------

Coming soon.
