Blynk
-----

Blynk is a platform with iOS and Android apps to control
Arduino, Raspberry Pi and the likes over the Internet.
You can easily build graphic interfaces for all your
projects by simply dragging and dropping widgets.
  
`Downloads, docs, tutorials <http://www.blynk.cc>`_ 
`Blynk community <http://community.blynk.cc>`_            
`Facebook <http://www.fb.com/blynkapp>`_            
`Twitter <http://twitter.com/blynk_app>`_            
                              
More info and download of these examples on `github <https://github.com/wipy/wipy/tree/master/examples/blynk>`_

Basic usage
^^^^^^^^^^^
This example shows one of the simplest scripts,
that doesn't define any custom behaviour.
You're still able to do direct pin operations.
In your Blynk App project:

  Add a Gauge widget,  bind it to Analog Pin 5.
  Add a Slider widget, bind it to Digital Pin 25.
  Run the App (green triangle in the upper right corner).

::

	import BlynkLib
	from network import WLAN

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	# start Blynk (this call should never return)
	blynk.run()


Virual read
^^^^^^^^^^^
This example shows how to display custom data on the widget.
In your Blynk App project:

  Add a Value Display widget,
  bind it to Virtual Pin V2,
  set the read frequency to 1 second.
  Run the App (green triangle in the upper right corner).
  
It will automagically call v2_read_handler.
Calling virtual_write updates widget value.
Don't forget to change WIFI_SSID, WIFI_AUTH and BLYNK_AUTH ;)

::

	import BlynkLib
	from network import WLAN
	import time

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	# to register virtual pins first define a handler:
	def v2_read_handler():
	    # this widget will show some time in seconds..
	    blynk.virtual_write(2, time.ticks_ms() // 1000)

	# attach virtual pin 0 to our handler
	blynk.add_virtual_pin(2, read=v2_read_handler)

	# start Blynk (this call should never return)
	blynk.run()


Virtual Write
^^^^^^^^^^^^^
This example shows how to perform custom actions
using data from the widget.

In your Blynk App project:
  Add a Slider widget,
  bind it to Virtual Pin V3.
  Run the App (green triangle in the upper right corner)
  
It will automagically call v3_write_handler.
In the handler, you can use args[0] to get current slider value.

::

	import BlynkLib
	from network import WLAN
	import time

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	# to register virtual pins first define a handler
	def v3_write_handler(value):
	    print('Current slider value: {}'.format(value))

	# attach virtual pin 3 to our handler
	blynk.add_virtual_pin(3, write=v3_write_handler)

	# start Blynk (this call should never return)
	blynk.run()


Tweet notify
^^^^^^^^^^^^

This example shows how to handle a button press and
send Twitter & Push notifications.

In your Blynk App project:
  Add a Button widget, bind it to Virtual Pin V4.
  Add a Twitter widget and connect it to your account.
  Add a Push notification widget.
  Run the App (green triangle in the upper right corner).

::

	import BlynkLib
	from network import WLAN
	import time

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	# to register virtual pins first define a handler
	def v4_write_handler(value):
	    if value: # is the the button is pressed?
	        blynk.notify('You pressed the button and I know it ;)')
	        blynk.tweet('My WiPy project is tweeting using @blynk_app and it’s awesome! #IoT #blynk @wipyio @micropython')

	# attach virtual pin 4 to our handler
	blynk.add_virtual_pin(4, write=v4_write_handler)

	# start Blynk (this call should never return)
	blynk.run()


Terminal
^^^^^^^^

This example shows how to add a custom terminal widget.

In your Blynk App project:
  Add a Terminal widget, bind it to Virtual Pin V3.
  Run the App (green triangle in the upper right corner).


::
		
	import BlynkLib
	from network import WLAN

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	def v3_write_handler(value):
	    # execute the command echo it back
	    blynk.virtual_write(3, 'Command: ' + value + '\n')
	    blynk.virtual_write(3, 'Result: ')
	    try:
	        blynk.virtual_write(3, str(eval(value)))
	    except:
	        try:
	            exec(value)
	        except Exception as e:
	            blynk.virtual_write(3, 'Exception:\n  ' + repr(e))
	    finally:
	        blynk.virtual_write(3, '\n')

	def v3_read_handler(value):
	    pass

	# attach virtual pin 3 to our handlers
	blynk.add_virtual_pin(3, v3_read_handler, v3_write_handler)

	# start Blynk (this call should never return)
	blynk.run()

Terminal repl
^^^^^^^^^^^^^
This example shows how to turn a Terminal widget into
the REPL console.

In your Blynk App project:
  Add a Terminal widget, bind it to Virtual Pin V5.
  Run the App (green triangle in the upper right corner).

::

	import BlynkLib
	from network import WLAN
	import os

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	def hello():
	    print('Welcome!')

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	term = blynk.repl(5)
	os.dupterm(term)

	# start Blynk (this call should never return)
	blynk.run()


User task
^^^^^^^^^

This example shows how to perform periodic actions and
update the widget value on demand.

In your Blynk App project:

  Add a Value Display widget,
  bind it to Virtual Pin V2,
  set reading frequency to 'PUSH'.
  Run the App (green triangle in the upper right corner).

::

	import BlynkLib
	from network import WLAN
	import time

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	# register the task running every 3 sec
	# (period must be a multiple of 50 ms)
	def my_user_task():
	    # do any non-blocking operations
	    print('Action')
	    blynk.virtual_write(2, time.ticks_ms() // 1000)

	blynk.set_user_task(my_user_task, 3000)

	# start Blynk (this call should never return)
	blynk.run()


Simple SSL
^^^^^^^^^^

This example shows how to make a secure connection using SSL.

Before running this example:
  The server certificate must be uploaded to the WiPy. This
  can easily done via FTP. Take the file 'ca.pem' located in
  the blynk examples folder and put it in '/flash/cert/'.
  Similary to firmware updates, certificates go into the internal
  file system, so it won't be visible after being transferred.

In your Blynk App project:
  Add a Gauge widget,  bind it to Analog Pin 5.
  Add a Slider widget, bind it to Digital Pin 25.
  Run the App (green triangle in the upper right corner).

::

	import BlynkLib
	from network import WLAN
	from machine import RTC

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# set the current time (mandatory to validate certificates)
	RTC(datetime=(2015, 10, 16, 11, 30, 0, 0, None))

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk with security enabled
	blynk = BlynkLib.Blynk(BLYNK_AUTH, ssl=True)

	# start Blynk (this call should never return)
	blynk.run()


Sync
^^^^

::

	import BlynkLib
	from network import WLAN

	WIFI_SSID  = 'YOUR_WIFI'
	WIFI_AUTH  = (WLAN.WPA2, 'YOUR_PASS')
	BLYNK_AUTH = 'YOUR_AUTH_TOKEN'

	# connect to WiFi
	wifi = WLAN(mode=WLAN.STA)
	wifi.connect(WIFI_SSID, auth=WIFI_AUTH)
	while not wifi.isconnected():
	    pass

	print('IP address:', wifi.ifconfig()[0])

	# initialize Blynk with security enabled
	blynk = BlynkLib.Blynk(BLYNK_AUTH)

	def blynk_connected():
	    # You can also use blynk.sync_virtual(pin)
	    # to sync a specific virtual pin
	    print("Updating all values from the server...")
	    blynk.sync_all()

	blynk.on_connect(blynk_connected)

	# start Blynk (this call should never return)
	blynk.run()