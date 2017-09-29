	 / _____)             _              | |    
	( (____  _____ ____ _| |_ _____  ____| |__  
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \ 
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2013 Semtech-Cycleo

LoRa Gateway project
=====================

## 1. Core library: libloragw
-----------------------------

This directory contains the sources of the library to build a LoRa Picocell
Gateway based on a Semtech LoRa multi-channel RF receiver (a.k.a. concentrator).
Once compiled all the code is contained in the libloragw.a file that will be 
statically linked (ie. integrated in the final executable).
The library implements the communication with the concentrator embedded MCU
through a serial port.

The library also comes with a bunch of basic tests programs that are used to 
test the different sub-modules of the library.

## 2. Helper programs
---------------------

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

This software is used to jump to the PicoCell Gateway bootloader for programming
the MCU with a new firmware.
Please refer to the readme of picoGW_mcu repository for more information about
MCU flash programming.

### 2.6. util_chip_id ###

This software is used to obtain the unique id of the PicoCell gateway (the
64 bits unique id extracted from the STM32 unique id registers).

## 4. User Guide
----------------

[A detailed PicoCell GW user guide is available here](http://www.semtech.com/images/datasheet/picocell_gateway_user_guide.pdf)

## 5. Changelog
---------------

### v0.2.0  ###

* HAL: reverted AGC FW to version 4, as v5 was necessary to fix a HW bug which
has been fixed since rev V02A of the picoCell reference design.
* HAL: fixed a bug lgw_com_send_command() function to prevent from hanging when
writing on the COM link was not possible.
* HAL: fixed memory alignment on FSK syncword configuration.

### v0.1.2  ###

* HAL: fixed a wrong copy size and position of MCU unique ID in lgw_mcu_get_unique_id().
* HAL: disabled some logs.

### v0.1.1  ###

* HAL: fixed MCU command wrong size in lgw_mcu_commit_radio_calibration().
* HAL: fixed MCU reset sequence to wait for MCU to complete reset and reinit
the communication bridge before exiting. This is to avoid issues when restarting
the concentrator after exit.

### v0.1.0  ###

* HAL: code clean-up/refactoring
* HAL: serial port configuration to handle both USB or UART communication with
mcu.
* util_boot: only used to jump to the MCU bootloader.
* util_chip_id: no more command line parameter, just print the PicoCell GW
unique ID on the console.
* HAL/util_*: added a parameter to lgw_connect() function to specify the COM
device path to be used to communicate with the concentrator board (tty...).

### v0.0.1  ###

* Initial release


## 6. Legal notice
------------------

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
