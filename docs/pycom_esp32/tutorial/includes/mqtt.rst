

MQTT
-----

Basic usage example
^^^^^^^^^^^^^^^^^^^

For more info on this example, see `this topic <https://forum.pycom.io/topic/211/simple-mqtt-tutorial/25>`_ on our forum


Broker used: `MiveMQ <http://www.hivemq.com/demos/websocket-client/>`_ 
Library used: `UMQTT <https://pypi.python.org/pypi/micropython-umqtt.simple>`_ (simple.py file)

The library has bug on line 57. Make sure to change:

::

	self.settimeout(self.timeout)

To

::

	self.sock.settimeout(self.timeout)


Usage example, after uploading simple.py to the board:

:: 

	from network import WLAN
	from simple import MQTTClient
	import machine
	import time

	def settimeout(duration): 
	     pass

	wlan = WLAN(mode=WLAN.STA)
	wlan.antenna(WLAN.EXT_ANT)
	wlan.connect("yourwifinetwork", auth=(WLAN.WPA2, "wifipassword"), timeout=5000)

	while not wlan.isconnected(): 
	     machine.idle()

	print("Connected to Wifi\n")
	client = MQTTClient("joe", "broker.hivemq.com", port=1883)
	client.settimeout = settimeout
	client.connect()

	while True:
	     print("Sending ON")
	     client.publish("/lights", "ON")
	     time.sleep(1)
	     print("Sending OFF")
	     client.publish("/lights", "OFF")
	     time.sleep(1)
