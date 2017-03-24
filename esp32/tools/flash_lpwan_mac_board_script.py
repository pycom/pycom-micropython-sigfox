import time

LPWAN_MAC_PATH='/flash/sys/lpwan.mac'

def MAC_to_bytearray(mac):
    import struct
    mac = mac.replace("-","").replace(":","")
    mac = int(mac, 16)
    return struct.pack('>Q', mac)

def write_mac(mac):
    with open(LPWAN_MAC_PATH, 'wb') as output:
        output.write(mac)
        time.sleep(0.5)

try:
    import machine
    new_mac = MAC_to_bytearray("{MAC_ADDRESS}")
    write_mac(new_mac)
    machine.reset()
except:
    print("LPWAN MAC write failure")
