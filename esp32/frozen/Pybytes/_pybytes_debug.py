'''
Copyright (c) 2021, Pycom Limited.
This software is licensed under the GNU GPL version 3 or any
later version, with permitted additional terms. For more information
see the Pycom Licence v1.0 document supplied with this file, or
available at https://www.pycom.io/opensource/licensing
'''

import os
import pycom  # pylint: disable=import-error

from machine import RTC
from time import timezone

# For compatibility with new 1.18 firmware release
try:
    DEBUG = pycom.nvs_get('pybytes_debug')
except:
    DEBUG = None


def print_debug(level, msg):
    """Print log messages into console."""
    if DEBUG is not None and level <= DEBUG:
        print(msg)


def print_debug_local(level, msg):
    """
    Print log messages.

    log messages will be stored in the device so
    the user can access that using FTP or Flash OTA.
    """
    if DEBUG is not None and level <= DEBUG:
        print_debug(0, 'adding local log')
        rtc = RTC()
        if not rtc.synced():
            rtc.ntp_sync("pool.ntp.org")
        while not rtc.synced():
            pass
        current_year, current_month, current_day, current_hour, current_minute, current_second, current_microsecond, current_tzinfo = rtc.now() # noqa
        msg = '\n {}-{}-{} {}:{}:{} (GMT+{}) >>>  {}'.format(
            current_day,
            current_month,
            current_year,
            current_hour,
            current_minute,
            current_second,
            timezone(),
            msg
        )
        try:
            fsize = os.stat('logs.log')
            if fsize.st_size > 1000000:
                # logs are bigger than 1 MB
                os.remove("logs.log")
        except Exception:
            pass

        log_file = open('logs.log', 'a+')
        log_file.write(msg)
        log_file.close()
