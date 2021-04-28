import sys, os
import json
import time
sys.path.append(os.path.relpath("../../tools"))
import pyboard
import threading

boards = list()

class PyboardRestartError(Exception):
    pass

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
                if(board.device_messages.startswith("ets ") and board.device_messages.endswith("information.\r\n")):
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
        board.expect_restart = False
        # Indicate that execution has not finished yet, need to re-run the commands again
        return True
    except pyboard.PyboardError as er:
        print(er)
        board.close()
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(1)
    if ret_err:
        board.exit_raw_repl()
        board.close()
        pycom_stdout_write_bytes(board, ret_err)
        sys.exit(1)
    # Indicate that execution successfully finished, no need to re-run the commands again
    return False

def thread_function(dev, test_suite):
    global boards
    board = PRTF_Pyboard(dev)
    boards.append(board)
    run = True

    while(run):
        board.enter_raw_repl()
        # Execute the Pycom Regression Test framework
        with open("prtf_device.py", 'rb') as f:
            prt_file = f.read()
            execbuffer(board, prt_file)
        # Execute the Test Script
        with open(test_suite + "/" + dev["script"], 'rb') as f:
            pyfile = f.read()
            run = execbuffer(board, pyfile)  
        board.exit_raw_repl()
 
    boards.remove(board)

# TODO: get the Test Suites to execute as input parameter
test_suites = ("Deepsleep", "Reset", "BLE_Sleep", "BLE_General_1", "Socket_1", "WLAN_Sleep", "LoraRAW_1")

for test_suite in test_suites:
    # Wait 1 second between Test Suites to not overlap accidentally
    time.sleep(1)
    
    print("=== Executing Test Suite: {} ===".format(test_suite))

    # Parse the configuration file
    with open(test_suite + "/config.json") as f:
        cfg_data = json.load(f)

    # Open the output file
    output_file = open(test_suite + "/" + "output.txt", "w+")

    threads = list()
    # Execute the tests on the devices
    for dev in cfg_data["devices"]:
        t = threading.Thread(target=thread_function, args=(cfg_data["devices"][dev], test_suite))
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







