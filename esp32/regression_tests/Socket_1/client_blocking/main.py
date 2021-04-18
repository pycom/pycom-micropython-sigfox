
import socket
import time
import uerrno
from network import WLAN
import machine


# Configuration of this test
WLAN_NETWORK_SSID = "PYCOM_REGR_TEST_AP"
WLAN_NETWORK_PWD  = "regression_test"
SERVER_ADDR       = "192.168.4.107"
SERVER_PORT       = 4567

# Connect to the Network
wlan = WLAN(mode=WLAN.STA)

# Wait for the Access Point to start
prtf_wait_for_command(PRTF_COMMAND_START)

nets = wlan.scan()
for net in nets:
    if net.ssid == WLAN_NETWORK_SSID:
        print('Network found!')
        wlan.connect(net.ssid, auth=(net.sec, WLAN_NETWORK_PWD), timeout=5000)
        while not wlan.isconnected():
            machine.idle() # save power while waiting
        print('WLAN connection succeeded!')
        break


# Set up the socket in non-blocking mode
s1 = socket.socket()
s1.setblocking(True)

# Wait signal from the Server side that it is ONLINE
prtf_wait_for_command(PRTF_COMMAND_GO)

# Connect to the Server
print("Connecting to {}:{}".format(SERVER_ADDR, SERVER_PORT))
s1.connect(socket.getaddrinfo(SERVER_ADDR, SERVER_PORT)[0][-1])
# Exchange data
s1.send("I am the client and this is my data.")
data_received = s1.recv(100)
print("Data received: {}".format(data_received.decode()))

# Close the socket
s1.close()
# Deinitialize the WLAN module
wlan.deinit()
