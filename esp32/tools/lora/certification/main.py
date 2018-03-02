from network import LoRa
# from certification import Compliance

compliance = Compliance(region=LoRa.EU868, activation=LoRa.OTAA)
compliance.run()
