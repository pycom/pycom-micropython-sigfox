'''
umqtt is a simple MQTT client for MicroPython.
Original code: https://github.com/micropython/micropython-lib/tree/master/umqtt.simple

Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''


try:
    from msg_handl import MsgHandler as msgHandler
except:
    from _msg_handl import MsgHandler as msgHandler

try:
    from pybytes_constants import MQTTConstants as mqttConst
except:
    from _pybytes_constants import MQTTConstants as mqttConst
try:
    from pybytes_debug import print_debug
except:
    from _pybytes_debug import print_debug

import time
import struct
import _thread


class MQTTMessage:
    def __init__(self):
        self.timestamp = 0
        self.state = 0
        self.dup = False
        self.mid = 0
        self.topic = ""
        self.payload = None
        self.qos = 0
        self.retain = False


class MQTTCore:

    def __init__(
                self,
                clientID,
                cleanSession,
                protocol,
                receive_timeout=3000,
                reconnectMethod=None
            ):
        self.client_id = clientID
        self._cleanSession = cleanSession
        self._protocol = protocol
        self._user = ""
        self._password = ""
        self._connectdisconnectTimeout = 30
        self._mqttOperationTimeout = 5
        self._topic_callback_queue = []
        self._callback_mutex = _thread.allocate_lock()
        self._pid = 0
        self._subscribeSent = False
        self._unsubscribeSent = False
        self._msgHandler = msgHandler(
                self._recv_callback,
                self.connect,
                receive_timeout,
                reconnectMethod
            )

    def configEndpoint(self, srcHost, srcPort):
        self._msgHandler.setEndpoint(srcHost, srcPort)

    def connect(self):
        if not self._msgHandler.createSocketConnection():
            return False

        self._send_connect(self._cleanSession)

        # delay to check the state
        count_10ms = 0
        while(count_10ms <= self._connectdisconnectTimeout * 100 and not self._msgHandler.isConnected()):  # noqa
            count_10ms += 1
            time.sleep(0.01)

        return True if self._msgHandler.isConnected() else False

    def subscribe(self, topic, qos, callback):
        if (topic is None or callback is None):
            raise TypeError("Invalid subscribe values.")
        topic = topic.encode('utf-8')

        header = mqttConst.MSG_SUBSCRIBE | (1 << 1)
        pkt = bytearray([header])

        # packet identifier + len of topic (16 bits) + topic len + QOS
        pkt_len = 2 + 2 + len(topic) + 1
        pkt.extend(self._encode_varlen_length(pkt_len))  # len of the remaining

        self._pid += 1
        pkt.extend(self._encode_16(self._pid))
        pkt.extend(self._pascal_string(topic))
        pkt.append(qos)

        self._subscribeSent = False
        self._msgHandler.push_on_send_queue(pkt)

        count_10ms = 0
        while(count_10ms <= self._mqttOperationTimeout * 100 and not self._subscribeSent):  # noqa
            count_10ms += 1
            time.sleep(0.01)

        if self._subscribeSent:
            self._callback_mutex.acquire()
            self._topic_callback_queue.append((topic, callback))
            self._callback_mutex.release()
            return True

        return False

    def publish(self, topic, payload, qos, retain, dup=False, priority=False):
        topic = topic.encode('utf-8') if hasattr(topic, 'encode') else topic
        payload = payload.encode('utf-8') if hasattr(payload, 'encode') else payload  # noqa

        header = mqttConst.MSG_PUBLISH | (dup << 3) | (qos << 1) | retain
        pkt_len = (2 + len(topic) + (2 if qos else 0) + (len(payload)))

        pkt = bytearray([header])
        pkt.extend(self._encode_varlen_length(pkt_len))  # len of the remaining
        pkt.extend(self._pascal_string(topic))
        if qos:
            # todo: I don't think this is the way to deal with the packet id
            self._pid += 1
            pkt.extend(self._encode_16(self._pid))

        pkt = pkt + payload
        if priority:
            self._msgHandler.priority_send(pkt)
        else:
            self._msgHandler.push_on_send_queue(pkt)

    def _encode_16(self, x):
        return struct.pack("!H", x)

    def _pascal_string(self, s):
        return struct.pack("!H", len(s)) + s

    def _encode_varlen_length(self, length):
        i = 0
        buff = bytearray()
        while 1:
            buff.append(length % 128)
            length = length // 128
            if length > 0:
                buff[i] = buff[i] | 0x80
                i += 1
            else:
                break

        return buff

    def _topic_matches_sub(self, sub, topic):
        result = True
        multilevel_wildcard = False

        slen = len(sub)
        tlen = len(topic)

        if slen > 0 and tlen > 0:
            if (sub[0] == '$' and topic[0] != '$') or (topic[0] == '$' and sub[0] != '$'):  # noqa
                return False

        spos = 0
        tpos = 0

        while spos < slen and tpos < tlen:
            if sub[spos] == topic[tpos]:
                if tpos == tlen-1:
                    # Check for e.g. foo matching foo/#
                    if spos == slen-3 and sub[spos+1] == '/' and sub[spos+2] == '#':  # noqa
                        result = True
                        multilevel_wildcard = True
                        break

                spos += 1
                tpos += 1

                if tpos == tlen and spos == slen-1 and sub[spos] == '+':
                    spos += 1
                    result = True
                    break
            else:
                if sub[spos] == '+':
                    spos += 1
                    while tpos < tlen and topic[tpos] != '/':
                        tpos += 1
                    if tpos == tlen and spos == slen:
                        result = True
                        break

                elif sub[spos] == '#':
                    multilevel_wildcard = True
                    if spos+1 != slen:
                        result = False
                        break
                    else:
                        result = True
                        break

                else:
                    result = False
                    break

        if not multilevel_wildcard and (tpos < tlen or spos < slen):
            result = False

        return result

    def _remove_topic_callback(self, topic):
        deleted = False

        self._callback_mutex.acquire()
        for i in range(0, len(self._topic_callback_queue)):
            if self._topic_callback_queue[i][0] == topic:
                self._topic_callback_queue.pop(i)
                deleted = True
        self._callback_mutex.release()

        return deleted

    def unsubscribe(self, topic):
        self._unsubscribeSent = False
        self._send_unsubscribe(topic, False)

        count_10ms = 0
        while(count_10ms <= self._mqttOperationTimeout * 100 and not self._unsubscribeSent):  # noqa
            count_10ms += 1
            time.sleep(0.01)

        if self._unsubscribeSent:
            topic = topic.encode('utf-8')
            return self._remove_topic_callback(topic)

        return False

    def disconnect(self, force=False):
        pkt = struct.pack('!BB', mqttConst.MSG_DISCONNECT, 0)
        if force:
            self._msgHandler.priority_send(pkt)
        else:
            self._msgHandler.push_on_send_queue(pkt)
            time.sleep(self._connectdisconnectTimeout)
        self._msgHandler.disconnect()
        return True

    def _send_connect(self, clean_session):
        pkt_len = (12 + len(self.client_id) +  # 10 + 2 + len(client_id)
                   (2 + len(self._user) if self._user else 0) +
                   (2 + len(self._password) if self._password else 0))

        flags = (0x80 if self._user else 0x00) | (0x40 if self._password else 0x00) | (0x02 if clean_session else 0x00)  # noqa

        pkt = bytearray([mqttConst.MSG_CONNECT])  # connect
        pkt.extend(self._encode_varlen_length(pkt_len))  # len of the remaining
        # len of "MQTT" (16 bits), protocol name, and protocol version
        pkt.extend(b'\x00\x04MQTT\x04')
        pkt.append(flags)
        pkt.extend(b'\x00\x00')  # disable keepalive
        pkt.extend(self._pascal_string(self.client_id))

        if self._user:
            pkt.extend(self._pascal_string(self._user))
        if self._password:
            pkt.extend(self._pascal_string(self._password))

        return self._msgHandler.priority_send(pkt)

    def _send_unsubscribe(self, topic, dup=False):
        pkt = bytearray()
        msg_type = mqttConst.MSG_UNSUBSCRIBE | (dup << 3) | (1 << 1)
        pkt.extend(struct.pack("!B", msg_type))

        remaining_length = 2 + 2 + len(topic)
        pkt.extend(self._encode_varlen_length(remaining_length))

        self._pid += 1
        pkt.extend(self._encode_16(self._pid))
        pkt.extend(self._pascal_string(topic))

        return self._msgHandler.push_on_send_queue(pkt)

    def _send_puback(self, msg_id):
        remaining_length = 2
        pkt = struct.pack(
             '!BBH',
             mqttConst.MSG_PUBACK,
             remaining_length,
             msg_id
         )

        return self._msgHandler.push_on_send_queue(pkt)

    def _send_pubrec(self, msg_id):
        remaining_length = 2
        pkt = struct.pack(
            '!BBH',
            mqttConst.MSG_PUBREC,
            remaining_length,
            msg_id
        )

        return self._msgHandler.push_on_send_queue(pkt)

    def _parse_connack(self, payload):
        if len(payload) != 2:
            return False

        (flags, result) = struct.unpack("!BB", payload)

        if result == 0:
            self._msgHandler.setConnectionState(mqttConst.STATE_CONNECTED)
            return True
        else:
            self._msgHandler.setConnectionState(mqttConst.STATE_DISCONNECTED)
            return False

    def _parse_suback(self, payload):
        self._subscribeSent = True
        return True

    def _parse_puback(self, payload):
        return True

    def _notify_message(self, message):
        notified = False
        self._callback_mutex.acquire()
        for t_obj in self._topic_callback_queue:
            if self._topic_matches_sub(t_obj[0], message.topic):
                t_obj[1](self, message)
                notified = True
        self._callback_mutex.release()

        return notified

    def _parse_publish(self, cmd, packet):
        msg = MQTTMessage()
        msg.dup = (cmd & 0x08) >> 3
        msg.qos = (cmd & 0x06) >> 1
        msg.retain = (cmd & 0x01)

        pack_format = "!H" + str(len(packet)-2) + 's'
        (slen, packet) = struct.unpack(pack_format, packet)
        pack_format = '!' + str(slen) + 's' + str(len(packet)-slen) + 's'
        (msg.topic, packet) = struct.unpack(pack_format, packet)

        if len(msg.topic) == 0:
            return False

        if msg.qos > 0:
            pack_format = "!H" + str(len(packet)-2) + 's'
            (msg.mid, packet) = struct.unpack(pack_format, packet)

        msg.payload = packet

        if msg.qos == 0:
            self._notify_message(msg)
        elif msg.qos == 1:
            self._send_puback(msg.mid)
            self._notify_message(msg)
        elif msg.qos == 2:
            self._send_pubrec(msg.mid)
            self._notify_message(msg)
        else:
            return False

        return True

    def _parse_unsuback(self, payload):
        self._unsubscribeSent = True
        return True

    def _parse_pingresp(self):
        self._msgHandler.setPingFlag(True)
        return True

    def _recv_callback(self, cmd, payload):
        msg_type = cmd & 0xF0

        if msg_type == mqttConst.MSG_CONNACK:
            return self._parse_connack(payload)
        elif msg_type == mqttConst.MSG_SUBACK:
            return self._parse_suback(payload)
        elif msg_type == mqttConst.MSG_PUBACK:
            return self._parse_puback(payload)
        elif msg_type == mqttConst.MSG_PUBLISH:
            return self._parse_publish(cmd, payload)
        elif msg_type == mqttConst.MSG_UNSUBACK:
            return self._parse_unsuback(payload)
        elif msg_type == mqttConst.MSG_PINGRESP:
            return self._parse_pingresp()
        else:
            print_debug(2, 'Unknown message type: %d' % msg_type)
            return False
