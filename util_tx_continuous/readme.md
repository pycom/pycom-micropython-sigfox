	  ______                              _
	 / _____)             _              | |
	( (____  _____ ____ _| |_ _____  ____| |__
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2014 Semtech-Cycleo

Tx continuous program for LoRa concentrator
===========================================


1. Introduction
----------------

This software is used to set LoRa concentrator in Tx continuous mode,
for spectral measurement.
The user can set the modulation type, the modulation parameters, and the multiple gains of
the Tx chain.
The program runs indefinitely, until the user stops the application.


2. Command line options
-----------------------
`-u`
use the FT2232 SPI-over-USB bridge for communication with the concentrator.
This is the default option.

`-d`
use the Linux SPI device driver located in /dev/spidev0.0 by default.

`-d filename`
use the Linux SPI device driver, but with an explicit path, for systems with 
several SPI device drivers, or uncommon numbering scheme.

### 2.3. Tx options ###

`-f`
Tx RF frequency in MHz.
Valid range: [800:1000]

`--dig`
Digital gain trim.
0:1, 1:7/8, 2:3/4, 3:1/2 

`--dac`
SX1257 Tx DAC gain trim.
0:-9dB, 1:-6dB, 2:-3dB, 3:0dB 

`--mix`
SX1257 Tx mixer gain trim.
Valid range: [0:15]
15 corresponds to maximum gain, 1 LSB corresponds to 2dB step

`--vpa`
External PA gain trim
Valid range: [0:3]

`--mod`
Modulation type, {'LORA','FSK','CW'}, 'CW':unmodulated carrier

`--sf`
LoRa Spreading Factor
Valid range: [7:12]

`--bw`
LoRa bandwidth in kHz
Valid range: [125,250,500]

`--br`
FSK bitrate in kbps.
Valid Range: [0.5:250]

`--fdev`
FSK frequency deviation in kHz.
Valid Range: [1:250]

`--bt`
FSK BT coefficient of gaussion filter.
Valid Range: [0:3]


3. Use
-------

Example:
./tx_continuous -d -f 902.3 --dac 2 --mix 14 --vpa 2.0 --mod "CW"


4. License
-----------

Copyright (c) 2013, SEMTECH S.A.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of the Semtech corporation nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL SEMTECH S.A. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*EOF*