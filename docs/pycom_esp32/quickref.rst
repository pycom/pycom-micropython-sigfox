.. _quickref_:

Quick reference for the Pycom modules
=====================================

.. image:: https://raw.githubusercontent.com/pycom/LoPy/master/docs/Pin-Out.png
    :alt: LoPy pinout
    :width: 200px

.. image:: https://raw.githubusercontent.com/pycom/LoPy/master/docs/LoPy_On_Expansion.png
    :alt: LoPy On the Expansion board
    :width: 315px

.. warning::

    DO NOT connect anything to Pins ``P5``, ``P6`` and ``P7``, as this pins are used byt the SPI bus that controls the LoRa radio. These pins should be treated as ``NC``. Wiring connections to these pins will cause incorrect behaviour of the LoRa radio.

.. note::

    Pin ``P2`` is also connected to the RGB LED. This pin can be used for other purposes if the RGB functionality is not needed.

General board control
---------------------

See the :mod:`machine` module::

    import machine

    help(machine) # display all members from the machine module
    machine.freq() # get the CPU frequency
    machine.unique_id() # return the 6-byte unique id of the board (the LoPy's WiFi MAC address)

Pins and GPIO
-------------

See :mod:`machine.Pin`::

    from machine import Pin

    # initialize ``P9`` in gpio mode and make it an output
    p_out = Pin('P9', mode=Pin.OUT)
    p_out.value(1)
    p_out.value(0)
    p_out.toggle()
    p_out(True)

    # make ``P10`` an input with the pull-up enabled
    p_in = Pin('P10', mode=Pin.IN, pull=Pin.PULL_UP)
    p_in() # get value, 0 or 1

UART (serial bus)
-----------------

See :mod:`machine.UART`::

    from machine import UART
    # this uses the UART_1 default pins for TXD and RXD (``P3`` and ``P4``)
    uart = UART(1, baudrate=9600)
    uart.write('hello')
    uart.read(5) # read up to 5 bytes

SPI bus
-------

::

    from machine import SPI

    # configure the SPI master @ 2MHz
    # this uses the SPI default pins for CLK, MOSI and MISO (``P10``, ``P11`` and ``P12``)
    spi = SPI(0, mode=SPI.MASTER, baudrate=2000000, polarity=0, phase=0)
    spi.write(bytes([0x01, 0x02, 0x03, 0x04, 0x05]) # send 5 bytes on the bus
    spi.read(5) # receive 5 bytes on the bus
    rbuf = bytearray(5)
    spi.write_readinto(bytes([0x01, 0x02, 0x03, 0x04, 0x05], rbuf) # send a receive 5 bytes

I2C bus
-------

See :mod:`machine.I2C`::

    from machine import I2C
    # configure the I2C bus
    i2c = I2C(0, I2C.MASTER, baudrate=100000)
    i2c.scan() # returns list of slave addresses
    i2c.writeto(0x42, 'hello') # send 5 bytes to slave with address 0x42
    i2c.readfrom(0x42, 5) # receive 5 bytes from slave
    i2c.readfrom_mem(0x42, 0x10, 2) # read 2 bytes from slave 0x42, slave memory 0x10
    i2c.writeto_mem(0x42, 0x10, 'xy') # write 2 bytes to slave 0x42, slave memory 0x10

LoRa (LoRaMAC)
--------------

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


WLAN (WiFi)
-----------

See :mod:`machine.WLAN`::

    import machine
    from network import WLAN

    # configure the WLAN subsystem in station mode (the default is AP)
    wlan = WLAN(mode=WLAN.STA)
    # go for fixed IP settings (IP, Subnet, Gateway, DNS)
    wlan.ifconfig(config=('192.168.0.107', '255.255.255.0', '192.168.0.1', '192.168.0.1'))
    wlan.scan()     # scan for available networks
    wlan.connect(ssid='mynetwork', auth=(WLAN.WPA2, 'my_network_key'))
    while not wlan.isconnected():
        pass
    print(wlan.ifconfig())

Telnet and FTP server
---------------------

See :mod:`network.Server`::

    from network import Server

    # init with new user, password and seconds timeout
    server = Server(login=('user', 'password'), timeout=60)
    server.timeout(300) # change the timeout
    server.timeout() # get the timeout
    server.isrunning() # check whether the server is running or not

Heart beat RGB LED
------------------

See :mod:`pycom`::

    import pycom

    pycom.heartbeat(False)  # disable the heartbeat LED
    pycom.heartbeat(True)   # enable the heartbeat LED
    pycom.heartbeat()       # get the heartbeat state
    pycom.rgbled(0xff00)    # make the LED light up in green color


Threading
---------

::

    import _thread
    import time

    def th_func(delay, id):
        while True:
            time.sleep(delay)
            print('Running thread %d' % id)

    for i in range(2):
        _thread.start_new_thread(th_func, (i + 1, i))

PWM
---

See :mod:`machine.PWM`::

    from machine import PWM
    pwm = PWM(0, frequency=5000)  # use PWM timer 0, with a frequency of 50KHz
    # create pwm channel on pin P12 with a duty cycle of 50%
    pwm_c = pwm.channel(0, pin='P12', duty_cycle=0.5)
    pwm_c.duty_cycle(0.3) # change the duty cycle to 30%


ADC
---

See :mod:`ADC`::

    from machine import ADC
    adc = ADC(0)
    adc_c = adc.channel(pin='P13')
    adc_c()
    adc_c.value()
