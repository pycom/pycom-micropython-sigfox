#With this amount of Alarms this test only makes sense to run on device with smaller RAM

import _thread
import time
from machine import Timer
import gc

def thread_function():
    print("Thread is executing...")
    return 0

def alarm_cb(self):
    _thread.start_new_thread(thread_function, ())

#Need to call gc.collect() because when this test is executing usually not enough free memory exists as earlier tests have used it
gc.collect()

Timer.Alarm(handler=alarm_cb, ms=1000,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1100,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1200,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1300,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1400,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1500,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1600,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1700,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1800,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=1900,  periodic=False)
Timer.Alarm(handler=alarm_cb, ms=2000,  periodic=False)
# The user thread created by this "alarm_cbk" would fail, if others had not been freed up already
Timer.Alarm(handler=alarm_cb, ms=2100,  periodic=False)

print("Starting while loop...")
timer = time.ticks_ms()
while time.ticks_ms() < timer + 3000: #runs for 3 seconds
    pass    # If the tasks started by the Alarm callback are not terminated immediatelly the device would run out of memory
            # as the Idle Task does not have the chance to run and free the resources of the terminated tasks before new ones created
            # In this case exception is raised by the last "alarm_cb" because it is not able to start the new thread
print("While loop has finished without exceptions!")