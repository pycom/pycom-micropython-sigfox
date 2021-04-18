from network import WLAN
from network import Bluetooth
import machine
import time

# Configuration of this test
WLAN_NETWORK_SSID = YOUR_SSID
WLAN_NETWORK_PWD  = YOUR_PWD

wlan = WLAN(mode=WLAN.STA)

nets = wlan.scan()
for net in nets:
    if net.ssid == WLAN_NETWORK_SSID:
        print('Network found!')
        wlan.connect(net.ssid, auth=(net.sec, WLAN_NETWORK_PWD), timeout=5000)
        while not wlan.isconnected():
            machine.idle() # save power while waiting
        print('WLAN connection succeeded!')
        break

print("WLAN connection status: {}".format(wlan.isconnected()))

print("Going to sleep for 1 second...")
machine.sleep(1000, True)
# Wait 5 seconds so the WLAN connection can be established
time.sleep(5)
print("WLAN connection status: {}".format(wlan.isconnected()))

# Deinitialize the WLAN module
wlan.deinit()