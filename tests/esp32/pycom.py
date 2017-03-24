import pycom

pycom.heartbeat(True)
print (pycom.heartbeat())
pycom.heartbeat(False)
print (pycom.heartbeat())

pycom.rgbled(0x000000)
print ("LED OFF")
pycom.rgbled(0x0F0000)
print ("LED RED")
pycom.rgbled(0x000F00)
print ("LED GREEN")
pycom.rgbled(0x00000F)
print ("LED BLUE")

pycom.heartbeat(True)
