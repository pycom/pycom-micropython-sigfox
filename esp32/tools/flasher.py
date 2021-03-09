#!/usr/bin/env python
#
# Copyright (c) 2021, Pycom Limited.
#
# This software is licensed under the GNU GPL version 3 or any
# later version, with permitted additional terms. For more information
# see the Pycom Licence v1.0 document supplied with this file, or
# available at https://www.pycom.io/opensource/licensing
#

"""
Flash the ESP32 (bootloader, partitions table and factory app).

How to call esptool:

python esptool.py '--chip', 'esp32', '--port', /dev/ttyUSB0, '--baud', '921600', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', bootloader.bin, '0x8000', partitions.bin, '0x10000', application.bin, '0x3FF000', 'config_no_wifi.bin'

"""

from esptool import ESP32ROM
import os
import sys
import struct
import sqlite3
import argparse
import subprocess
import threading
import time
import fw_version
import csv

working_threads = {}
macs_db = None
wmacs = {}

DB_MAC_UNUSED = 0
DB_MAC_ERROR = -1
DB_MAC_LOCK = -2
DB_MAC_OK = 1

def open_macs_db(db_filename):
    global macs_db
    if not os.path.exists(db_filename):
        print("MAC addresses database not found")
        sys.exit(1)
    macs_db = sqlite3.connect(db_filename)

def fetch_MACs(number):
    return [x[0].encode('ascii', 'ignore') for x in macs_db.execute("select mac from macs where status = 0 order by rowid asc limit ?", (number,)).fetchall()]

def set_mac_status(mac, wmac, status):
    macs_db.execute("update macs set status = ?, last_touch = strftime('%s','now'), wmac = ? where mac = ?", (status, wmac, mac))
    macs_db.commit()

def print_exception(e):
    print ('Exception: {}, on line {}'.format(e, sys.exc_info()[-1].tb_lineno))

def erase_flash(port, command):
    global working_threads
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    num_erases = 0

    # poll the process for new output until finished
    while True:
        nextline = process.stdout.readline()
        if nextline == '' and process.poll() != None:
            break
        if 'Chip erase completed successfully' in nextline:
            sys.stdout.write('Board erased OK on port %s\n' % port)
            num_erases += 1
        sys.stdout.flush()

    # hack to give feedback to the main thread
    if process.returncode != 0 or num_erases != 1:
        working_threads[port] = None

def read_wlan_mac(port, command):
    global working_threads
    global wmacs
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    mac_read = False

    # poll the process for new output until finished
    while True:
        nextline = process.stdout.readline()
        if nextline == '' and process.poll() != None:
            break
        if 'MAC: ' in nextline:
            wmacs[port] = nextline[5:-1].replace(":", "-").upper()
            sys.stdout.write('MAC address %s read OK on port %s\n' % (nextline[5:-1], port))
            mac_read = True
        sys.stdout.flush()

    # hack to give feedback to the main thread
    if process.returncode != 0 or not mac_read:
        working_threads[port] = None


def set_vdd_sdio_voltage(port, command):
    global working_threads
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    # poll the process for new output until finished
    while True:
        nextline = process.stdout.readline()
        if nextline == '' and process.poll() != None:
            break
        if 'VDD_SDIO setting complete' in nextline:
            sys.stdout.write('Board VDD_SDIO Voltage configured OK on port %s\n' % port)
        sys.stdout.flush()

    # hack to give feedback to the main thread
    if process.returncode != 0:
        working_threads[port] = None

def flash_firmware(port, command):
    global working_threads
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    num_hashes = 0

    # poll the process for new output until finished
    while True:
        nextline = process.stdout.readline()
        if nextline == '' and process.poll() != None:
            break
        if 'at 0x00001000' in nextline:
            sys.stdout.write('Bootloader programmed OK on port %s\n' % port)
        elif 'at 0x00008000' in nextline:
            sys.stdout.write('Partition table programmed OK on port %s\n' % port)
        elif 'at 0x00010000' in nextline:
            sys.stdout.write('Application programmed OK on port %s\n' % port)
        elif 'Hash of data verified' in nextline:
            num_hashes += 1
        sys.stdout.flush()

    # hack to give feedback to the main thread
    if process.returncode != 0 or num_hashes != 3:
        working_threads[port] = None

