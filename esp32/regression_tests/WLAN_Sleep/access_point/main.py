from network import WLAN

# Configuration of this test
WLAN_NETWORK_SSID = "PYCOM_REGR_TEST_AP"
WLAN_NETWORK_PWD  = "regression_test"

# Set up this device as an Access Point
wlan = WLAN(mode=WLAN.AP, ssid = WLAN_NETWORK_SSID, auth = (WLAN.WPA2, WLAN_NETWORK_PWD))
print("Access Point is ready.")

# Tell the Client device that AP is up and running
prtf_send_command(PRTF_COMMAND_GO)

# Wait for the Client device to finish its test
prtf_wait_for_command(PRTF_COMMAND_GO)

# Deinitialize the WLAN module
wlan.deinit()