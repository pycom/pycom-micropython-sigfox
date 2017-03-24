import time
from network import LoRa
from certification import Compliance

time.sleep(5)

compliance = Compliance(activation=LoRa.OTAA)
compliance.run()
