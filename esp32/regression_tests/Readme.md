# Short readme for how to use the Pycom Regression Test Framework (PRTF)

## Starting the regression tests
Connect the devices via serial ports to the computer and execute the `prtf_main.py` script, it executes all the test suites one by one.

## Configure the test execution
The test execution can be influenced by modifying the following parameters in `prtf_main.py`:
* `auto_select_port`: sets whether the serial ports should be automatically assigned to the devices or the assignment should come from the configuration of the test suites.
* `reset_between_tests`: sets whether reset is needed between the test suits.
* `test_suites`: the Test Suites to execute.

## Adding a new test suite
A new test suite can be added by creating a dedicated folder for it and adding the following necessary files:
* `Device folder`: For each `device` a separate folder must be created containing the script to execute during the test.
* `config.json`: file containing the configuration of the test suite. For every `device` a configuration entry must exist:
    * `id`: identifier of the device used during the test.
    * `address`: serial port where the device is connected to. This assignment only matters if `auto_select_port` is set to False.
    * `script`: relative path of the test script to execute on this device, e.g.: `client/main.py` where `client` is a `Device folder` and `main.py` is the script to execute.
* `expected.txt`: expected output of the test suite used to determine whether the test passed or failed.

## Pycom Regression Test Command (PRTC)
The test execution of the devices can be syncronized using Pycom Regression Test Commands (PRTC).
A command can be sent by a device and other devices can wait for it using the following APIs:
* `prtf_send_command(command)` - API to send a command to every other devices
* `prtf_wait_for_command(command)` - API to wait for a command

The following command are supported:
* `PRTF_COMMAND_START` - a device should use this command to indicate to the other devices that they should start.
* `PRTF_COMMAND_GO` - a device should use this command to indicate to the other devices that they should continue test execution.
* `PRTF_COMMAND_STOP` - a device should use this command to indicate to the other devices that they should stop and finish the test.
* `PRTF_COMMAND_RESTART` - a device should use this command to indicate to the *Pycom Regression Test Framework* that an intentional restart is going to happen and the test execution should continue after the restart.

**Example for sending a PRTC**

```
from network import LoRa
import socket
import time

print("Starting...")

lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868)
s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)

for i in range(10):
    if i == 0:
        print("Notifying the other device to start")
        prtf_send_command(PRTF_COMMAND_START)
    recv = s.recv(4)
    print(recv.decode())
    s.send('Pong')

s.close()
```

**Example for waiting for a PRTC**

```
from network import LoRa
import socket
import time

# Wait for the other device to setup
prtf_wait_for_command(PRTF_COMMAND_START)

print("Starting...")

lora = LoRa(mode=LoRa.LORA, region=LoRa.EU868  )
s = socket.socket(socket.AF_LORA, socket.SOCK_RAW)
s.setblocking(True)

for i in range(10):
    s.send('Ping')
    recv = s.recv(4)
    print(recv.decode())
    time.sleep(1)

s.close()
```

## Checking the results
When `prtf_main.py` is being executed, the result of each executed test suite is printed out on the screen together with the actual output from the devices.

Messages with prefix `PRTF INFO:` are info messages from the Pycom Regression Test Framework and not compared against the `expected.txt`.

Messages from the devices under test are prefixed with their `id` configured in `json.config`.
To disable printing out the messages from the devices the relevant line in `prtf_main.py\pycom_stdout_write_bytes` should be disabled.

**Example - messages from devices are printed:**

```
PRTF INFO: executing Test Suite: Deepsleep
PRTF INFO: Device - /dev/ttyACM0
PRTF INFO: Device - Resetting the device.
Device - Starting...
Device - Going to deepsleep for 5 seconds...
Device - Starting...
Device - Awake from deepsleep
PRTF INFO: result of Deepsleep: PASSED
PRTF INFO: executing Test Suite: Reset
PRTF INFO: Device - /dev/ttyACM0
PRTF INFO: Device - Resetting the device.
Device - Starting...
Device - Resetting the device...
Device - Starting...
Device - Alive after reset
PRTF INFO: result of Reset: PASSED
PRTF INFO: executing Test Suite: BLE_Sleep
PRTF INFO: Client - /dev/ttyACM0
PRTF INFO: Server - /dev/ttyACM1
PRTF INFO: Client - Resetting the device.
PRTF INFO: Server - Resetting the device.
Client - Starting...
Client - Coex register schm btdm cb faild
Server - Starting...
Server - Coex register schm btdm cb faild
Server - Advertisement has been started.
Server - Client connected
Client - Connection status with My_BLE_Server: True
Client - Going to sleep for 1 second...
Server - Client disconnected
Client - Coex register schm btdm cb faild
Server - Client connected
Client - Connection status with My_BLE_Server: True
Server - Client disconnected
PRTF INFO: result of BLE_Sleep: PASSED
PRTF INFO: executing Test Suite: Socket_1
PRTF INFO: Socket_1 - not enough open ports, skipping the test.
```

**Example - messages from devices are NOT printed:**

```
PRTF INFO: executing Test Suite: Deepsleep
PRTF INFO: Device - /dev/ttyACM0
PRTF INFO: Device - Resetting the device.
PRTF INFO: result of Deepsleep: PASSED
PRTF INFO: executing Test Suite: Reset
PRTF INFO: Device - /dev/ttyACM0
PRTF INFO: Device - Resetting the device.
PRTF INFO: result of Reset: PASSED
PRTF INFO: executing Test Suite: BLE_Sleep
PRTF INFO: Client - /dev/ttyACM0
PRTF INFO: Server - /dev/ttyACM1
PRTF INFO: Client - Resetting the device.
PRTF INFO: Server - Resetting the device.
PRTF INFO: result of BLE_Sleep: PASSED
PRTF INFO: executing Test Suite: BLE_General_1
PRTF INFO: Client - /dev/ttyACM0
PRTF INFO: Server - /dev/ttyACM1
PRTF INFO: Client - Resetting the device.
PRTF INFO: Server - Resetting the device.
PRTF INFO: result of BLE_General_1: PASSED
PRTF INFO: executing Test Suite: Socket_1
PRTF INFO: Socket_1 - not enough open ports, skipping the test.
PRTF INFO: executing Test Suite: WLAN_Sleep
PRTF INFO: Client - /dev/ttyACM0
PRTF INFO: Access Point - /dev/ttyACM1
PRTF INFO: Client - Resetting the device.
PRTF INFO: Access Point - Resetting the device.
PRTF INFO: result of WLAN_Sleep: PASSED
PRTF INFO: executing Test Suite: LoraRAW_1
PRTF INFO: Node 1 - /dev/ttyACM0
PRTF INFO: Node 2 - /dev/ttyACM1
PRTF INFO: Node 1 - Resetting the device.
PRTF INFO: Node 2 - Resetting the device.
PRTF INFO: result of LoraRAW_1: PASSED
```