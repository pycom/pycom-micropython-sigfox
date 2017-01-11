
LoPy to LoPy
------------

This example show how to connect two nodes via LoRa.

**Node A script:**

::

    from network import LoRa
    import socket
    import time

    lora = LoRa(mode=LoRa.LORA, frequency=863000000)
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
    s.setblocking(False)

    while True:
        if s.recv(64) == b'Ping':
            s.send('Pong')
        time.sleep(5)



**Node B script:**

::

    from network import LoRa
    import socket
    import time

    lora = LoRa(mode=LoRa.LORA, frequency=863000000)
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
    s.setblocking(False)

    while True:
        s.send('Ping')
        time.sleep(5)
