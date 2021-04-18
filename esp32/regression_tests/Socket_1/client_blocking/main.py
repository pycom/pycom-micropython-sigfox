
import socket
import time
import uerrno
from network import WLAN
import machine


# Configuration of this test
WLAN_NETWORK_SSID = YOUR_SSID
WLAN_NETWORK_PWD  = YOUR_PWD
SERVER_ADDR_TO_CONNECT    = SERVER_IP_ADDRESS
SERVER_PORT_TO_CONNECT    = SERVER_PORT

# Connect to the Network
wlan = WLAN(mode=WLAN.STA)

nets = wlan.scan()
for net in nets:
    if net.ssid == WLAN_NETWORK_SSID:
        print('Network found!')
        wlan.connect(net.ssid, auth=(net.sec, WLAN_NETWORK_PWD), timeout=5000)
        while not wlan.isconnected():
            machine.idle() # save power while waiting
        print('WLAN connection succeeded!')
        print("My IP address: {}".format(wlan.ifconfig()[0]))
        break


# Set up the socket in non-blocking mode
s1 = socket.socket()
s1.setblocking(True)

# Wait signal from the Server side that it is ONLINE
prtf_wait_for_command(PRTF_COMMAND_GO)

print("Connecting to {}:{}".format(SERVER_ADDR_TO_CONNECT, SERVER_PORT_TO_CONNECT))
s1.connect(socket.getaddrinfo(SERVER_ADDR_TO_CONNECT, SERVER_PORT_TO_CONNECT)[0][-1])
s1.send("I am the client and this is my data.")
data_received = s1.recv(100)
print("Data received: {}".format(data_received.decode()))

# Close the socket
s1.close()
# Deinitialize the WLAN module
wlan.deinit()
