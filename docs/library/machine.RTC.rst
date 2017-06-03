.. currentmodule:: machine

class RTC -- real time clock
============================

The RTC is used to keep track of the date
and time.

Example usage::

    from machine import RTC

    rtc = RTC()
    rtc.init((2014, 5, 1, 4, 13, 0, 0, 0))
    print(rtc.now())


Constructors
------------

.. class:: RTC(id=0, ...)

   Create an RTC object. See init for parameters of initialization.::


      # id of the RTC may be set if multiple are connected. Defaults to id = 0.
      rtc = RTC(id=0)


Methods
-------

.. method:: rtc.init(datetime=None, source=RTC.INTERNAL_RC)

   Initialize the RTC. The arguments are:

    - ``datetime`` when passed it sets the current time.
      It is a tuple of the form: ``(year, month, day[, hour[, minute[, second[, microsecond[, tzinfo]]]]])``.
    - ``source`` selects the oscillator that drives the RTC. The options are ``RTC.INTERNAL_RC`` and ``RTC.XTAL_32KHZ``

   For example: ::

      # for 2nd of February 2017 at 10:30am (TZ 0)
      rtc.init((2017, 2, 28, 10, 30, 0, 0, 0))

   .. note::
      
      tzinfo is ignored by this method. Use ``time.timezone`` to achieve similar results.

.. method:: rtc.now()

   Get get the current datetime tuple.::

      # returns datetime tuple
      rtc.now()

.. only:: port_pycom_esp32

    .. method:: rtc.ntp_sync(server, \*, update_period=3600)

        Set up automatic fetch and update the time using NTP (SNTP).

            - ``server`` is the URL of the NTP server. Can be set to ``None`` to disable the periodic updates.
            - ``update_period`` is the number of **seconds** between updates. Shortest period is 15 seconds.

        Can be used like: ::

            
            rtc.ntp_sync("pool.ntp.org") # this is an example. You can select a more specific server according to your geographical location


    Constants
    ---------

    .. data:: RTC.INTERNAL_RC
              RTC.XTAL_32KHZ

        clock source


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
