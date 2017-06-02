.. _pytrack_ref:

API Reference
=============

This chapter describes the various libraries which are designed for the Pytrack Board. This includes details about the various methods and classes available for each of the Pytrack's GPS and 3-Axis Accelerometer.

Accelerometer (LIS2HH12)
------------------------

Pytrack has a 3-Axis Accelerometer that provides outputs for XYZ coordinates as well as roll, pitch and yaw.

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


GPS with GLONASS (Quectel L76-L GNSS)
--------------------------------------------

Coming soon.
