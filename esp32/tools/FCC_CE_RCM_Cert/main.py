from network import Sigfox
from network import LoRa
import socket
import machine
import time

hop_num = 68
start_freq = 863000000
end_freq = 870000000

lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868)
sigfox = None

sock = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
sock.setblocking(False)

def select_new_frequency():
    global hop_num
    global start_freq

    r_num = (machine.rng() % hop_num) + 1
    hop = r_num * 100000
    frequency = start_freq + hop
    return frequency

def sock_send_repeat(packt_len, num_packts, delay_secs, fhss=False):
    global sock
    global lora

    if delay_secs <= 0:
        delay_secs = 0.1

    for i in range(num_packts):
        packet = bytes([machine.rng() & 0xFF for b in range(packt_len)])
        print("Sending packet number {}: {}".format(i, packet))
        if fhss:
            lora.frequency(select_new_frequency())
        sock.send(packet)
        time.sleep(delay_secs)

def sock_send_all_channels(packt_len, delay_secs=0.1):
    global sock
    global lora
    global start_freq
    global end_freq

    start = start_freq
    end = end_freq

    if delay_secs <= 0:
        delay_secs = 0.1

    for i in range((end - start) / 100000):
        packet = bytes([machine.rng() & 0xFF for b in range(packt_len)])
        print("Sending packet number {}: {}".format(i, packet))
        lora.frequency(start + (i * 100000))
        sock.send(packet)
        time.sleep(delay_secs)

def start_test_fcc():
    global sigfox
    global lora
    global start_freq
    global end_freq
    global hop_num

    sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ2)
    lora = LoRa(mode=LoRa.LORA, region=LoRa.US915)
    start_freq = 902000000
    end_freq = 928000000
    hop_num = 258

def start_test_rcm():
    global sigfox
    global lora
    global start_freq
    global end_freq
    global hop_num

    sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ4)
    lora = LoRa(mode=LoRa.LORA, region=LoRa.AU915)
    start_freq = 915000000
    end_freq = 928000000
    hop_num = 128

def start_test_ce():
    global sigfox
    global lora
    global start_freq
    global end_freq
    global hop_num

    sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ1)
    lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868)
    start_freq = 863000000
    end_freq = 870000000
    hop_num = 68

def do_freq_hopping_fcc():
    global sigfox

    frequency = 902200000 + 25000

    while frequency < 904700000:
        sigfox.test_mode(8, frequency)
        frequency += 25000


def do_freq_hopping_rcm():
    global sigfox

    frequency = 915000000 + 25000

    while frequency < 928000000:
        sigfox.test_mode(8, frequency)
        frequency += 25000


def do_freq_hopping_ce():
    global sigfox

    frequency = 863000000 + 100000

    while frequency < 870000000:
        sigfox.test_mode(8, frequency)
        frequency += 100000


def do_continuos_transmit(frequency):
    global sigfox

    while True:
        sigfox.test_mode(8, frequency)
        time.sleep_ms(5)

print()
print("*** LoRa/Sigfox radio setup complete, ready to test ***")
print()
