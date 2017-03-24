#!/usr/bin/env python

"""
pyboard interface

This module provides the Pyboard class, used to communicate with and
control the pyboard over a serial USB connection.

Example usage:

1. first import this file with:
    import pyboard

2. Then create a conection:

    in case of a serial port:
        pyb = pyboard.Pyboard('/dev/ttyACM0')

    or in Windows:
        pyb = pyboard.Pyboard('COM2')    

    For telnet connections:
        pyb = pyboard.Pyboard('192.168.1.1')

    Or even (if the name is defined in the DNS or the 'hosts' file:
        pyb = pyboard.Pyboard('mypyboard')

    You can also include a custom port number:
        pyb = pyboard.Pyboard('mypyboard:2323')

Then:

    pyb.enter_raw_repl()
    pyb.exec('pyb.LED(1).on()')
    pyb.exit_raw_repl()


Note: if using Python2 then pyb.exec must be written as pyb.exec_.
To run a script from the local machine on the board and print out the results:

    import pyboard
    pyboard.execfile('test.py', device='/dev/ttyACM0')

This script can also be run directly.  To execute a local script, use:

    ./pyboard.py test.py

Or:

    python pyboard.py test.py

"""

import sys
import time
import os
import stat
from threading import Thread

try:
    stdout = sys.stdout.buffer
except AttributeError:
    # Python2 doesn't have buffer attr
    stdout = sys.stdout

def stdout_write_bytes(b):
    b = b.replace(b"\x04", b"")
    stdout.write(b)
    stdout.flush()

def separate_address_port(string):
    import re
    match = re.match(r'([\w.]+)$|([\w.]+):(\d+)$', string)
    if match is None:
        raise PyboardError('\nInvalid address')
    address = match.group(1)
    port = 23
    if address == None:
        address = match.group(2)
        port = int(match.group(3))

    return address, port

class PyboardError(Exception):
    pass

def to_bytes(s):
    if type(s) == bytes:
        return s
    try:
        return bytes(s)
    except:
        return bytes(s, 'utf8')

class Periodic:
    def __init__(self, period, callback):
        self.thread = Thread(target=self.on_timer, args=(period, callback))
        self.run = True
        self.thread.start()

    def __del__(self):
        self.stop()

    def stop(self):
        self.run = False

    def on_timer(self, period, callback):
        while self.run == True:
            time.sleep(period)
            callback()


class Serial_connection:
    def __init__(self, device, baudrate=115200, connection_timeout=10):
        import serial
        delayed = False
        self.connected = False
        for attempt in range(connection_timeout + 1):
            try:
                try:
                    self.stream = serial.Serial(device, baudrate=baudrate, interCharTimeout=1)
                    self.__in_waiting = self.stream.inWaiting
                    self.reset_input_buffer = self.stream.flushInput
                except (TypeError):
                    # version 3 of the pyserial library changed everything
                    self.stream = serial.Serial(device, baudrate=baudrate, inter_byte_timeout=1)
                    self.__in_waiting = lambda: self.stream.in_waiting
                    self.reset_input_buffer = self.stream.reset_input_buffer
                self.__expose_stream_methods()
                self.connected = True
                if attempt != 0:
                    print('')   # to break the dotted line printed on retries
                break
            except (OSError, IOError): # Py2 and Py3 have different errors
                if connection_timeout == 0:
                    continue

                if attempt == 0 and connection_timeout != 0:
                    sys.stdout.write('Waiting {} seconds for pyboard '.format(connection_timeout))

                time.sleep(1)
                sys.stdout.write('.')
                sys.stdout.flush()

    def authenticate(self, user, password):
        # needs no authentication
        return True

    def keep_alive(self):
        return True

    def settimeout(self, value):
        self.stream.timeout = value

    def gettimeout(self):
        return self.stream.timeout

    def __expose_stream_methods(self):
        self.close = self.stream.close
        self.write = self.stream.write
        self.flush = self.stream.flush
        self.read = self.stream.read

    def read_until(self, ending, timeout):
        ''' implement a telnetlib-like read_until '''
        current_timeout = self.stream.timeout
        self.stream.timeout = 0.1

        timeout_count = 0
        data = bytearray()
        while True:
            if data.endswith(ending):
                break

            new_data = self.stream.read(1)
            data.extend(new_data)
            if new_data == '':
                timeout_count += 1
                if timeout_count >= 10 * timeout:
                    break

        self.stream.timeout = current_timeout
        return data

    def read_eager(self):
        ''' implement a telnetlib-like read_eager '''
        waiting = self.__in_waiting()
        if waiting == 0:
            return ''
        return self.stream.read(waiting)

    def read_some(self):
        ''' implement a telnetlib-like read_some '''
        waiting = self.__in_waiting()
        if waiting != 0:
            return self.stream.read(waiting)
        else:
            return self.stream.read(1)

    @staticmethod
    def is_serial_port(name):
        return name[:3] == 'COM' or (os.path.exists(name) == True and \
            stat.S_ISCHR(os.stat(name).st_mode) == True)

    def enable_binary(self):
        pass

    def disable_binary(self):
        pass

    def read_with_timeout(self, length, timeout):
        buf = b''
        original_timeout = self.gettimeout()
        self.settimeout(timeout)
        buf = self.stream.read(length)
        self.settimeout(original_timeout)
        return buf


