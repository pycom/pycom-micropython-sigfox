from network import LoRa
import socket
import time

# Wait for the other device to setup
prtf_wait_for_command(PRTF_COMMAND_START)

print("Starting...")

lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868  )
s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)

for i in range(10):
    s.send('Ping')
    recv = s.recv(4)
    print(recv.decode())
    time.sleep(1)

s.close()