
ADC
---

See :mod:`ADC`::

    from machine import ADC
    adc = ADC(0)
    adc_c = adc.channel(pin='P13')
    adc_c()
    adc_c.value()
