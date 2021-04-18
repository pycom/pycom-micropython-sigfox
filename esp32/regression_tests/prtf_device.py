from machine import UART

PRTF_COMMAND_STARTED      = 1
PRTF_COMMAND_WAITING      = 2
PRTF_COMMAND_GO           = 3
PRTF_COMMAND_STOPPED      = 4

uart = UART(0, 115200)

def prtf_command_to_bytes(command):
    if(command == PRTF_COMMAND_STARTED):
        return b"PRTC:STARTED\n"
    if(command == PRTF_COMMAND_WAITING):
        return b"PRTC:WAITING\n"
    if(command == PRTF_COMMAND_GO):
        return b"PRTC:GO\n"
    if(command == PRTF_COMMAND_STOPPED):
        return b"PRTC:STOPPED\n"
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




