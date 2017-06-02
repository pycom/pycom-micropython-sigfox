Communicate between the LoPy and a Raspberry Pi
-----------------------------------------------

The LoPy is a powerful piece of kit but occasionally you may require a full linux operating system. This example shows you how to communicate between a LoPy and a Raspberry Pi to send LoRaWAN message using the `uart` and `lora` modules.

.. warning::

	If you are using the Raspberry Pi 3 (possibly future versions as well), you may need to tweak some config.txt settings to enable the UART Bus. Please see this post_ from the Raspberry Pi Foundation.


::

	# run script as superuser (e.g. $ sudo python3 sendlora.py)
	import serial

	with serial.Serial('/dev/ttyAMA0', 115200, timeout=10) as ser:
		ser.write(b'send')


.. _post: https://www.raspberrypi.org/forums/viewtopic.php?f=28&t=141195


Below is the code for one of the LoPys to receive data from the Raspberry Pi and send data using raw LoRa (LoRaMAC). You can find out more about raw LoRa in this additional tutorial.

::

	from network import LoRa
	import socket
	import machine
	import time

	# initialize LoRa in LORA mode
	# more params can also be given, like frequency, tx power and spreading factor
	lora = LoRa(mode=LoRa.LORA, tx_power=10, sf=12, frequency=868000000)
	s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
	s.setblocking(True)

	uart1 = UART(1, 115200, bits=8, parity=None, stop=1)
	uart1.init(baudrate=115200, bits=8, parity=None, stop=1)
	uart1.write("UART Connected...")

	while True:
	    if uart1.any():
	        data = uart1.readall()
	        print(data)
	        s.send(data)
	    time.sleep(0.25)

It's that simple! If you want to confirm your work, here's a picture showing
how to place your board properly on the expansion board:

.. image:: rpi.gif
    :alt: Raspberry Pi
    :align: center
