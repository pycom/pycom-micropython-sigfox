.. _lora_examples:

LoRa examples
=============

LoRa (LoRaMAC)
--------------

::

    from network import LoRa
    import socket

    # Initialize LoRa in LORA mode.
    # More params can be given, like frequency, tx power and spreading factor.
    lora = LoRa(mode=LoRa.LORA)

    # create a raw LoRa socket
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
    s.setblocking(False)

    # send some data
    s.send(bytes([0x01, 0x02, 0x03])

    # get any data received...
    data = s.recv(64)
    print(data)

LoRa (LoRaWAN with OTAA)
------------------------

::

    from network import LoRa
    import socket
    import time

    # Initialize LoRa in LORAWAN mode.
    lora = LoRa(mode=LoRa.LORAWAN)

    # create an OTAA authentication parameters
    app_eui = binascii.unhexlify('AD A4 DA E3 AC 12 67 6B'.replace(' ',''))
    app_key = binascii.unhexlify('11 B0 28 2A 18 9B 75 B0 B4 D2 D8 C7 FA 38 54 8B'.replace(' ',''))

    # join a network using OTAA (Over the Air Activation)
    lora.join(activation=LoRa.OTAA, auth=(app_eui, app_key), timeout=0)

    # wait until the module has joined the network
    while not lora.has_joined():
        time.sleep(2.5)
        print('Not yet joined...')

    # create a LoRa socket
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)

    # set the LoRaWAN data rate
    s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)

    # make the socket non-blocking
    s.setblocking(False)

    # send some data
    s.send(bytes([0x01, 0x02, 0x03]))

    # get any data received...
    data = s.recv(64)
    print(data)


LoRa (LoRaWAN with ABP)
-----------------------

::

    from network import LoRa
    import socket

    # Initialize LoRa in LORAWAN mode.
    lora = LoRa(mode=LoRa.LORAWAN)

    # create an ABP authentication params
    dev_addr = struct.unpack(">l", binascii.unhexlify('00 00 00 05'.replace(' ','')))[0]
    nwk_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))
    app_swkey = binascii.unhexlify('2B 7E 15 16 28 AE D2 A6 AB F7 15 88 09 CF 4F 3C'.replace(' ',''))

    # join a network using ABP (Activation By Personalization)
    lora.join(activation=LoRa.ABP, auth=(dev_addr, nwk_swkey, app_swkey))

    # create a LoRa socket
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)

    # set the LoRaWAN data rate
    s.setsockopt(socket.SOL_LORA, socket.SO_DR, 5)

    # make the socket non-blocking
    s.setblocking(False)

    # send some data
    s.send(bytes([0x01, 0x02, 0x03]))

    # get any data received...
    data = s.recv(64)
    print(data)