class Telnet_connection:
    import telnetlib
    import socket

    def __init__(self, uri, connection_timeout=15, read_timeout=10):
        self.__read_timeout = read_timeout
        self.connected = False
        self.__pending_AYT_RESP = False
        try:
            address, port = separate_address_port(uri)
            self.stream = Telnet_connection.telnetlib.Telnet(address, port, timeout=connection_timeout)
            self.__socket = self.stream.get_socket()
            self.__socket.setsockopt(Telnet_connection.socket.IPPROTO_TCP,
                Telnet_connection.socket.TCP_NODELAY, 1)
            self.stream.set_option_negotiation_callback(self.__process_options)
            self.connected = True
            self.__expose_stream_methods()

        except Telnet_connection.socket.error:
            pass

    def settimeout(self, value):
        self.__socket.settimeout(value)

    def gettimeout(self):
        return self.__socket.gettimeout()

    def _wait_for_exact_text(self, remote_text):
        remote_text = to_bytes(remote_text)
        return remote_text in self.stream.read_until(remote_text, self.__read_timeout)

    def _wait_for_multiple_exact_text(self, remote_options):
        for i in range(0, len(remote_options)):
            remote_options[i] = to_bytes(remote_options[i])
        return self.stream.expect(remote_options, self.__read_timeout)[0]

    def _wait_and_respond(self, remote_text, response, delay_before=0):
        response = to_bytes(response)
        if self._wait_for_exact_text(remote_text) == True:
            if delay_before != 0:
                time.sleep(delay_before)
            self.stream.write(response + b'\r\n')
            return True
        else:
            return False

    def authenticate(self, user, password):
        if self._wait_and_respond(b'Login as:', user) == True and \
        self._wait_and_respond(b'Password:', password, 0.2) == True:
            status = self._wait_for_multiple_exact_text([
                'Type "help\(\)" for more information.',
                'Invalid credentials, try again.'])
            if status == 0:
                return True
            elif status == 1:
                raise PyboardError('Invalid credentials')
        return False

    def close(self):
        self.__socket.shutdown(Telnet_connection.socket.SHUT_RDWR)
        self.stream.close()

    def write(self, data):
        self.stream.write(data)
        return len(data)

    def __process_options(self, telnet_socket, command, option):
        if command == Telnet_connection.telnetlib.AYT:
            self.__pending_AYT_RESP = False

    def keep_alive(self):
        if self.__pending_AYT_RESP == True:
            return False
        else:
            self.__socket.sendall(Telnet_connection.telnetlib.IAC +
                Telnet_connection.telnetlib.AYT)
            self.__pending_AYT_RESP = True
            return True

    def __expose_stream_methods(self):
        self.read_until = self.stream.read_until
        self.read_eager = self.stream.read_eager
        self.read_some = self.stream.read_some

    def __set_mode(self, mode_char):
        self.__socket.sendall(Telnet_connection.telnetlib.IAC +
        mode_char + Telnet_connection.telnetlib.BINARY)
    
    def enable_binary(self):
        self.__set_mode(Telnet_connection.telnetlib.WILL)
        self.__set_mode(Telnet_connection.telnetlib.DO)
        
    def disable_binary(self):
        self.__set_mode(Telnet_connection.telnetlib.WONT)
        self.__set_mode(Telnet_connection.telnetlib.DONT)

    def flush(self):
        pass

