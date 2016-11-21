.. currentmodule:: machine

class ADC -- analog to digital conversion
=========================================

Quick usage example
-------------------

      ::

            import machine

            adc = machine.ADC()             # create an ADC object
            apin = adc.channel(pin='P16')   # create an analog pin on P16
            val = apin()                    # read an analog value
            val = apin.value()              # another way to read it

Constructors
------------

.. class:: ADC(id=0)

    Create an ADC object. **Note:** ``id`` must be 0.

Methods
-------

.. method:: ADC.init()

   Enable the ADC block. This method is automatically called on object creation.

.. method:: ADC.deinit()

   Disable the ADC block.

.. method:: ADC.channel(\*, pin)

   Create an analog pin. The pin must be passed as a named parameter.
   Returns an object of type :ref:`ADCChannel <ADCChannel>`. Example::

      apin = adc.channel(pin='P16')

.. _ADCChannel:
class ADCChannel --- read analog values from internal or external sources
=========================================================================

      ADC channels can be connected to internal points of the MCU or to GPIO pins.
      ADC channels are created using the ADC.channel method.

Methods
-------

      .. comment: the method adcchannel gets modified on runtime by javascript bellow

      .. method:: adcchannel()

      Fast method to read the channel value.

      .. method:: adcchannel.value()

      Read the channel value.

      .. method:: adcchannel.init()

      Re-init (and effectively enable) the ADC channel. This method is automatically called on object creation.

      .. method:: adcchannel.deinit()

      Disable the ADC channel.


.. warning:: 

      ADC pin input range is 0-1.4V (being 1.8V the absolute maximum that it 
      can withstand). When GP2, GP3, GP4 or GP5 are remapped to the 
      ADC block, 1.8 V is the maximum. If these pins are used in digital mode, 
      then the maximum allowed input is 3.6V.

.. raw:: html

    <script>
        el = document.getElementById('machine.adcchannel').getElementsByClassName('descclassname')[0].innerText = "";
    </script>      