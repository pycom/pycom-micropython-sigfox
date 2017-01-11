.. currentmodule:: machine

class ADC -- analog to digital conversion
=========================================

Usage::

   import machine

   adc = machine.ADC()             # create an ADC object
   apin = adc.channel(pin='P16')   # create an analog pin on P16
   val = apin()                    # read an analog value

Constructors
------------

.. #todo: document the bits parameter

.. class:: ADC(id=0)

   Create an ADC object, that will let you associate a channel
   with a pin.
   For more info check the :ref:`hardware section<hardware>`.

Methods
-------

.. method:: adc.init()

   Enable the ADC block. This method is automatically called on object creation.

.. method:: adc.deinit()

   Disable the ADC block.

.. #todo: original documentation had an id here, but was removed to avoid confusion

.. method:: adc.channel(\*, pin)

   Create an analog pin. ``pin`` is a keyword-only string argument.
   Returns an instance of :class:`ADCChannel`. Example::

      # enable an ADC channel on P16
      apin = adc.channel(pin='P16')

class ADCChannel --- read analog values from internal or external sources
-------------------------------------------------------------------------

ADC channels can be connected to internal points of the MCU or to GPIO pins.
ADC channels are created using the ADC.channel method.

.. comment: the method adcchannel gets modified on runtime by javascript bellow

.. method:: adcchannel()

   Fast method to read the channel value.

.. method:: adcchannel.value()

   Read the channel value.

.. method:: adcchannel.init()

   (Re)init and enable the ADC channel. This method is automatically called on object creation.

.. method:: adcchannel.deinit()

   Disable the ADC channel.

.. raw:: html

    <script>
        el = document.getElementById('machine.adcchannel').getElementsByClassName('descclassname')[0].innerText = "";
    </script>

.. warning::

      ADC pin input range is 0-1.1V (being 4.4V the absolute maximum that they can withstand).