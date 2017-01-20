.. currentmodule:: machine

class RTC -- real time clock
============================

The RTC is used to keep track of the date
and time.

Example usage::

    rtc = machine.RTC()
    rtc.init((2014, 5, 1, 4, 13, 0, 0, 0))
    print(rtc.now())


Constructors
------------

.. class:: RTC(id=0, ...)

   Create an RTC object. See init for parameters of initialization.

Methods
-------

.. method:: rtc.init(datetime)

   Initialize the RTC. Datetime is a tuple of the form:

      ``(year, month, day[, hour[, minute[, second[, microsecond[, tzinfo]]]]])``

.. method:: rtc.now()

   Get get the current datetime tuple.

.. only:: port_pycom_esp32

    .. method:: rtc.calibration([cal])

        Get or set RTC calibration.

        With no arguments, ``calibration()`` returns the current calibration
        value, which is an integer in the range [-(2^27 - 1) : 2^27 -1].  With one
        argument it sets the RTC calibration for long term counting.

        The RTC counter ticks at 5 MHz. Current crystal has an error of less than 10 ppm (5 minutes a year),
        which is more than acceptable for most applications. Calibration is only needed if you want to achieve
        errors lower than that.

        The units of ``cal`` are in 1/128 of RTC tick.

        Units added will slow down the clock. Conversely, negative values will speed it up.

        Experienced users can see the note bellow on how the RTC keeps the count of time for more information
        on how calibration works.

    .. note::

        Pycom's port of MicroPython for the ESP32 uses the UNIX epoch (1970-01-01).

    .. note::

        Internally, a 64 bit counter is used to keep microseconds, so the time will overflow
        around year 586512 CE.

    .. note::

        The pseudocode for the RTC ISR is something like:

        ::

            # the 5 in the comments come from 5 ticks per us (@ 5 MHz)

            bres_counter = 0

            def rtc_isr():
                global bres_counter
                bres_limit = 128 * 8388605 + cal # 8388605 = 5 * ((2^23 - 1) // 5)
                bres_counter += 128 * 8388607 # 128 * (2^23 - 1)
                while bres_counter >= bres_limit:
                    bres_counter -= bres_limit
                    uptime_microseconds += 1677721 # 8388605 / 5

        ``cal`` is the calibration parameter that can be set using the calibration method.

        The idea was extracted from `here <http://www.romanblack.com/one_sec.htm>`_ , which in turn
        is inspired on the `Bresenham's line algorithm <https://en.wikipedia.org/wiki/Bresenham's_line_algorithm>`_ .


.. only:: not port_pycom_esp32

    .. method:: RTC.deinit()

        Resets the RTC to the time of January 1, 2015 and starts running it again.

    .. method:: RTC.alarm(id, time, /*, repeat=False)

        Set the RTC alarm. Time might be either a millisecond value to program the alarm to
        current time + time_in_ms in the future, or a datetimetuple. If the time passed is in
        milliseconds, repeat can be set to ``True`` to make the alarm periodic.

    .. method:: RTC.alarm_left(alarm_id=0)

        Get the number of milliseconds left before the alarm expires.

    .. method:: RTC.cancel(alarm_id=0)

        Cancel a running alarm.

    .. method:: RTC.irq(\*, trigger, handler=None, wake=machine.IDLE)

        Create an irq object triggered by a real time clock alarm.

            - ``trigger`` must be ``RTC.ALARM0``
            - ``handler`` is the function to be called when the callback is triggered.
            - ``wake`` specifies the sleep mode from where this interrupt can wake
            up the system.

    Constants
    ---------

    .. data:: RTC.ALARM0

        irq trigger source
