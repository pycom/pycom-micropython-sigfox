.. currentmodule:: machine

class DAC -- digital to analog conversion
=========================================
The DAC is used to output analog values (a specific voltage) on pin P22 or pin P21.
The voltage will be between 0 and 3.3V.

Usage::

   import machine

   dac = machine.DAC('P22')        # create a DAC object
   dac.write(0.5)                  # set output to 50%

   dac_tone = machine.DAC('P21')   # create a DAC object
   dac_tone.tone(1000, 0)          # set tone output to 1kHz

Constructors
------------

.. class:: DAC(pin)

   Create a DAC object, that will let you associate a channel with a pin.
   ``pin`` can be a string argument.

Methods
-------

.. method:: dac.init()

   Enable the DAC block. This method is automatically called on object creation.

.. method:: dac.deinit()

   Disable the DAC block.

.. method:: dac.write(value)

   Set the DC level for a DAC pin. ``value`` is a float argument, with values between 0 and 1.

.. method:: dac.tone(frequency, amplitude)

  Sets up tone signal to the specified ``frequency`` at  ``amplitude`` scale.
  ``frequency`` can be from 125Hz to 20kHz.
  ``amplitude`` is an integer specifying the tone amplitude to write the DAC pin.
  Amplitude value represents: 0 is 0dB, 1 is 6dB, 2 is 12dB, 3 is 18dB.
