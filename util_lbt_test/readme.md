	 / _____)             _              | |    
	( (____  _____ ____ _| |_ _____  ____| |__  
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \ 
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
		©2013 Semtech-Cycleo

Lora Gateway RSSI histogram
===========================

1. Introduction
----------------

This software captures and computes the histogram of RSSIs on SX1272.

2. Dependencies
----------------

loragw_fpga

3. Usage
---------
Before running the util_histo_test, the SX1301 MUST be first configured in RX mode.
For example, you must run any packet-forwarder or packet-logger

util_histo_test -f <start_freq>:<stop_freq>:<freq_step> -t <tempo> -n <nb_point> -o <rssi_offset>

Command-line options:
 -h: help
 -f: <start_freq>:<stop_freq>:<freq_step>
 -n: number of RSSI captures default is 10000  (authorized value are 1000 <-> 32000)
 -t: delay b/w two capture in number of 32MHz clock period, default is 32000 (one RSSI read each ms) (authorized value are 1000 <-> 32000)
 -o: rssi negative offset in dBm (default 142 means that a -142 dBm offset is applied on rssi read value)

4. Changelog
-------------

2015-02-17	v1.0	Initial version
