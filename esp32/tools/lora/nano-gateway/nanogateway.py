from network import LoRa
from machine import Timer
import socket
import json


class NanoGateway:
    def __init__(self, url, port):
        self._server = (url, port)
        self.udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

        self.lora = LoRa(mode=LoRa.LORA, frequency=868100000, bandwidth=LoRa.BW_125KHZ,
                         sf=7, preamble=8, coding_rate=LoRa.CODING_4_5)
        self.lora_sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.lora_sock.setblocking(False)

    def make_stat_pkt(self):
        

    def run(self):
        lora_rx_pkt = self.lora_sock.recv(256)
        if lora_rx_pkt:
            udp_pkt = self._make_udp_pkt(lora_rx_pkt, time.ticks_ms())
            self.udp_sock.sendto(self._server, udp_pkt)


PROTOCOL_VERSION = 1

PUSH_DATA = 0
PUSH_ACK = 1
PULL_DATA = 2
PULL_ACK = 4
PULL_RESP = 3

GATEWAY_ID = '1a2b3c4d5e6f7081'

gateway_stat = {"stat": {"time": 0, "lati": 0, "long": 0, "alti": 0, "rxnb": 0, "rxf2": 0, "ackr": 0, "dwnb": 2, "txnb": 2}}
node_packet = {"rxpk": [{"tmst": 0, "chan": 0, "rfch": 0, "freq": 868.1, "stat": 1, "modu": "LORA", "datr": "SF7BW125", "codr": "4/5", "rssi": -20, "lsnr": 5.1, "size": 0, "data": ""}]}

from network import WLAN
from network import LoRa
import os
import binascii
import machine
import json
import time
import errno
import _thread
import socket
from machine import Timer

# initialize LoRa in LORA mode
# more params can also be given, like frequency, tx power and spreading factor
lora = LoRa(mode=LoRa.LORA, frequency=868100000, bandwidth=LoRa.BW_125KHZ, sf=7, preamble=8, coding_rate=LoRa.CODING_4_5)
# create a raw LoRa socket
lora_sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
lora_sock.setblocking(False)

wlan = WLAN(mode=WLAN.STA)
wlan.connect('ONO43DA', auth=(None, '3PM3w7mVCeR1'))

while not wlan.isconnected():
    time.sleep(2)

time.sleep(0.5)
rtc = machine.RTC()
rtc.ntp_sync("pool.ntp.org") # select an appropriate server

server_addr = socket.getaddrinfo('router.eu.thethings.network', 1700)[0][-1]

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.settimeout(0.5)

def make_gateway_stat():
    now = rtc.now()
    gateway_stat["stat"]["time"] = "%d-%02d-%02d %02d:%02d:%02d GMT" % (now[0], now[1], now[2], now[3], now[4], now[5])
    return json.dumps(gateway_stat)

a_lock = _thread.allocate_lock()

def make_node_data(rx_data):
    node_packet["rxpk"][0]["tmst"] = time.ticks_us()
    node_packet["rxpk"][0]["data"] = binascii.b2a_base64(rx_data)[:-1]
    node_packet["rxpk"][0]["size"] = len(rx_data)
    return json.dumps(node_packet)

def push_data(data):
    global sock
    token = os.urandom(2)
    packet = bytes([PROTOCOL_VERSION]) + token + bytes([PUSH_DATA]) + binascii.unhexlify(GATEWAY_ID) + data
    with a_lock:
        try:
            sock.sendto(packet, server_addr)
        except Exception:
            print("PUSH exception")

def pull_data():
    global sock
    token = os.urandom(2)
    packet = bytes([PROTOCOL_VERSION]) + token + bytes([PULL_DATA]) + binascii.unhexlify(GATEWAY_ID)
    with a_lock:
        try:
            sock.sendto(packet, server_addr)
        except Exception:
            print("PULL exception")

push_data(make_gateway_stat())
udp_alarm = Timer.Alarm(handler=lambda f: push_data(make_gateway_stat()), s=60, periodic=True)
udp_alarm2 = Timer.Alarm(handler=lambda f2: pull_data(), s=25, periodic=True)

pkts_up_acked = 0

def send_down_link(data):
    lora_sock.send(data)

def udp_thread(sock_r):
    global pkts_up_acked

    while True:
        try:
            data, src = sock_r.recvfrom(1024)
            _type = data[3]
            if _type == PUSH_ACK:
                pkts_up_acked += 1
                print("Push ack!")
            elif _type == PULL_ACK:
                pkts_up_acked += 1
                print("Pull ack!")
            elif _type == PULL_RESP:
                tx_pk = json.loads(data[4:])
                l_tmst = tx_pk["txpk"]["tmst"]
                t_us = (l_tmst - time.ticks_us() - 30000)
                l_data = tx_pk["txpk"]["data"]
                print("pkt time: ", t_us)
                lora_alarm = Timer.Alarm(handler=lambda f: send_down_link(binascii.a2b_base64(l_data)), us=t_us)
                print("Pull rsp!")
                print(l_data)
        except socket.timeout:
            pass
        except Exception:
            print("UDP recv EXCEPTION")

_thread.start_new_thread(udp_thread, (sock,))

def rx_lora(s):
    # try to receive
    rx = s.recv(128)
    if rx:
        # print("lora rx: " + str(rx, 'ascii'))
        push_data(make_node_data(rx))

import gc

while True:
    if not wlan.isconnected():
        wlan.connect('ONO43DA', auth=(None, '3PM3w7mVCeR1'))
        while not wlan.isconnected():
            time.sleep(2)
    else:
        rx_lora(lora_sock)
        time.sleep(2)
