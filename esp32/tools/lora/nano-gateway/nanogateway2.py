from network import WLAN
from network import LoRa
from machine import Timer
import os
import binascii
import machine
import json
import time
import errno
import _thread
import socket


PROTOCOL_VERSION = const(2)

PUSH_DATA = const(0)
PUSH_ACK = const(1)
PULL_DATA = const(2)
PULL_ACK = const(4)
PULL_RESP = const(3)


gateway_stat = {"stat": {"time": 0, "lati": 0, "long": 0, "alti": 0, "rxnb": 0, "rxf2": 0, "ackr": 0, "dwnb": 2, "txnb": 2}}
node_packet = {"rxpk": [{"tmst": 0, "chan": 0, "rfch": 0, "freq": 868.1, "stat": 1, "modu": "LORA", "datr": "SF7BW125", "codr": "4/5", "rssi": 0, "lsnr": 0, "size": 0, "data": ""}]}


class NanoGateway:

    def __init__(self, id, frequency, dr, ssid, password, server, port, ntp='pool.ntp.org', ntp_period=3600):
        self.id = id
        self.frequency = frequency
        self.sf = self.dr_to_sf(dr)
        self.ssid = ssid
        self.password = password
        self.server = server
        self.port = port
        self.ntp = ntp
        self.ntp_period = ntp_period

        self.uplink_tmst = 0
        self.pkts_up_acked = 0

        self.udp_lock = _thread.allocate_lock()

        # Change WiFi to STA mode and connect
        self.wlan = WLAN(mode=WLAN.STA)
        self.connect_to_wifi()

        # Sync UTC time
        self.rtc = machine.RTC()
        self.rtc.ntp_sync(self.ntp, update_period=self.ntp_period)
        print("Time sync!")

        # Get the server IP and create the UDP socket
        self.server_ip = socket.getaddrinfo(self.server, self.port)[0][-1]
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setblocking(False)

        self.uplink_alarm = None
        self.stat_alarm = Timer.Alarm(handler=lambda t: self.push_data(self.make_gateway_stat()), s=60, periodic=True)
        self.pull_alarm = Timer.Alarm(handler=lambda u: self.pull_data(), s=25, periodic=True)

        # push data immediatelly
        self.push_data(self.make_gateway_stat())

        time.sleep(0.05)

        _thread.start_new_thread(self.udp_thread, ())

        # initialize LoRa in LORA mode
        # more params can also be given, like frequency, tx power and spreading factor
        self.lora = LoRa(mode=LoRa.LORA, frequency=self.frequency, bandwidth=LoRa.BW_125KHZ, sf=7, 
                         preamble=8, coding_rate=LoRa.CODING_4_5, tx_iq=True)
        # create a raw LoRa socket
        self.lora_sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
        self.lora_sock.setblocking(False)

        self.lora.callback(trigger=(LoRa.RX_PACKET_EVENT | LoRa.TX_PACKET_EVENT), handler=self.lora_cb)

    def connect_to_wifi(self):
        self.wlan.connect(self.ssid, auth=(None, self.password))
        while not self.wlan.isconnected():
            time.sleep(0.5)
        print("WiFi connected!")

    def make_gateway_stat(self):
        now = self.rtc.now()
        gateway_stat["stat"]["time"] = "%d-%02d-%02d %02d:%02d:%02d GMT" % (now[0], now[1], now[2], now[3], now[4], now[5])
        return json.dumps(gateway_stat)

    def make_node_data(self, rx_data, tmst, dr, rssi, snr):
        node_packet["rxpk"][0]["tmst"] = tmst
        node_packet["rxpk"][0]["datr"] = dr
        node_packet["rxpk"][0]["rssi"] = rssi
        node_packet["rxpk"][0]["lsnr"] = float(snr)
        node_packet["rxpk"][0]["data"] = binascii.b2a_base64(rx_data)[:-1]
        node_packet["rxpk"][0]["size"] = len(rx_data)
        return json.dumps(node_packet)

    def push_data(self, data):
        token = os.urandom(2)
        packet = bytes([PROTOCOL_VERSION]) + token + bytes([PUSH_DATA]) + binascii.unhexlify(self.id) + data
        with self.udp_lock:
            try:
                self.sock.sendto(packet, self.server_ip)
            except Exception:
                print("PUSH exception")

    def pull_data(self):
        token = os.urandom(2)
        packet = bytes([PROTOCOL_VERSION]) + token + bytes([PULL_DATA]) + binascii.unhexlify(self.id)
        with self.udp_lock:
            try:
                self.sock.sendto(packet, self.server_ip)
            except Exception:
                print("PULL exception")

    def ack_pull_rsp(self, token):
        packet = bytes([PROTOCOL_VERSION]) + token + bytes([PULL_ACK]) + binascii.unhexlify(self.id)
        with self.udp_lock:
            try:
                self.sock.sendto(packet, self.server_ip)
            except Exception:
                print("PULL RSP ACK exception")

    def lora_cb(self, lora):
        events = lora.events()
        if events & LoRa.RX_PACKET_EVENT:
            rx_data = self.lora_sock.recv(256)
            stats = lora.stats()
            self.push_data(self.make_node_data(rx_data, stats.timestamp, stats.sf, stats.rssi, stats.snr))
            self.uplink_tmst = stats.timestamp
        elif events & LoRa.TX_PACKET_EVENT:
            # return to the uplink sf and frequency
            lora.sf(self.sf)
            lora.frequency(self.frequency)

    def dr_to_sf(self, dr):
        sf = dr[2:4]
        if sf[1] not in '0123456789':
            sf = sf[:1]
        return int(sf)

    def send_down_link(self, data, tmst, dr, frequency):
        self.lora.sf(self.dr_to_sf(dr))
        # self.lora.frequency(frequency)
        self.lora.frequency(869525000)
        t = tmst - time.ticks_us()
        while time.ticks_us() < tmst:
            pass
        self.lora_sock.send(data)
        print("Downlink delay:", tmst, str(time.ticks_us() - self.uplink_tmst))
        print("t:", t)

    def udp_thread(self):
        while True:
            try:
                data, src = self.sock.recvfrom(1024)
                _token = data[1:3]
                _type = data[3]
                if _type == PUSH_ACK:
                    self.pkts_up_acked += 1
                    print("Push ack!")
                elif _type == PULL_ACK:
                    self.pkts_up_acked += 1
                    print("Pull ack!")
                elif _type == PULL_RESP:
                    tx_pk = json.loads(data[4:])
                    tmst = tx_pk["txpk"]["tmst"]
                    t_us = tmst - time.ticks_us() - 5000
                    if t_us < 0:
                        t_us += 0xFFFFFFFF
                    if t_us < 10000000:
                        self.uplink_alarm = Timer.Alarm(handler=lambda x: self.send_down_link(binascii.a2b_base64(tx_pk["txpk"]["data"]), 
                                                                                              tx_pk["txpk"]["tmst"] - 10, tx_pk["txpk"]["datr"],
                                                                                              int(tx_pk["txpk"]["freq"]) * 1000000), us=t_us)
                        print('Downlink!', tmst, int(tx_pk["txpk"]["freq"]) * 1000000)
                        self.ack_pull_rsp(_token)
                        print(t_us)
                    else:
                        print("Downlink TMST error!")
            except socket.timeout:
                pass
            except OSError as e:
                if e.errno == errno.EAGAIN:
                    pass
                else:
                    print("UDP recv OSError Exception")
            except Exception:
                print("UDP recv Exception")
            time.sleep(0.05)

nanogw = NanoGateway(id='1a2b3c4d5e6f7081', frequency=868100000, dr="SF7BW125", ssid='H369A09317F',
                     password='667F976DC447', server='router.eu.thethings.network',
                     port=1700, ntp='pool.ntp.org', ntp_period=3600)

while True:
    time.sleep(2)
