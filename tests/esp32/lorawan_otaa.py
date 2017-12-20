import binascii
import socket
import errno
import time
import os

# only execute this test on the LoPy
if os.uname().sysname != 'LoPy' and os.uname().sysname != 'FiPy':
    print("SKIP")
    import sys
    sys.exit()
else:
    from network import LoRa

print('Starting LoRaWAN OTAA test')

def otaa_join(lora):
    dev_eui = binascii.unhexlify('11 22 33 44 55 66 77 88'.replace(' ',''))
    app_eui = binascii.unhexlify('AD A4 DA E3 AC 12 67 6B'.replace(' ',''))
    app_key = binascii.unhexlify('11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'.replace(' ',''))

    lora.join(activation=LoRa.OTAA, auth=(dev_eui, app_eui, app_key), timeout=0)

    time.sleep(0.5)

    # wait until the module has joined the network
    join_wait = 0
    while not lora.has_joined() and join_wait < 10:
        time.sleep(5)
        join_wait += 1
        if join_wait < 2:
            print('Waiting to join')

    if lora.has_joined():
        print('Network joined!')
    else:
        raise OSError('LoRa join failed')

lora = LoRa(mode=LoRa.LORAWAN)

otaa_join(lora)

s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)
s.bind(1)

print('Sending...')

# try all the datarates from 0 to 6 and toggle confirmed and non confirmed messages
for i in range(7):
    s.setsockopt(socket.SOL_LORA, socket.SO_DR, i)
    s.setsockopt(socket.SOL_LORA, socket.SO_CONFIRMED, (i % 2) == 0)
    print(s.send("Sending pk #%d" % i))
    time.sleep(0.5)
    print(s.recv(64).decode('ascii'))
    print(lora.events() == LoRa.RX_PACKET_EVENT | LoRa.TX_PACKET_EVENT)
    print(lora.events() == 0)

s.close()

# join again
otaa_join(lora)

print(lora.mac() == bytes(0xFF for i in range(8)))

s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)
s.setblocking(False)
s.bind(2)

# try all the datarates from 0 to 6 and toggle confirmed and non confirmed messages
for i in range(7):
    print(s.send("Sending pk #%d" % i))
    time.sleep(0.5)
    # the packet should not have been received yet as it's non-blocking
    print(lora.events() == LoRa.TX_PACKET_EVENT)
    print(lora.events() == 0)
    time.sleep(2)

lora.init(mode=LoRa.LORAWAN, public=True, adr=False)

time.sleep(0.5)

try:
    s.send('123')
except Exception as e:
    if e.errno == errno.ENETDOWN:
        print('Exception')
    else:
        print("ENETDOWN Exception not thrown")

otaa_join(lora)

def lora_cb_handler(lora):
    global s
    print(type(lora))
    try:
        events = lora.events()
        if events & LoRa.TX_PACKET_EVENT:
            print("Packet sent")
        if events & LoRa.RX_PACKET_EVENT:
            print("Packet received")
            print(s.recv(64))
    except Exception:
        print('Exception')

cb = lora.callback(handler=lora_cb_handler, trigger=LoRa.TX_PACKET_EVENT | LoRa.RX_PACKET_EVENT)
s.setblocking(True)
for i in range(2):
    print(s.send("Sending pk #%d" % i))
    time.sleep(0.5)

lst = (lora, s)

cb = lora.callback(handler=lora_cb_handler, trigger=LoRa.TX_PACKET_EVENT | LoRa.RX_PACKET_EVENT, arg=lst)
s.setblocking(True)
for i in range(2):
    print(s.send("Sending pk #%d" % i))
    time.sleep(0.5)

print(lora.stats().rx_timestamp > 0)
print(lora.stats().rssi < 0)
print(lora.stats().snr > 0)
print(lora.stats().sftx >= 0)
print(lora.stats().sfrx >= 0)
print(lora.stats().tx_trials >= 1)
