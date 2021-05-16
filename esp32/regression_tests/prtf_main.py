#!/usr/bin/env python3

import sys, os, glob, serial
import json
import time
sys.path.append(os.path.relpath("../../tools"))
import pyboard
import threading

threads = list()
boards = list()

class PyboardRestartError(Exception):
    pass

class PyboardMicroPythonError(Exception):
    pass

def serial_ports():
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        ports = glob.glob('/dev/ttyACM*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.usbmodemPy*')
    else:
        raise EnvironmentError('Unsupported platform')

    return ports

# Base class is the Pyboard class created by MicroPython project
# Pyboard class handles the instruction excution with the device
class PRTF_Pyboard(pyboard.Pyboard):
    device_id = ""
    device_messages = ""
    expect_restart = False

    def __init__(self, dev, baudrate=115200, user='micro', password='python', wait=0):
        self.device_id = dev["id"]
        self.expect_restart = False
        pyboard.Pyboard.__init__(self, dev["address"], baudrate, user, password, wait)

    def exec_reset(self):
        self.enter_raw_repl()
        self.exec_raw_no_follow("import machine")
        try:
            self.exec_raw_no_follow("machine.reset()")
        except:
            # Do nothing
            pass
        self.exit_raw_repl()
        return None
    
    # This is from the Base class, need to override here because data_consumer() needs extra argument
    def read_until(self, min_num_bytes, ending, timeout=10, data_consumer=None):
        # if data_consumer is used then data is not accumulated and the ending must be 1 byte long
        assert data_consumer is None or len(ending) == 1

        data = self.serial.read(min_num_bytes)
        if data_consumer:
            data_consumer(self, data)
        timeout_count = 0
        while True:
            if data.endswith(ending):
                break
            elif self.serial.inWaiting() > 0:
                new_data = self.serial.read(1)
                if data_consumer:
                    data_consumer(self, new_data)
                    data = new_data
                else:
                    data = data + new_data
                timeout_count = 0
            else:
                timeout_count += 1
                if timeout is not None and timeout_count >= 100 * timeout:
                    break
                time.sleep(0.01)
        return data

def handle_command(board):
    command = board.device_messages
    if(command == "PRTC:RESTART\n"):
        # The device will restart intentionally
        board.expect_restart = True
    else:
        # For the other commands just broadcast it to every other devices
        for b in boards:
            if(b != board):
                b.serial.write(command.encode())


def pycom_stdout_write_bytes(board, b):
    b = b.replace(b"\x04", b"")
    board.device_messages += b.decode()
    if(board.device_messages.endswith("\n")):
        if(board.device_messages.startswith("PRTC:")):
            handle_command(board)
            board.device_messages = ""
        else:
            if(board.expect_restart):
                # Detect the message printed out when the device restarts
                if(board.device_messages.endswith("Type \"help()\" for more information.\r\n")):
                    board.device_messages = ""
                    # This means the device restarted, need to handle it in the corresponding Thread
                    raise PyboardRestartError("Restarted!")
            else:
                # Enable the next line to print the output on the terminal on the fly
                print(board.device_id + " - " + board.device_messages, end="")
                output_file.write(board.device_id + " - " + board.device_messages)
                board.device_messages = ""

def execbuffer(board, buf):
    try:
        # timeout is specified in 0.01 sec increments (10 ms), wait maximum 10 minutes = 60.000 ms
        ret, ret_err = board.exec_raw(buf, timeout=10*60*100, data_consumer=pycom_stdout_write_bytes)
    except PyboardRestartError as er:
        # Indicate that execution has not finished yet, need to re-run the commands again
        raise er
    except pyboard.PyboardError as er:
        print(er)
        # Error happened in Pyboard class
        raise er
    except KeyboardInterrupt:
        # TODO: handle it correctly, reset all devices and then exit
        sys.exit(1)
    # This case happens e.g. on Ctrl+C sent to the device
    if ret_err:
        # Raise special Exception which will be handled in the thread
        raise PyboardMicroPythonError(ret_err.decode())

def board_thread(dev, test_suite):
    global boards, reset_between_tests

    board = PRTF_Pyboard(dev)
    boards.append(board)

    if(reset_between_tests == True):
        board.exec_reset()
        time.sleep(5)

    while True:
        #TODO: handle when this fails
        board.enter_raw_repl()
        # Execute the Pycom Regression Test framework
        with open("prtf_device.py", 'rb') as f:
            prt_file = f.read()
            execbuffer(board, prt_file)
        # Execute the Test Script
        with open(test_suite + "/" + dev["script"], 'rb') as f:
            pyfile = f.read()
            try:
                execbuffer(board, pyfile)
                # Execution finished successfully, no need to re-run
                break
            except PyboardRestartError:
                # Continue running, this exception was caused by an intentional reset
                board.expect_restart = False
            except PyboardMicroPythonError as e:
                message_bytes = "MicroPython Exception happened: " + str(e)
                pycom_stdout_write_bytes(board, message_bytes.encode())
                # KeyboardInterrupt means the device was terminated intentionally, do not consider it as an error
                if("KeyboardInterrupt:" not in str(e)):
                    # Other exception means problem happened, signal the other threads to stop
                    for other_board in boards:
                        # Send Ctrl+C Ctrl+C to the other devices which will stop script execution and the handler thread will be terminated
                        if(other_board != board):
                            other_board.serial.write(b"\r\x03\x03")
                # Reset current board to leave it in stable state
                board.exec_reset()
                # Execution finished
                break

        board.exit_raw_repl()
        # Wait 1 sec before starting over
        time.sleep(1)
 
    board.close()
    boards.remove(board)

# TODO: get whether the serial ports should be automatically assigned to the devices or the assignment should come from the configuration
auto_select_port = True
# TODO: get whether reset is needed between the Test Suits as an input parameter
reset_between_tests = True
# TODO: get the Test Suites to execute as an input parameter
test_suites = ("Deepsleep", "Reset", "BLE_Sleep", "BLE_General_1", "Socket_1", "WLAN_Sleep", "LoraRAW_1")

for test_suite in test_suites:
    # Wait 3 second between Test Suites to not overlap accidentally and/or wait reset to finish
    time.sleep(3)
    
    print("=== Executing Test Suite: {} ===".format(test_suite))

    # Parse the configuration file
    with open(test_suite + "/config.json") as f:
        cfg_data = json.load(f)

    # Open the output file
    output_file = open(test_suite + "/" + "output.txt", "w+")

    ports = serial_ports()

    if auto_select_port == True:
        if(len(ports) < len(cfg_data["devices"])):
            print("{}: Not enough open ports, skipping the test.".format(test_suite))
            continue
 
    # Execute the tests on the devices
    for dev in cfg_data["devices"]:
        if auto_select_port == True:
            cfg_data["devices"][dev]["address"] = ports[0]
            ports.pop(0)
        t = threading.Thread(target=board_thread, args=(cfg_data["devices"][dev], test_suite))
        t.start()
        threads.append(t)

    # Wait for all the threads executing the test on the devices to finish
    for t in threads:
        t.join()

    with open(test_suite + "/" + "expected.txt", 'r') as f:
        expected_data = f.read()

    output_file.seek(0)
    output_data = output_file.read() 
    output_file.close()

    result = "FAILED"
    if expected_data == output_data:
        result = "PASSED"
    
    print("=== Result of {}: {} ===".format(test_suite, result))

    threads.clear()
    boards.clear()







