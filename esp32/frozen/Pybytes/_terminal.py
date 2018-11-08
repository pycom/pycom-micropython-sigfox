from machine import UART

class Terminal:

    def __init__(self, pybytes_protocol):
        self.__pybytes_protocol = pybytes_protocol
        self.original_terminal = UART(0, 115200)
        self.message_from_pybytes = False

    def write(self, data):
        if self.message_from_pybytes:
            self.__pybytes_protocol.__send_terminal_message(data)
        else:
            self.original_terminal.write(data)

    def read(self, size):
        return self.original_terminal.read(size)

    def message_sent_from_pybytes_start(self):
        self.message_from_pybytes = True

    def message_sent_from_pybytes_end(self):
        self.message_from_pybytes = False
