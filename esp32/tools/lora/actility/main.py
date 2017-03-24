import time
from network import LoRa
from actility import Actility

time.sleep(5)

actility = Actility(activation=LoRa.OTAA, adr=True)   # actility = Actility(activation=LoRa.ABP, adr=False)
actility.run()
