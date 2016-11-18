.. _quickref_:

Threading
---------

::

    import _thread
    import time

    def th_func(delay, id):
        while True:
            time.sleep(delay)
            print('Running thread %d' % id)

    for i in range(2):
        _thread.start_new_thread(th_func, (i + 1, i))

PWM
---

See :mod:`machine.PWM`::

    from machine import PWM
    pwm = PWM(0, frequency=5000)  # use PWM timer 0, with a frequency of 50KHz
    # create pwm channel on pin P12 with a duty cycle of 50%
    pwm_c = pwm.channel(0, pin='P12', duty_cycle=0.5)
    pwm_c.duty_cycle(0.3) # change the duty cycle to 30%


ADC
---

See :mod:`ADC`::

    from machine import ADC
    adc = ADC(0)
    adc_c = adc.channel(pin='P13')
    adc_c()
    adc_c.value()
