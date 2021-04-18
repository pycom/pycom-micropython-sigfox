from network import WLAN
import machine
import time

# Configuration of this test
WLAN_NETWORK_SSID = "PYCOM_REGR_TEST_AP"
WLAN_NETWORK_PWD  = "regression_test"

# Connect to the Network
wlan = WLAN(mode=WLAN.STA)

# Wait for the Access Point to start
prtf_wait_for_command(PRTF_COMMAND_START)

nets = wlan.scan()
for net in nets:
    if net.ssid == WLAN_NETWORK_SSID:
        print('Network found!')
        wlan.connect(net.ssid, auth=(net.sec, WLAN_NETWORK_PWD), timeout=5000)
        while not wlan.isconnected():
            machine.idle() # save power while waiting
        print('WLAN connection succeeded!')
        break

print("WLAN connection status: {}".format(wlan.isconnected()))

print("Going to sleep for 1 second...")
machine.sleep(1000, True)
# Wait 5 seconds so the WLAN connection can be re-established
time.sleep(5)
print("WLAN connection status: {}".format(wlan.isconnected()))

# Indicate to the Access Point that the test has been finished
prtf_send_command(PRTF_COMMAND_STOP)

# Deinitialize the WLAN module
wlan.deinit()