class Socket_connection:
    import socket

    def __init__(self, host):
        self.connected = False
        try:
            self.stream = Socket_connection.socket.socket(Socket_connection.socket.AF_INET, Socket_connection.socket.SOCK_STREAM)
            self.stream.connect(separate_address_port(host))
            self.__expose_stream_methods()
            self.connected = True
        except Socket_connection.socket.error:
            pass

    def __expose_stream_methods(self):
        self.write = self.stream.send

    def close(self):
        self.stream.shutdown(Socket_connection.socket.SHUT_RDWR)
        self.stream.close()

    def settimeout(self, value):
        self.stream.settimeout(value)

    def gettimeout(self):
        return self.stream.gettimeout()

    def keep_alive(self):
        return True

    def authenticate(self, user, password):
        # needs no authentication
        return True

    def read(self, length):
        buf = b''
        while length > 0:
            new_data = self.stream.recv(length % 4096) #todo: properly manage disconnections
            buf += new_data
            length -= len(new_data)
        return buf

    def read_with_timeout(self, length, timeout):
        buf = b''
        original_timeout = self.gettimeout()
        self.settimeout(timeout)
        buf = self.stream.recv(length)
        self.settimeout(original_timeout)
        return buf

class Pyboard:
    LOST_CONNECTION = 1

    def __init__(self, device, baudrate=115200, user='micro', password='python', connection_timeout=0, keep_alive=0):
        self.__device = None
        self._connect(device, baudrate, user, password, connection_timeout, keep_alive, False)

    def close_dont_notify(self):
        if self.connected == False:
            return
        try:
            self.keep_alive_interval.stop()
            self.connected = False
            self.connection.close()
        except:
            # the connection might not exist, so ignore this one
            pass

    def close(self):
        if self.connected == False:
            return
        self.close_dont_notify()
        try:
            self.__disconnected_callback()
        except:
            pass

    def get_connection_type(self):
        return self.__connectionType

    def get_username_password(self):
        return (self.__username, self.__password)

    def set_disconnected_callback(self, callback):
        self.__disconnected_callback = callback

    def flush(self):
        # flush input (without relying on serial.flushInput())
        while self.connection.read_eager():
            pass

    def _connect(self, device, baudrate=115200, user='micro', password='python', connection_timeout=0, keep_alive=0, raw=False):
        self.connected = False
        if self.__device == None:
            self.__device = device
            self.__baudrate = baudrate
            self.__username = user
            self.__password = password
            self.__connection_timeout = connection_timeout

        if Serial_connection.is_serial_port(self.__device) == True:
            self.__connectionType = 'serial'
            self.connection = Serial_connection(self.__device, baudrate=self.__baudrate, connection_timeout=self.__connection_timeout)
        else:
            if raw == False:
                self.__connectionType = 'telnet'
                self.connection = Telnet_connection(self.__device, connection_timeout=self.__connection_timeout)
            else:
                self.__connectionType = 'socket'
                self.connection = Socket_connection(self.__device)
        if self.connection.connected == False:
            raise PyboardError('\nFailed to establish a connection with the board at: ' + self.__device)

        if self.connection.authenticate(self.__username, self.__password) == False:
            raise PyboardError('\nFailed to authenticate with the board at: ' + self.__device)

        if keep_alive != 0:
            self.keep_alive_interval = Periodic(keep_alive, self._keep_alive)

        self.connected = True

    def _keep_alive(self):
        if self.connected == False:
            return
        try:
            if self.connection.keep_alive() == False:
                self.__disconnected_callback(Pyboard.LOST_CONNECTION)
        except:
             self.__disconnected_callback(Pyboard.LOST_CONNECTION)

    def check_connection(self):
        self._keep_alive()
        return self.connected

    def read_until(self, ending, timeout=10, data_consumer=None):
        if data_consumer == None:
            data = self.connection.read_until(ending, timeout)
        else:
            data = bytearray()
            cycles = int(timeout / 0.1)
            for i in range(1, cycles):
                new_data = self.connection.read_until(ending, 0.1)
                data.extend(new_data)
                data_consumer(new_data)
                if new_data.endswith(ending):
                    break
        return data

    def enter_raw_repl(self):
        self.enter_raw_repl_no_reset()
        self.flush()
        self.connection.write(b'\x04') # ctrl-D: soft reset
        data = self.read_until(b'soft reboot\r\n')
        if not data.endswith(b'soft reboot\r\n'):
            print(data)
            raise PyboardError('could not enter raw repl')
        # By splitting this into 2 reads, it allows boot.py to print stuff,
        # which will show up after the soft reboot and before the raw REPL.
        data = self.read_until(b'raw REPL; CTRL-B to exit\r\n')
        if not data.endswith(b'raw REPL; CTRL-B to exit\r\n'):
            print(data)
            raise PyboardError('could not enter raw repl')


    def enter_raw_repl_no_reset(self):
        self.connection.write(b'\r\x03\x03') # ctrl-C twice: interrupt any running program

        self.flush()

        self.connection.write(b'\r\x01') # ctrl-A: enter raw REPL
        data = self.read_until(b'raw REPL; CTRL-B to exit\r\n')
        if not data.endswith(b'raw REPL; CTRL-B to exit\r\n'):
            print(data)
            raise PyboardError('could not enter raw repl')

    def exit_raw_repl(self):
        self.connection.write(b'\r\x02') # ctrl-B: enter friendly REPL

    def follow(self, timeout, data_consumer=None):
        # wait for normal output
        data = self.read_until(b'\x04', timeout=timeout, data_consumer=data_consumer)
        if not data.endswith(b'\x04'):
            raise PyboardError('timeout waiting for first EOF reception')
        data = data[:-1]

        # wait for error output
        data_err = self.read_until(b'\x04', timeout=timeout)
        if not data_err.endswith(b'\x04'):
            raise PyboardError('timeout waiting for second EOF reception')
        data_err = data_err[:-1]

        # return normal and error output
        return data, data_err

    def send(self, data):
        data = to_bytes(data)
        try:
            self.connection.write(data)
        except:
            self.close()

    def recv(self, callback):
        import socket

        self.want_exit_recv = False

        while 1:
            try:
                data = self.connection.read_some()
                if data and self.want_exit_recv == False:
                    callback(data)
                else:
                    break
            except socket.timeout:
                continue
            except:
                break
        if self.want_exit_recv == False:
            self.close()

    def exit_recv(self):
        self.want_exit_recv = True
        self.send("\x03") # Ctrl-C

    def _wait_for_exact_text(self, remote_text):
        remote_text = to_bytes(remote_text)
        return remote_text in self.read_until(remote_text)

    def reset(self):
        self.exit_raw_repl()
        if not self._wait_for_exact_text(b'Type "help()" for more information.\r\n'):
            raise PyboardError('could not enter reset')
        self.connection.write(b'\x04')
        if not self._wait_for_exact_text(b'PYB: soft reboot\r\n'):
            raise PyboardError('could not enter reset')

    def exec_raw_no_follow(self, command):
        command_bytes = to_bytes(command)

        # check we have a prompt
        data = self.read_until(b'>')
        if not data.endswith(b'>'):
            raise PyboardError('could not enter raw repl')

        # write command
        for i in range(0, len(command_bytes), 256):
            self.connection.write(command_bytes[i:min(i + 256, len(command_bytes))])
            self.connection.flush()
        self.connection.write(b'\x04')

        # check if we could exec command
        data = self.read_until(b'OK')
        if data != b'OK':
            raise PyboardError('could not exec command')

    def exec_raw(self, command, timeout=10, data_consumer=None):
        self.exec_raw_no_follow(command);
        return self.follow(timeout, data_consumer)

    def eval(self, expression):
        ret = self.exec_('print({})'.format(expression))
        ret = ret.strip()
        return ret

    def exec_(self, command):
        ret, ret_err = self.exec_raw(command)
        if ret_err:
            raise PyboardError('exception', ret, ret_err)
        return ret

    def execfile(self, filename):
        with open(filename, 'rb') as f:
            pyfile = f.read()
        return self.exec_(pyfile)

    def get_time(self):
        t = str(self.eval('pyb.RTC().datetime()'), encoding='utf8')[1:-1].split(', ')
        return int(t[4]) * 3600 + int(t[5]) * 60 + int(t[6])

