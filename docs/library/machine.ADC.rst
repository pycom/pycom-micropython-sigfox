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

   Create an ADC object, that will let you associate a channel with a pin.
   For more info check the :ref:`hardware section<hardware>`.

Methods
-------

.. method:: adc.init(\*, bits=12)

   Enable the ADC block. This method is automatically called on object creation.

      - ``bits`` can take values between 9 and 12 and selects the number of bits of resolution of the ADC block.
.. method:: adc.deinit()

   Disable the ADC block.

.. method:: adc.channel(id=0, \*, pin, attn=ADC.ATTN_0DB)

   Create an analog pin.

      - ``id`` is a keyword-only string argument. Valid pins are P13 to P20.
      - ``pin`` is a keyword-only string argument. Valid pins are P13 to P20.
      - ``attn`` is the attenuation level. The supported values are: ``ADC.ATTN_0DB``, ``ADC.ATTN_2_5DB``, ``ADC.ATTN_6DB``, ``ADC.ATTN_11DB``

   Returns an instance of :class:`ADCChannel`. Example::

      # enable an ADC channel on P16
      apin = adc.channel(pin='P16')

Constants
---------

.. data:: ADC.ATTN_0DB
          ADC.ATTN_2_5DB
          ADC.ATTN_6DB
          ADC.ATTN_11DB

   ADC channel attenuation values


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

      ADC pin input range is 0-1.1V. This maximum value can be increased up to 3.3V using the highest attenuation of 11dB.
      DO NOT exceed the maximum of 3.3V in order to avoid damaging the device.