def run_initial_test(port, board):
    global working_threads

    if board == 'LoPy':
        import run_initial_lopy_test as run_test
    elif board == 'LoPy4':
        import run_initial_lopy4_test as run_test
    elif board == 'SiPy':
        import run_initial_sipy_test as run_test
    else:
        import run_initial_wipy_test as run_test

    try:
        if not run_test.test_board(port):
            # same trick to give feedback to the main thread
            working_threads[port] = None
    except Exception:
        working_threads[port] = None

def flash_lpwan_mac(port, mac):
    import flash_lpwan_mac
    global working_threads

    try:
        if not flash_lpwan_mac.program_board(port, mac):
            # same trick to give feedback to the main thread
            working_threads[port] = None
    except Exception:
            working_threads[port] = None

def run_final_test(port, board, mac):
    if board == 'LoPy':
        import run_final_lopy_test as run_test
    elif board == 'LoPy4':
        import run_final_lopy4_test as run_test
    else:
        import run_final_sipy_test as run_test

    try:
        if not run_test.test_board(port, mac, fw_version.number):
            # same trick to give feedback to the main thread
            working_threads[port] = None
    except Exception:
            working_threads[port] = None

def run_qa_test(port, board):
    global working_threads

    if board == 'LoPy':
        import run_qa_lopy_test as run_test
    elif board == 'LoPy4':
        import run_qa_lopy4_test as run_test
    elif board == 'SiPy':
        import run_qa_sipy_test as run_test
    else:
        import run_qa_wipy_test as run_test

    try:
        if not run_test.test_board(port, fw_version.number):
            # same trick to give feedback to the main thread
            working_threads[port] = None
    except Exception:
        working_threads[port] = None

