import time
import binascii
import config
import socket
from network import LoRa

lora = LoRa(mode=LoRa.LORAWAN, adr=True)

DEV_EUI = binascii.unhexlify(config.DEV_EUI.replace(' ', ''))
APP_EUI = binascii.unhexlify('AD A4 DA E3 AC 12 67 6B'.replace(' ', ''))
APP_KEY = binascii.unhexlify('11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'.replace(' ', ''))

lora.join(activation=LoRa.OTAA, auth=(DEV_EUI, APP_EUI, APP_KEY), timeout=0)

# wait until the module has joined the network
while not lora.has_joined():
    time.sleep(2.5)
    print('Not joined yet...')

print('Network joined!')
time.sleep(5.0)

s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)
s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, False)
s.setblocking(False)

while True:
    s.send('Pycom - Objenious')
    time.sleep(30)
