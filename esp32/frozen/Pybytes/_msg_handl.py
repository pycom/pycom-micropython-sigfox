'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

try:
    from pybytes_constants import MQTTConstants as mqttConst
except:
    from _pybytes_constants import MQTTConstants as mqttConst
try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

import time
import socket
import _thread
import select
import struct


class MsgHandler:

    def __init__(
            self,
            receive_callback,
            connect_helper,
            receive_timeout=3000,
            reconnectMethod=None
    ):
        self._host = ""
        self._port = -1
        self._sock = None
        self._output_queue_size = -1
        self._output_queue_dropbehavior = -1
        self._mqttOperationTimeout = 0
        self._connection_state = mqttConst.STATE_DISCONNECTED
        self._conn_state_mutex = _thread.allocate_lock()
        self._poll = select.poll()
        self._output_queue = []
        self._out_packet_mutex = _thread.allocate_lock()
        _thread.stack_size(10240)
        _thread.start_new_thread(self._io_thread_func, ())
        _thread.stack_size(8192)
        self._recv_callback = receive_callback
        self._connect_helper = connect_helper
        self._pingSent = False
        self._ping_interval = 20
        self._waiting_ping_resp = False
        self._ping_cutoff = 3
        self._receive_timeout = receive_timeout
        self.reconnectMethod = reconnectMethod

    def setOfflineQueueConfiguration(self, queueSize, dropBehavior):
        self._output_queue_size = queueSize
        self._output_queue_dropbehavior = dropBehavior

    def setEndpoint(self, srcHost, srcPort):
        self._host = srcHost
        self._port = srcPort

    def setOperationTimeout(self, timeout):
        self._mqttOperationTimeout = timeout

    def createSocketConnection(self):
        self._conn_state_mutex.acquire()
        self._connection_state = mqttConst.STATE_CONNECTING
        self._conn_state_mutex.release()
        try:
            if self._sock:
                self._poll.unregister(self._sock)
                self._sock.close()
                self._sock = None

            self._sock = socket.socket()
            self._sock.settimeout(30)
            self._sock.connect(
                socket.getaddrinfo(self._host, self._port)[0][-1]
            )
            self._poll.register(self._sock, select.POLLIN)
        except socket.error as err:
            print_debug(2, "Socket create error: {0}".format(err))
            self._conn_state_mutex.acquire()
            self._connection_state = mqttConst.STATE_DISCONNECTED
            self._conn_state_mutex.release()
            return False
        return True

    def disconnect(self):
        if self._sock:
            self._sock.close()
            self._sock = None

    def isConnected(self):
        connected = False
        self._conn_state_mutex.acquire()
        if self._connection_state == mqttConst.STATE_CONNECTED:
            connected = True
        self._conn_state_mutex.release()

        return connected

    def setConnectionState(self, state):
        self._conn_state_mutex.acquire()
        self._connection_state = state
        self._conn_state_mutex.release()

    def _drop_message(self):
        if self._output_queue_size == -1:
            return False
        elif (self._output_queue_size == 0) and (self._connection_state == mqttConst.STATE_CONNECTED): # noqa
            return False
        else:
            return True if len(self._output_queue) >= self._output_queue_size else False # noqa

    def push_on_send_queue(self, packet):
        if self._drop_message():
            if self._output_queue_dropbehavior == mqttConst.DROP_OLDEST:
                self._out_packet_mutex.acquire()
                if self._out_packet_mutex.locked():
                    self._output_queue.pop(0)
                self._out_packet_mutex.release()
            else:
                return False

        self._out_packet_mutex.acquire()
        if self._out_packet_mutex.locked():
            self._output_queue.append(packet)
        self._out_packet_mutex.release()

        return True

    def priority_send(self, packet):
        msg_sent = False
        self._out_packet_mutex.acquire()
        msg_sent = self._send_packet(packet)
        self._out_packet_mutex.release()

        return msg_sent

    def _receive_packet(self):
        try:
            if not self._poll.poll(self._receive_timeout):
                return False
        except Exception:
            return False

        # Read message type
        try:
            self._sock.setblocking(False)
            msg_type = self._sock.recv(1)
        except socket.error:
            self.disconnect()
            self.reconnectMethod()
            return False
        else:
            if len(msg_type) == 0:
                return False
            msg_type = struct.unpack("!B", msg_type)[0]
            self._sock.setblocking(True)

        # Read payload length

        """
        using the old header checking to read the
        payload length of the received packet
        """

        bytes_remaining = 0
        read_bytes = 0
        payload = b''
        sh = 0
        while True:
            try:
                b = self._sock.read(1)[0]
            except Exception:
                return False
            bytes_remaining |= (b & 0x7f) << sh
            if not b & 0x80:
                break
            sh += 7

        # Read payload
        try:
            if self._sock:
                read_bytes = bytes_remaining
                while read_bytes > 0:
                    new_payload = self._sock.recv(read_bytes)
                    payload += new_payload
                    read_bytes -= len(new_payload)
                    new_payload = b''
        except socket.error:
            return False
        return self._recv_callback(msg_type, payload)

    def _send_pingreq(self):
        pkt = struct.pack('!BB', mqttConst.MSG_PINGREQ, 0)
        return self.priority_send(pkt)

    def setPingFlag(self, flag):
        self._pingSent = flag

    def _send_packet(self, packet):
        written = -1
        try:
            if self._sock:
                written = self._sock.write(packet)
                if(written is None):
                    written = -1
                else:
                    print_debug(2, 'Packet sent. (Length: %d)' % written)
        except socket.error as err:
            print_debug(2, 'Socket send error {0}'.format(err))
            return False

        return True if len(packet) == written else False

    def _verify_connection_state(self):
        elapsed = time.time() - self._start_time
        if not self._waiting_ping_resp and elapsed > self._ping_interval:
            if self._connection_state == mqttConst.STATE_CONNECTED:
                self._pingSent = False
                self._send_pingreq()
                self._waiting_ping_resp = True
            elif self._connection_state == mqttConst.STATE_DISCONNECTED:
                self._connect_helper()

            self._start_time = time.time()
        elif self._waiting_ping_resp and (self._connection_state == mqttConst.STATE_CONNECTED or elapsed > self._mqttOperationTimeout): # noqa
            if not self._pingSent:
                if self._ping_failures <= self._ping_cutoff:
                    self._ping_failures += 1
                else:
                    self._connect_helper()
            else:
                self._ping_failures = 0

            self._start_time = time.time()
            self._waiting_ping_resp = False

    def _io_thread_func(self):
        self._start_time = time.time()
        self._ping_failures = 0
        while True:
            self._out_packet_mutex.acquire()
            if self._ping_failures == 0:
                if self._out_packet_mutex.locked() and len(self._output_queue) > 0: # noqa
                    packet = self._output_queue[0]
                    if self._send_packet(packet):
                        self._output_queue.pop(0)
                    elif self.reconnectMethod is not None:
                        self._output_queue.pop(0)
                        self.disconnect()
                        self.reconnectMethod()
            self._out_packet_mutex.release()
            self._receive_packet()
            time.sleep(1)
