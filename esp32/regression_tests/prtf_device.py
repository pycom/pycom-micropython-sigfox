from machine import UART
import time

PRTF_COMMAND_START                 = 1
PRTF_COMMAND_GO                    = 2
PRTF_COMMAND_STOP                  = 3
PRTF_COMMAND_RESTART               = 4


uart = UART(0, 115200)

def prtf_command_to_bytes(command):
    if(command == PRTF_COMMAND_START):
        return b"PRTC:START\n"
    if(command == PRTF_COMMAND_GO):
        return b"PRTC:GO\n"
    if(command == PRTF_COMMAND_STOP):
        return b"PRTC:STOP\n"
    if(command == PRTF_COMMAND_RESTART):
        return b"PRTC:RESTART\n"
    return None

def prtf_send_command(command):
    command_to_send = prtf_command_to_bytes(command)
    if(command_to_send is not None):
        uart.write(command_to_send)
    else:
        # TODO: Drop exception
        pass

def prtf_wait_for_command(command):
    command_wait = prtf_command_to_bytes(command)
    data = uart.readline()
    while(data != command_wait):
        data = uart.readline()
        time.sleep(0.1)




