Short readme for how to use the PyJTAG.
Detailed information are here: https://pycomiot.atlassian.net/wiki/spaces/FIR/pages/966295564/Usage+of+PyJTAG

Setup the PyJTAG board's switches:
ESP32 JTAG: all turned ON
ESP32 B.LOADER: all turned ON except SAFE_BOOT_SW which is OFF
TO LTE UART 1/2: does not matter
CURRENT SHUNTS: connected

Generally follow these rules to setup JTAG debugging on your OS: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/jtag-debugging/index.html

(Download link of OpenOCD for ESP32 from Espressif: https://github.com/espressif/openocd-esp32/releases)

Running OpenOCD assuming the current location is "esp32" folder in Firmware-Development repository:
Start OpenOCD: "PATH_TO_OPENOCD/bin/openocd -s PATH_TO_OPENOCD/share/openocd/scripts -s PyJTAG -f PyJTAG/interface/ftdi/esp32-pycom.cfg -f PyJTAG/board/esp32-pycom.cfg"
Output should be like: 

Open On-Chip Debugger  v0.10.0-esp32-20191114 (2019-11-14-14:15)
Licensed under GNU GPL v2
For bug reports, read
	http://openocd.org/doc/doxygen/bugs.html
none separate
adapter speed: 20000 kHz
Info : Configured 2 cores
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Error: type 'esp32' is missing virt2phys
Info : ftdi: if you experience problems at higher adapter clocks, try the command "ftdi_tdo_sample_edge falling"
Info : clock speed 20000 kHz
Info : JTAG tap: esp32.cpu0 tap/device found: 0x120034e5 (mfg: 0x272 (Tensilica), part: 0x2003, ver: 0x1)
Info : JTAG tap: esp32.cpu1 tap/device found: 0x120034e5 (mfg: 0x272 (Tensilica), part: 0x2003, ver: 0x1)
Info : Listening on port 3333 for gdb connections


When OpenOCD is running start GDB from "esp32" folder assuming you have a FIPY:
"xtensa-esp32-elf-gdb -x gdbinit build/FIPY/release/application.elf"

