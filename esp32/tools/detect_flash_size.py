from esptool import ESPLoader
from contextlib import contextmanager
import argparse
import os, sys

__version__ = "0.1"


DETECTED_FLASH_SIZES = {0x12: '256KB', 0x13: '512KB', 0x14: '1MB',
                        0x15: '2MB', 0x16: '4MB', 0x17: '8MB', 0x18: '16MB'}


ESP_ROM_BAUD    = 115200

@contextmanager
def suppress_stdout():
    with open(os.devnull, "w") as devnull:
        old_stdout = sys.stdout
        sys.stdout = devnull
        try:  
            yield
        finally:
            sys.stdout = old_stdout

def arg_auto_int(x):
    return int(x, 0)

parser = argparse.ArgumentParser(description='detect_flash_size.py v%s - ESP32 flash size detection tool' % __version__, prog='detect_flash_size')

parser.add_argument('--port', '-p',
                    help='Serial port conected to esp32 target',
                    default=os.environ.get('ESPTOOL_CHIP', 'auto'))

parser.add_argument('--baud', '-b',
                    help='Serial port baud rate used when flashing/reading',
                    type=arg_auto_int,
                    default=os.environ.get('ESPTOOL_BAUD', ESP_ROM_BAUD))

parser.add_argument('--trace', '-t',
                    help="Enable trace-level output of esptool.py interactions.",
                    action='store_true')

parser.add_argument('--connect-mode', '-c',
                    help="Connect Mode.",
                    choices=['default_reset', 'no_reset', 'no_reset_no_sync'],
                    default=os.environ.get('ESPTOOL_BEFORE', 'default_reset'))

args = parser.parse_args(None)

failed = False
with suppress_stdout():
    try:
        esp = ESPLoader(port = args.port, baud = args.baud, trace_enabled=args.trace)
        chip = esp.detect_chip(port = args.port, baud = args.baud, connect_mode=args.connect_mode, trace_enabled=args.trace)
        chip = chip.run_stub()
        flash_id = chip.flash_id()
        size_id = flash_id >> 16
        flash_size = DETECTED_FLASH_SIZES.get(size_id)
    except:
        failed = True

if failed:
    print("Cannot Detect Flash Size!")
else:
    print('%s' % flash_size)



