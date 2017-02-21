5.2 LoPy
========

Pinout
------

The pinout of the module is available as a `PDF file <../_downloads/lopy_pinout.pdf>`_.


.. image:: images/pinout_lopy_screenshot.png
    :align: center
    :scale: 50 %
    :alt: LoPy Pinout
    :target: ../_downloads/lopy_pinout.pdf


.. warning::

    DO NOT connect anything to Pins ``P5``, ``P6`` and ``P7``, as this pins are used byt the SPI bus that controls the LoRa radio. These pins should be treated as ``NC``. Wiring connections to these pins will cause incorrect behaviour of the LoRa radio.


Specsheets
----------

The specsheet of the LoPy developer version is available as a `PDF file <../_downloads/lopySpecsheetGraffiti.pdf>`_. The specsheet of the **L01** which is the OEM version of the LoPy can be found in the following `link <../_downloads/l01SpecsheetGraffitiOEM.pdf>`_.
