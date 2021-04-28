from network import LoRa
import socket
import time

print("Starting...")

lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868)
s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)

for i in range(10):
    if i == 0:
        print("Notifying the other device to start")
        prtf_send_command(PRTF_COMMAND_START)
    recv = s.recv(4)
    print(recv.decode())
    s.send('Pong')

s.close()