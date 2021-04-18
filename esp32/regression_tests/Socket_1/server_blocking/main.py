import usocket
import time
from network import WLAN
import machine

# Configuration of this test
WLAN_NETWORK_SSID = YOUR_SSID
WLAN_NETWORK_PWD  = YOUR_PWD
LISTENING_PORT    = SERVER_PORT

# Handle the client
def client_handle(clientsocket):
    # Receive maxium of 100 bytes from the client
    r = clientsocket.recv(100)

    # If recv() returns with 0 the other end closed the connection
    if len(r) == 0:
        clientsocket.close()
        return
    else:
        # Print out the received data
        print("Data received: {}".format(r.decode()))

    # Sends back our data
    clientsocket.send(str("I am the server and this is my data."))

    # Close the clientsocket
    clientsocket.close()



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

# Set up server socket
serversocket = usocket.socket(usocket.AF_INET, usocket.SOCK_STREAM)
serversocket.setsockopt(usocket.SOL_SOCKET, usocket.SO_REUSEADDR, 1)
serversocket.bind((wlan.ifconfig()[0], LISTENING_PORT))
# Start listening
serversocket.listen(1)
# Indicate to the Client that the Server is ONLINE
prtf_send_command(PRTF_COMMAND_GO)
# Accept the connection of the client
(clientsocket, address) = serversocket.accept()
client_handle(clientsocket)

# Close the serversocket
serversocket.close()
# Deinitalize WLAN module 
wlan.deinit()