def main():
    cmd_parser = argparse.ArgumentParser(description='Flash the ESP32 and optionally run a small test on it.')
    cmd_parser.add_argument('--esptool', default=None, help='the path to the esptool')
    cmd_parser.add_argument('--espefuse', default=None, help='the path to the espefuse')
    cmd_parser.add_argument('--boot', default=None, help='the path to the bootloader binary')
    cmd_parser.add_argument('--table', default=None, help='the path to the partitions table')
    cmd_parser.add_argument('--app', default=None, help='the path to the application binary')
    cmd_parser.add_argument('--macs', default="macs.db", help='the path to the MAC addresses database')
    cmd_parser.add_argument('--ports', default=['/dev/ttyUSB0'], nargs='+', help="the serial ports of the ESP32's to program")
    cmd_parser.add_argument('--erase', default=None, help='set to True to erase the boards first')
    cmd_parser.add_argument('--qa', action='store_true', help='just do some quality asurance test')
    cmd_parser.add_argument('--board', default='LoPy', help='identifies the board to be flashed and tested')
    cmd_parser.add_argument('--revision', default='1', help='identifies the hardware revision')
    cmd_args = cmd_parser.parse_args()

    global working_threads
    global wmacs
    output = ""
    ret = 0
    global_ret = 0

    if cmd_args.qa:
        raw_input("Please reset all the boards, wait until the LED starts blinking and then press enter...")
        time.sleep(2.5)   # wait for the board to reset
        try:
            for port in cmd_args.ports:
                working_threads[port] = threading.Thread(target=run_qa_test, args=(port, cmd_args.board))
                working_threads[port].start()

            for port in cmd_args.ports:
                if working_threads[port]:
                    working_threads[port].join()

            for port in cmd_args.ports:
                if working_threads[port] == None:
                    print("Failed QA test on board connected to %s" % port)
                    ret = 1

        except Exception as e:
            ret = 1
            print_exception(e)

        if ret == 0:
            print("=============================================================")
            print("QA test succeeded on all boards:-)")
            print("=============================================================")
        else:
            print("=============================================================")
            print("ERROR: Some boards failed the QA test!")
            print("=============================================================")
            global_ret = 1
    else:

        print("Reading the WLAN MAC address...")
        try:
            for port in cmd_args.ports:
                cmd = ['python', 'esptool.py', '--port', port, 'read_mac']
                working_threads[port] = threading.Thread(target=read_wlan_mac, args=(port, cmd))
                working_threads[port].start()

            for port in cmd_args.ports:
                if working_threads[port]:
                    working_threads[port].join()
            _ports = list(cmd_args.ports)
            for port in _ports:
                if working_threads[port] == None:
                    print("Error reading the WLAN MAC on the board on port %s" % port)
                    cmd_args.ports.remove(port)
                    ret = 1

        except Exception as e:
            ret = 1
            print_exception(e)

        if ret == 0:
            print("=============================================================")
            print("WLAN MAC address reading succeeded :-)")
            print("=============================================================")
        else:
            print("=============================================================")
            print("ERROR: WLAN MAC address reading failed in some boards!")
            print("=============================================================")
            global_ret = 1

        raw_input("Please reset all the boards and press enter to continue with the flashing process...")

        if int(cmd_args.revision) > 1:
            # program the efuse bits to set the VDD_SDIO voltage to 1.8V
            try:
                print('Configuring the VDD_SDIO voltage...')
                for port in cmd_args.ports:
                    cmd = ['python', cmd_args.espefuse, '--port', port, '--do-not-confirm', 'set_flash_voltage', '1.8V']
                    working_threads[port] = threading.Thread(target=set_vdd_sdio_voltage, args=(port, cmd))
                    working_threads[port].start()

                for port in cmd_args.ports:
                    if working_threads[port]:
                        working_threads[port].join()
                _ports = list(cmd_args.ports)
                for port in _ports:
                    if working_threads[port] == None:
                        print("Error setting the VDD_SDIO voltage on the board on port %s" % port)
                        cmd_args.ports.remove(port)
                        ret = 1

            except Exception as e:
                ret = 1
                print_exception(e)

            if ret == 0:
                print("=============================================================")
                print("VDD_SDIO voltage setting succeeded :-)")
                print("=============================================================")
            else:
                print("=============================================================")
                print("ERROR: VDD_SDIO voltage setting failed in some boards!")
                print("=============================================================")
                global_ret = 1

            raw_input("Please reset all the boards and press enter to continue with the flashing process...")
            time.sleep(1.0)   # wait for the board to reset
            working_threads = {}

        if cmd_args.erase:
            try:
                print('Erasing flash memory... (will take a few seconds)')
                for port in cmd_args.ports:
                    cmd = ['python', cmd_args.esptool, '--chip', 'esp32', '--port', port, '--baud', '921600',
                           'erase_flash']
                    working_threads[port] = threading.Thread(target=erase_flash, args=(port, cmd))
                    working_threads[port].start()

                for port in cmd_args.ports:
                    if working_threads[port]:
                        working_threads[port].join()
                _ports = list(cmd_args.ports)
                for port in _ports:
                    if working_threads[port] == None:
                        print("Error erasing board on port %s" % port)
                        cmd_args.ports.remove(port)
                        ret = 1

            except Exception as e:
                ret = 1
                print_exception(e)

            if ret == 0:
                print("=============================================================")
                print("Batch erasing succeeded :-)")
                print("=============================================================")
            else:
                print("=============================================================")
                print("ERROR: Batch erasing failed in some boards!")
                print("=============================================================")
                global_ret = 1

            raw_input("Please reset all the boards and press enter to continue with the flashing process...")
            time.sleep(1.0)   # wait for the board to reset
            working_threads = {}

        try:
            if cmd_args.board == 'LoPy' or cmd_args.board == 'SiPy' or cmd_args.board == 'LoPy4':
                open_macs_db(cmd_args.macs)
                macs_list = fetch_MACs(len(cmd_args.ports))
                if len(macs_list) < len(cmd_args.ports):
                    print("No enough remaining MAC addresses to use")
                    sys.exit(1)
                mac_per_port = {}
                i = 0
                for port in cmd_args.ports:
                    mac_per_port[port] = macs_list[i]
                    i += 1
            for port in cmd_args.ports:
                cmd = ['python', cmd_args.esptool, '--chip', 'esp32', '--port', port, '--baud', '921600',
                       'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', cmd_args.boot,
                       '0x8000', cmd_args.table, '0x10000', cmd_args.app]
                working_threads[port] = threading.Thread(target=flash_firmware, args=(port, cmd))
                working_threads[port].start()
            for port in cmd_args.ports:
                if working_threads[port]:
                    working_threads[port].join()
            _ports = list(cmd_args.ports)
            for port in _ports:
                if working_threads[port] == None:
                    print("Error programming board on port %s" % port)
                    cmd_args.ports.remove(port)
                    ret = 1
                else:
                    print("Board on port %s programmed OK" % port)
        except Exception as e:
            ret = 1
            print_exception(e)

        if ret == 0:
            print("=============================================================")
            print("Batch programming succeeded :-)")
            print("=============================================================")
        else:
            print("=============================================================")
            print("ERROR: Batch firmware programming failed on some boards!")
            print("=============================================================")
            global_ret = 1

        raw_input("Please place all boards into run mode, RESET them and then \n press enter to continue with the testing process...")
        time.sleep(5.0)   # wait for the board to reset

        working_threads = {}

        try:
            for port in cmd_args.ports:
                working_threads[port] = threading.Thread(target=run_initial_test, args=(port, cmd_args.board))
                working_threads[port].start()

            for port in cmd_args.ports:
                if working_threads[port]:
                    working_threads[port].join()
            _ports = list(cmd_args.ports)
            for port in _ports:
                if working_threads[port] == None:
                    print("Error testing board on port %s" % port)
                    cmd_args.ports.remove(port)
                    ret = 1
                elif cmd_args.board == 'WiPy':
                    print("Batch test OK on port %s, firmware version %s" % (port, fw_version.number))
                    with open('%s_Flasher_Results.csv' % (cmd_args.board), 'ab') as csv_file:
                        csv_writer = csv.writer(csv_file, delimiter=',')
                        csv_writer.writerow(['%s' % (cmd_args.board), '%s' % (fw_version.number), ' ', 'OK'])

        except Exception as e:
            ret = 1
            print_exception(e)

        if ret == 0:
            print("=============================================================")
            print("Batch testing succeeded :-)")
            print("=============================================================")
        else:
            print("=============================================================")
            print("ERROR: Batch testing failed in some boards!")
            print("=============================================================")
            global_ret = 1


        # only do the MAC programming and MAC verificacion for the LoPy, SiPy and LoPy4
        if cmd_args.board == 'LoPy' or cmd_args.board == 'SiPy' or cmd_args.board == 'LoPy4':
            print("Waiting before programming the LPWAN MAC address...")
            time.sleep(3.5)   # wait for the board to reset

            working_threads = {}

            try:
                for port in cmd_args.ports:
                    set_mac_status(mac_per_port[port], "", DB_MAC_LOCK) # mark them as locked, so if the script fails and doesn't get to save, they wont be accidentally reused
                    working_threads[port] = threading.Thread(target=flash_lpwan_mac, args=(port, mac_per_port[port]))
                    working_threads[port].start()

                for port in cmd_args.ports:
                    if working_threads[port]:
                        working_threads[port].join()

                _ports = list(cmd_args.ports)
                for port in _ports:
                    if working_threads[port] == None:
                        print("Error programing MAC address on port %s" % port)
                        cmd_args.ports.remove(port)
                        ret = 1
                        set_mac_status(mac_per_port[port], wmacs[port], DB_MAC_ERROR)

            except Exception as e:
                ret = 1
                print_exception(e)

            if ret == 0:
                print("=============================================================")
                print("Batch MAC programming succeeded :-)")
                print("=============================================================")
            else:
                print("=============================================================")
                print("ERROR: Batch MAC programming failed in some boards!")
                print("=============================================================")
                global_ret = 1

            print("Waiting for the board(s) to reboot...")
            time.sleep(4.5)   # wait for the board to reset

            working_threads = {}

            try:
                for port in cmd_args.ports:
                    working_threads[port] = threading.Thread(target=run_final_test, args=(port, cmd_args.board, mac_per_port[port]))
                    working_threads[port].start()

                for port in cmd_args.ports:
                    if working_threads[port]:
                        working_threads[port].join()

                for port in cmd_args.ports:
                    if working_threads[port] == None:
                        ret = 1
                        set_mac_status(mac_per_port[port], wmacs[port], DB_MAC_ERROR)
                        print("Error performing MAC address test on port %s" % port)
                    else:
                        set_mac_status(mac_per_port[port], wmacs[port], DB_MAC_OK)
                        print("Final test OK on port %s, firmware version %s, MAC address %s" % (port, fw_version.number, mac_per_port[port]))
                        with open('%s_Flasher_Results.csv' % (cmd_args.board), 'ab') as csv_file:
                            csv_writer = csv.writer(csv_file, delimiter=',')
                            csv_writer.writerow(['%s' % (cmd_args.board), '%s' % (fw_version.number), '%s' % (mac_per_port[port]), 'OK'])

            except Exception as e:
                ret = 1
                print_exception(e)

            if ret == 0:
                print("=============================================================")
                print("Final test succeeded on all boards :-)")
                print("=============================================================")
            else:
                print("=============================================================")
                print("ERROR: Some boards failed the final test!")
                print("=============================================================")
                global_ret = 1

            macs_db.close()

    sys.exit(global_ret)

if __name__ == "__main__":
    main()
