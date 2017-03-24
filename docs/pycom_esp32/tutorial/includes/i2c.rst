

I2C
---

The following example receives data from a light sensor using I2C. Sensor used is the BH1750FVI Digital Light Sensor.

::

	import time
	from machine import I2C
	import bh1750fvi

	i2c = I2C(0, I2C.MASTER, baudrate=100000)
	light_sensor = bh1750fvi.BH1750FVI(i2c, addr=i2c.scan()[0])

	while(True):
		data = light_sensor.read()
		print(data)
	    time.sleep(1)

Driver file for the BH1750FVI (place in bh1750fvi.py)

::

	# Simple driver for the BH1750FVI digital light sensor

	class BH1750FVI:
	    MEASUREMENT_TIME = const(120)

	    def __init__(self, i2c, addr=0x23, period=150):
	        self.i2c = i2c
	        self.period = period
	        self.addr = addr
	        self.time = 0
	        self.value = 0
	        self.i2c.writeto(addr, bytes([0x10])) # start continuos 1 Lux readings every 120ms

	    def read(self):
	        self.time += self.period
	        if self.time >= MEASUREMENT_TIME:
	            self.time = 0
	            data = self.i2c.readfrom(self.addr, 2)
	            self.value = (((data[0] << 8) + data[1]) * 1200) // 1000
	        return self.value


Light sensor + LoRa
^^^^^^^^^^^^^^^^^^^^

This is the same code, with added LoRa connectivity, sending the lux value from the lightsensor to another LoPy.

::

	import socket
	import time
	import pycom
	import struct
	from network import LoRa
	from machine import I2C
	import bh1750fvi

	LORA_PKG_FORMAT = "!BH"
	LORA_CONFIRM_FORMAT = "!BB"

	DEVICE_ID = 1

	pycom.heartbeat(False)

	lora = LoRa(mode=LoRa.LORA, tx_iq=True, frequency = 863000000)
	lora_sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
	lora_sock.setblocking(False)

	i2c = I2C(0, I2C.MASTER, baudrate=100000)
	light_sensor = bh1750fvi.BH1750FVI(i2c, addr=i2c.scan()[0])

	while(True):
	    msg = struct.pack(LORA_PKG_FORMAT, DEVICE_ID, light_sensor.read())
	    lora_sock.send(msg)

	    pycom.rgbled(0x150000)

	    wait = 5
	    while (wait > 0):
	        wait = wait - 0.1
	        time.sleep(0.1)
	        recv_data = lora_sock.recv(64)
	        
	        if (len (recv_data) >= 2):
	            status, device_id = struct.unpack(LORA_CONFIRM_FORMAT, recv_data)
	            
	            if (device_id == DEVICE_ID and status == 200):
	                pycom.rgbled(0x001500)
	                wait = 0

	    time.sleep(1)