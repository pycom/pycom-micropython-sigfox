
LoRa-MAC (Raw LoRa)
-------------------

Basic LoRa connection example, sending and receiving data. In LoRa-MAC mode the LoRaWAN layer is bypassed
and the radio is used directly. The data sent is not formatted or encrypted in any way, and no addressing information is added to the frame.

For the example below you need to LoPy's. You can notice that there's a ``While`` loop with a random delay time to minimize the chances of the 2 LoPy's transmitting at the same time. Run the code below on the 2 LoPy modules and you you will see the word ``'Hello'`` being received on both sides.

::

    from network import LoRa
    import socket
    import machine
    import time

    # initialize LoRa in LORA mode
    # more params can also be given, like frequency, tx power and spreading factor
    lora = LoRa(mode=LoRa.LORA)

    # create a raw LoRa socket
    s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)

    while True:
        # send some data
        s.setblocking(True)
        s.send('Hello')

        # get any data received...
        s.setblocking(False)
        data = s.recv(64)
        print(data)

        # wait a random amount of time
        time.sleep(machine.rng() & 0x0F)
