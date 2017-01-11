:mod:`machine` --- functions related to the board
=================================================

.. module:: machine
   :synopsis: functions related to the board

The ``machine`` module contains specific functions related to the board.

Quick usage example
-------------------

    ::

        import machine

        help(machine) # display all members from the machine module
        machine.freq() # get the CPU frequency
        machine.unique_id() # return the 6-byte unique id of the board (the LoPy's WiFi MAC address)


Reset related functions
-----------------------

.. function:: reset()

   Resets the device in a manner similar to pushing the external RESET
   button.

.. function:: reset_cause()

   Get the reset cause. See :ref:`constants <machine_constants>` for the possible return values.

Interrupt related functions
---------------------------

.. only:: port_wipy

  .. function:: disable_irq()

     Disable interrupt requests.
     Returns the previous IRQ state: ``False``/``True`` for disabled/enabled IRQs
     respectively.  This return value can be passed to enable_irq to restore
     the IRQ to its original state.

  .. function:: enable_irq(state=True)

     Enable interrupt requests.
     If ``state`` is ``True`` (the default value) then IRQs are enabled.
     If ``state`` is ``False`` then IRQs are disabled.  The most common use of
     this function is to pass it the value returned by ``disable_irq`` to
     exit a critical section.

.. only:: port_2wipy or port_lopy or port_pycom_esp32

  .. function:: disable_irq()

     Disable interrupt requests.
     Returns and integer representing the previous IRQ state.
     This return value can be passed to enable_irq to restore the IRQ to its original state.

  .. function:: enable_irq([state])

     Enable interrupt requests.
     The most common use of this function is to pass the value returned by ``disable_irq`` to
     exit a critical section. Another options is to enable all interrupts which can be achieved
     by calling the function with no parameters.

Power related functions
-----------------------

.. function:: freq()

    .. only:: not port_wipy

        Returns CPU frequency in hertz.

    .. only:: port_wipy or port_lopy or port_2wipy or port_pycom_esp32

        Returns a tuple of clock frequencies: ``(sysclk,)``
        These correspond to:

        - sysclk: frequency of the CPU

.. only:: port_wipy

    .. function:: idle()

       Gates the clock to the CPU, useful to reduce power consumption at any time during
       short or long periods. Peripherals continue working and execution resumes as soon
       as any interrupt is triggered (on many ports this includes system timer
       interrupt occurring at regular intervals on the order of millisecond).

    .. function:: sleep()

       Stops the CPU and disables all peripherals except for WLAN. Execution is resumed from
       the point where the sleep was requested. For wake up to actually happen, wake sources
       should be configured first.

    .. function:: deepsleep()

       Stops the CPU and all peripherals (including networking interfaces, if any). Execution
       is resumed from the main script, just as with a reset. The reset cause can be checked
       to know that we are coming from ``machine.DEEPSLEEP``. For wake up to actually happen,
       wake sources should be configured first, like ``Pin`` change or ``RTC`` timeout.

.. only:: port_wipy

    .. function:: wake_reason()

        Get the wake reason. See :ref:`constants <machine_constants>` for the possible return values.

Miscellaneous functions
-----------------------

.. only:: port_wipy or port_lopy or port_2wipy or port_pycom_esp32

    .. function:: main(filename)

        Set the filename of the main script to run after boot.py is finished.  If
        this function is not called then the default file main.py will be executed.

        It only makes sense to call this function from within boot.py.

    .. function:: rng()

        Return a 24-bit software generated random number.

.. function:: unique_id()

   Returns a byte string with a unique identifier of a board/SoC. It will vary
   from a board/SoC instance to another, if underlying hardware allows. Length
   varies by hardware (so use substring of a full value if you expect a short
   ID). In some MicroPython ports, ID corresponds to the network MAC address.

   Hint: use :mod:`binascii`.hexlify() to convert the byte string to the much used 
   hexadecimal form.

.. only:: port_wipy

    .. function:: time_pulse_us(pin, pulse_level, timeout_us=1000000)

       Time a pulse on the given `pin`, and return the duration of the pulse in
       microseconds.  The `pulse_level` argument should be 0 to time a low pulse
       or 1 to time a high pulse.

       The function first waits while the pin input is different to the `pulse_level`
       parameter, then times the duration that the pin is equal to `pulse_level`.
       If the pin is already equal to `pulse_level` then timing starts straight away.

       The function will raise an OSError with ETIMEDOUT if either of the waits is
       longer than the given timeout value (which is in microseconds).

.. _machine_constants:

.. only:: port_wipy

  Constants
  ---------

  .. data:: machine.IDLE
  .. data:: machine.SLEEP
  .. data:: machine.DEEPSLEEP

      irq wake values

  .. data:: machine.PWRON_RESET
  .. data:: machine.HARD_RESET
  .. data:: machine.WDT_RESET
  .. data:: machine.DEEPSLEEP_RESET
  .. data:: machine.SOFT_RESET

      reset causes

  .. data:: machine.WLAN_WAKE
  .. data:: machine.PIN_WAKE
  .. data:: machine.RTC_WAKE

      wake reasons

.. only:: port_lopy or port_2wipy or port_pycom_esp32

  Constants
  ---------

  .. data:: machine.PWRON_RESET
            machine.SOFT_RESET

      reset causes


.. only:: port_wipy

  Classes
  -------

  .. toctree::
     :maxdepth: 1

     machine.ADC.rst
     machine.I2C.rst
     machine.Pin.rst
     machine.RTC.rst
     machine.SD.rst
     machine.SPI.rst
     machine.Timer.rst
     machine.UART.rst
     machine.WDT.rst


.. only:: port_lopy or port_2wipy or port_pycom_esp32

  Classes
  -------

  .. raw:: html

    <modify_html name="TOC_1"/>

  .. toctree::
    :maxdepth: 1

    machine.Pin.rst
    machine.UART.rst
    machine.SPI.rst
    machine.Timer.rst
    machine.I2C.rst
    machine.PWM.rst
    machine.ADC.rst
    machine.DAC.rst
    machine.SD.rst

  .. raw:: html

    <script>
        toc = document.getElementsByName('TOC_1')[0].getElementsByTagName('div')[0].getElementsByTagName('ul')[0].getElementsByTagName('li');
        for (i = 0; i < toc.length; i++) {
            if (toc[i].innerText.search(/ADCChannel|PWMChannel/) !== -1) {
                toc[i].remove();
            }
        }
    </script>
