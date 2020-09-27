# Short readme for how to use the PyJTAG

## Setup
Generally follow these rules to setup JTAG debugging on your OS: https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/jtag-debugging/index.html

Download link for OpenOCD for ESP32 from Espressif: https://github.com/espressif/openocd-esp32/releases


## Build the firmware
Create the firmware with `BTYPE=debug` flag.

Note: Do not use the default pins assigned to UART, SPI, CAN because they are used by the JTAG. Pins not to be used: P4, P9, P10, P23.

## Setup the PyJTAG board

PyJTAG's switches:
 * ESP32 JTAG: all turned ON
 * ESP32 B.LOADER: all turned ON except SAFE_BOOT_SW which is OFF
 * TO LTE UART 1/2: does not matter
 * CURRENT SHUNTS: connected

Place the Pycom board with the reset button towards the Current Shunts. Now connect the PyJTAG via usb. You will see four new USB devices. On Linux this will look like this:
```
$ lsusb -d 0403:
Bus 001 Device 010: ID 0403:6011 Future Technology Devices International, Ltd FT4232H Quad HS USB-UART/FIFO IC
$ ls /dev/ttyUSB?
/dev/ttyUSB0  /dev/ttyUSB1  /dev/ttyUSB2  /dev/ttyUSB3
```

## Start OCD

Go to `esp32` folder in Firmware-Development repository and run:
```
PATH_TO_OPENOCD/bin/openocd -s PATH_TO_OPENOCD/share/openocd/scripts -s PyJTAG -f PyJTAG/interface/ftdi/esp32-pycom.cfg -f PyJTAG/board/esp32-pycom.cfg
```

Output should be like: 
```
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
```

## Start GDB

When OpenOCD is running, start GDB from `esp32` folder. Assuming you have a FIPY:
```
xtensa-esp32-elf-gdb -x PyJTAG/gdbinit build/FIPY/debug/application.elf
```

In `PyJTAG/gdbinit` a breakpoint is configured at `TASK_Micropython`, so execution should stop there first:

```
Thread 1 hit Temporary breakpoint 1, TASK_Micropython (pvParameters=0x0) at mptask.c:136
```


## REPL

Connect to `/dev/ttyUSB2` to reach the REPL terminal over usb serial. E.g. using pymakr in Atom. 

## Troubleshooting
If openocd says "Error: Connect failed", try to close gdb and openocd and start over.

If `/dev/ttyUSB0` doesn't show up or disappears, disconnect the PyJTAG board, reconnect and start over.

It can be advisable to use the `gdb` from the latest xtensa toolchain, even if an earlier version is used to build the firmware.

If `gdb` does not reach the `Thread 1 hit Temporary breakpoint ...` line, close and reopen `gdb`.


## Versions
There are two generations of PyJTAG boards:

1) V1R1 with a green PCB has three blocks of switches. Make sure SAFE_BOOT_SW is off on this version
2) V1R2 with a black PCB and two blocks of switches.

Both generation boards can be equipped with pogo pins that connect to the bottom of the development board and allow LTE debugging. There can either be pins that connect to a GPy or pins that conenct to a FiPy.

To reach the modem UART connect to `/dev/ttyUSB1`.

## Extra
A few more details are here: https://pycomiot.atlassian.net/wiki/spaces/FIR/pages/966295564/Usage+of+PyJTAG

