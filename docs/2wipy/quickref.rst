.. _quickref_:

Quick reference for the WiPy 2.0
================================

.. image:: https://raw.githubusercontent.com/pycom/WiPy/master/docs/Pin-Out.png
    :alt: WiPy pinout
    :width: 200px

.. image:: https://raw.githubusercontent.com/pycom/WiPy/master/docs/WiPy_On_Expansion.png
    :alt: WiPy On the Expansion board
    :width: 315px

.. note::

    Pin ``P2`` is also connected to the RGB LED. This pin can be used for other purposes if the RGB functionality is not needed.

General board control
---------------------

::

    import machine

    help(machine) # display all members from the machine module
    machine.freq() # get the CPU frequency
    machine.unique_id() # return the 6-byte unique id of the board (the WiPy's WiFi MAC address)

Pins and GPIO
-------------

::

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

::

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

::

    from machine import I2C
    # configure the I2C bus
    i2c = I2C(0, I2C.MASTER, baudrate=100000)
    i2c.scan() # returns list of slave addresses
    i2c.writeto(0x42, 'hello') # send 5 bytes to slave with address 0x42
    i2c.readfrom(0x42, 5) # receive 5 bytes from slave
    i2c.readfrom_mem(0x42, 0x10, 2) # read 2 bytes from slave 0x42, slave memory 0x10
    i2c.writeto_mem(0x42, 0x10, 'xy') # write 2 bytes to slave 0x42, slave memory 0x10

WLAN (WiFi)
-----------

::

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

::

    from network import Server

    # init with new user, password and seconds timeout
    server = Server(login=('user', 'password'), timeout=60)
    server.timeout(300) # change the timeout
    server.timeout() # get the timeout
    server.isrunning() # check whether the server is running or not

Heart beat RGB LED
------------------

::

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

::

    from machine import PWM
    pwm = PWM(0, frequency=5000)  # use PWM timer 0, with a frequency of 50KHz
    # create pwm channel on pin P12 with a duty cycle of 50%
    pwm_c = pwm.channel(0, pin='P12', duty_cycle=0.5)
    pwm_c.duty_cycle(0.3) # change the duty cycle to 30%


ADC
---

::

    from machine import ADC
    adc = ADC(0)
    adc_c = adc.channel(pin='P13')
    adc_c()
    adc_c.value()
