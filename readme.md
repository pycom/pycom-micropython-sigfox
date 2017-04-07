	 / _____)             _              | |    
	( (____  _____ ____ _| |_ _____  ____| |__  
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \ 
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2013 Semtech-Cycleo

LoRa Gateway project
=====================

1. Core library: libloragw
---------------------------

This directory contains the sources of the library to build a LoRa Picocell
Gateway based on a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
Once compiled all the code is contained in the libloragw.a file that will be 
statically linked (ie. integrated in the final executable).
The library implements an USB CDC (virtual com port) to communicate with the
embedded mcu.

The library also comes with a bunch of basic tests programs that are used to 
test the different sub-modules of the library.

2. Helper programs
-------------------

Those programs are included in the project to provide examples on how to use 
the HAL library, and to help the system builder test different parts of it.

### 2.1. util_pkt_logger ###

This software is used to set up a LoRa concentrator using a JSON configuration
file and then record all the packets received in a log file, indefinitely, until
the user stops the application.

### 2.2. util_com_stress ###

This software is used to check the reliability of the link between the host
platform (on which the program is run) and the LoRa concentrator register file
that is the interface through which all interaction with the LoRa concentrator
happens.

### 2.3. util_tx_test ###

This software is used to send test packets with a LoRa concentrator. The packets
contain little information, on no protocol (ie. MAC address) information but
can be used to assess the functionality of a gateway downlink using other
gateways as receivers.

### 2.4. util_tx_continuous ###

This software is used to set LoRa concentrator in Tx continuous mode,
for spectral measurement.

### 2.5. util_boot ###

This software is used to set PicoCell Gateway in dfu mode for download new FW MCU.

### 2.6. util_chip_id ###

This software is used to obtain the unique id of the PicoCell gateway.
64 bits unique id extracts from the STM32 uinque id registers.


4. Changelog
-------------

### v0.0.1  ###

* Initial release


5. Legal notice
----------------

The information presented in this project documentation does not form part of 
any quotation or contract, is believed to be accurate and reliable and may be 
changed without notice. No liability will be accepted by the publisher for any 
consequence of its use. Publication thereof does not convey nor imply any 
license under patent or other industrial or intellectual property rights. 
Semtech assumes no responsibility or liability whatsoever for any failure or 
unexpected operation resulting from misuse, neglect improper installation, 
repair or improper handling or unusual physical or electrical stress 
including, but not limited to, exposure to parameters beyond the specified 
maximum ratings or operation outside the specified range. 

SEMTECH PRODUCTS ARE NOT DESIGNED, INTENDED, AUTHORIZED OR WARRANTED TO BE 
SUITABLE FOR USE IN LIFE-SUPPORT APPLICATIONS, DEVICES OR SYSTEMS OR OTHER 
CRITICAL APPLICATIONS. INCLUSION OF SEMTECH PRODUCTS IN SUCH APPLICATIONS IS 
UNDERSTOOD TO BE UNDERTAKEN SOLELY AT THE CUSTOMER'S OWN RISK. Should a
customer purchase or use Semtech products for any such unauthorized 
application, the customer shall indemnify and hold Semtech and its officers, 
employees, subsidiaries, affiliates, and distributors harmless against all 
claims, costs damages and attorney fees which could arise.

*EOF*
