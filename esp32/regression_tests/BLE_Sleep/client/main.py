from network import Bluetooth
import time
import machine

BLE_SERVER_NAME = 'My_BLE_Server'

print("Starting...")

bt = Bluetooth()
bt.start_scan(-1)

while True:
    adv = bt.get_adv()
    if adv and bt.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL) == BLE_SERVER_NAME:
        conn = bt.connect(adv.mac)
        break

time.sleep(1)
bt.start_scan(-1)

print("Connection status with {}: {}".format(BLE_SERVER_NAME, conn.isconnected()))

print("Going to sleep for 1 second...")
machine.sleep(1000, True)

print("Connection status with {}: {}".format(BLE_SERVER_NAME, conn.isconnected()))

# To avoid crash in esp-idf 4.1 BLE stack, 1 sec waiting time is needed.
# Fix for the problem: https://github.com/espressif/esp-idf/commit/714d88e
time.sleep(1)

conn.disconnect()
bt.deinit()

# Indicate to the Server that we have finished
prtf_send_command(PRTF_COMMAND_GO)
