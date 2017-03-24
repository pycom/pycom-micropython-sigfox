
.. _pir_sensor:

PIR Sensor
----------

This code reads PIR sensor triggers from `this simple PIR sensor <https://www.kiwi-electronics.nl/PIR-Motion-Sensor>`_ and sends an HTTP request for every trigger, in this case to a Domoticz installation. When motion is constantly detected, this PIR sensor keeps the pin high, in which case this code will keep sending HTTP requests every 10 seconds (configurable with the hold_time variable)

Main logic (main.py)

::

	import time
	from network import WLAN
	from machine import Pin
	from domoticz import Domoticz

	wl = WLAN(WLAN.STA)
	d = Domoticz("<ip>", 8080 ,"<hash>")

	#config
	hold_time_sec = 10

	#flags
	last_trigger = -10 

	pir = Pin('G4',mode=Pin.IN,pull=Pin.PULL_UP)

	# main loop
	print("Starting main loop")
	while True:
	    if pir() == 1:
	        if time.time() - last_trigger > hold_time_sec:
	            last_trigger = time.time()
	            print("Presence detected, sending HTTP request")
	            try:
	                return_code = d.setVariable('Presence:LivingRoom','1')
	                print("Request result: "+str(return_code))
	            except Exception as e:
	                print("Request failed")
	                print(e)
	    else:
	        last_trigger = 0
	        print("No presence")

	    time.sleep_ms(500)

	print("Exited main loop")


Boot file with wifi script (boot.py)

For more wifi scripts, see the :ref:`wlan step by step <wlan_step_by_step>` tutorial.

::

	import os
	import machine

	uart = machine.UART(0, 115200)
	os.dupterm(uart)

	known_nets = {
	    'NetworkID':        {'pwd': '<password>', 'wlan_config':  ('10.0.0.8', '255.255.0.0', '10.0.0.1', '10.0.0.1')}, 
	}

	from network import WLAN
	wl = WLAN()


	if machine.reset_cause() != machine.SOFT_RESET:

	    wl.mode(WLAN.STA)
	    original_ssid = wl.ssid()
	    original_auth = wl.auth()

	    print("Scanning for known wifi nets")
	    available_nets = wl.scan()
	    nets = frozenset([e.ssid for e in available_nets])

	    known_nets_names = frozenset([key for key in known_nets])
	    net_to_use = list(nets & known_nets_names)
	    try:
	        net_to_use = net_to_use[0]
	        net_properties = known_nets[net_to_use]
	        pwd = net_properties['pwd']
	        sec = [e.sec for e in available_nets if e.ssid == net_to_use][0]
	        if 'wlan_config' in net_properties:
	            wl.ifconfig(config=net_properties['wlan_config']) 
	        wl.connect(net_to_use, (sec, pwd), timeout=10000)
	        while not wl.isconnected():
	            machine.idle() # save power while waiting
	        print("Connected to "+net_to_use+" with IP address:" + wl.ifconfig()[0])
	        
	    except Exception as e:
	        print("Failed to connect to any known network, going into AP mode")
	        wl.init(mode=WLAN.AP, ssid=original_ssid, auth=original_auth, channel=6, antenna=WLAN.INT_ANT)


Domoticz wrapper (domoticz.py)

::

	import socket
	class Domoticz:
	    
	    def __init__(self, ip, port,  basic):
	        self.basic = basic
	        self.ip = ip
	        self.port = port
	    
	    def setLight(self, idx, command):
	        return self.sendRequest("type=command&param=switchlight&idx="+idx+"&switchcmd="+command)

	    def setVariable(self, name, value):
	        return self.sendRequest("type=command&param=updateuservariable&vtype=0&vname="+name+"&vvalue="+value)

	    def sendRequest(self, path):
	        try:
	            s = socket.socket()
	            s.connect((self.ip,self.port))
	            s.send(b"GET /json.htm?"+path+" HTTP/1.1\r\nHost: pycom.io\r\nAuthorization: Basic "+self.basic+"\r\n\r\n")
	            status = str(s.readline(), 'utf8')
	            code = status.split(" ")[1]
	            s.close()
	            return code
	            
	        except Exception:
	            print("HTTP request failed")
	            return 0

