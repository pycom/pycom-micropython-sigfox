
PIR Sensor
----------

This code reads PIR sensor triggers from `this simple PIR sensor <https://www.kiwi-electronics.nl/PIR-Motion-Sensor>`_. 

::

	import time
	from machine import Pin

	#flags
	pir_triggered = False

	#callbacks
	def pirTriggered(pin):
	    global pir_triggered
	    pir_triggered = True

	pir = Pin('GP4',mode=Pin.IN,pull=Pin.PULL_UP)
	pir.irq(trigger=Pin.IRQ_RISING, handler=pirTriggered)

	# main loop
	print("Starting main loop")
	while True:
	    time.sleep_ms(500)
	    if pir_triggered:
	        pir_triggered = False
	        print("Sensor triggered")


