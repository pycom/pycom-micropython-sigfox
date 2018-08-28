# _boot.py -- always run on boot-up, even during safe boot
import os
from machine import UART
os.dupterm(UART(0, 115200))
