#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

import pyboard
import os

def load_board_script():
    with open(os.path.dirname(os.path.realpath(__file__)) + '/lopy_initial_test_board_script.py', 'rb') as input:
        remote_code = input.read()
    return remote_code

def run_program_script(pyb):
    flash_test_code = load_board_script()
    pyb.enter_raw_repl_no_reset()
    pyb.exec_raw_no_follow(flash_test_code)

def detect_test_status(pyb):
    if pyb._wait_for_exact_text("Test "):
        status = pyb.read_until("\n")
        if "OK" in status:
            return True
        else:
            return False
    else:
        return False

def test_board(serial_port):
    pyb = pyboard.Pyboard(serial_port)
    run_program_script(pyb)
    return detect_test_status(pyb)
