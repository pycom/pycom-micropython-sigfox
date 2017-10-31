from network import LoRa
from network import WLAN
import socket
import time


wlan = WLAN(mode=WLAN.AP, ssid='pycom-test-ap')

lora = LoRa(mode=LoRa.LORA, frequency=868000000, public=False)

s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)

while True:
    s.send('Pycom-868')
    time.sleep(0.1)
    lora = LoRa(mode=LoRa.LORA, frequency=434000000, public=False)
    time.sleep(0.05)
    s.send('Pycom-434')
    time.sleep(0.1)
    lora = LoRa(mode=LoRa.LORA, frequency=868000000, public=False)
    time.sleep(0.05)
