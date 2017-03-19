# _boot.py -- always run on boot-up, even during safe boot
import os
from machine import UART
UART(1, 115200)
UART(2, 115200)
os.dupterm(UART(0, 115200))