# in Python2 exec is a keyword so one must use "exec_"
# but for Python3 we want to provide the nicer version "exec"
setattr(Pyboard, "exec", Pyboard.exec_)

def execfile(filename, device='/dev/ttyACM0', baudrate=115200, user='micro', password='python'):
    pyb = Pyboard(device, baudrate, user, password)
    pyb.enter_raw_repl()
    output = pyb.execfile(filename)
    stdout_write_bytes(output)
    pyb.exit_raw_repl()
    pyb.close()

def main():
    import argparse
    cmd_parser = argparse.ArgumentParser(description='Run scripts on the pyboard.')
    cmd_parser.add_argument('--device', default='/dev/ttyACM0', help='the serial device or the IP address of the pyboard')
    cmd_parser.add_argument('-b', '--baudrate', default=115200, help='the baud rate of the serial device')
    cmd_parser.add_argument('-u', '--user', default='micro', help='the telnet login username')
    cmd_parser.add_argument('-p', '--password', default='python', help='the telnet login password')
    cmd_parser.add_argument('-c', '--command', help='program passed in as string')
    cmd_parser.add_argument('-w', '--wait', default=0, type=int, help='seconds to wait for USB connected board to become available')
    cmd_parser.add_argument('--follow', action='store_true', help='follow the output after running the scripts [default if no scripts given]')
    cmd_parser.add_argument('files', nargs='*', help='input files')
    args = cmd_parser.parse_args()

    def execbuffer(buf):
        try:
            pyb = Pyboard(args.device, args.baudrate, args.user, args.password, args.wait)
            pyb.enter_raw_repl()
            ret, ret_err = pyb.exec_raw(buf, timeout=None, data_consumer=stdout_write_bytes)
            pyb.exit_raw_repl()
            pyb.close()
        except PyboardError as er:
            print(er)
            sys.exit(1)
        except KeyboardInterrupt:
            sys.exit(1)
        if ret_err:
            stdout_write_bytes(ret_err)
            sys.exit(1)

    if args.command is not None:
        execbuffer(to_bytes(args.command))

    for filename in args.files:
        with open(filename, 'rb') as f:
            pyfile = f.read()
            execbuffer(pyfile)

    if args.follow or (args.command is None and len(args.files) == 0):
        try:
            pyb = Pyboard(args.device, args.baudrate, args.user, args.password, args.wait)
            ret, ret_err = pyb.follow(timeout=None, data_consumer=stdout_write_bytes)
            pyb.close()
        except PyboardError as er:
            print(er)
            sys.exit(1)
        except KeyboardInterrupt:
            sys.exit(1)
        if ret_err:
            stdout_write_bytes(ret_err)
            sys.exit(1)

if __name__ == "__main__":
    main()
