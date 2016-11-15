**********************************************
:mod:`pycom` -- Pycom boards specific features
**********************************************

.. module:: pycom
   :synopsis: Pycom boards specific features

The ``pycom`` module contains functions to control specific features of the
pycom boards, such as the heartbeat RGB LED.

Functions
---------

.. function:: heartbeat([enable])

   Get or set the state (enabled or disabled) of the heartbeat LED. Accepts and
   returns boolean values (``True`` or ``False``).

.. function:: rgbled(color)

   Set the color of the RGB LED. The color is specified as 24 bit value represeting
   red, green and blue, where the red color is represented by the 8 most significant
   bits. For instance, passign the value 0x00FF00 will light up the LED in a very bright green.
