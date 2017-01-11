
Timers
-------

Detailed information about this class can be found in :class:`Timer <.Timer>`.

Chronometer
^^^^^^^^^^^

The Chronometer can be used to measure how much time has elapsed in your code. The following example uses it as a simple stopwatch.

::

	from machine import Timer
	import time

	chrono = Timer.Chrono()

	chrono.start()
	time.sleep(1.25) # simulate the first lap took 1.25 seconds
	lap = chrono.read() # read elapsed time without stopping
	time.sleep(1.5)
	chrono.stop()
	total = chrono.read()

	print()
	print("\nthe racer took %f seconds to finish the race" % total)
	print("  %f seconds in the first lap" % lap)
	print("  %f seconds in the last lap" % (total - lap))


Alarm
^^^^^
The Alarm can be used to get interrupts at a specific interval. The following code executes a callback every second for 10 seconds.

::

	from machine import Timer

	class Clock:

	    def __init__(self):
	        self.seconds = 0
	        self.__alarm = Timer.Alarm(self._seconds_handler, 1, periodic=True)

	    def _seconds_handler(self, alarm):
	        self.seconds += 1
	        print("%02d seconds have passed" % self.seconds)
	        if self.seconds == 10:
	            alarm.callback(None) # stop counting after 10 seconds

	clock = Clock()

.. note::
	There are no restrictions to what you can do in an interrupt. It's even possible to do network requests. But keep in mind that interrupts hare handled sequentially, so it's good practice to keep them short. More information can be found in chapter :ref:`2.6 Interrupt handling <pycom_interrupt_handling>`.
