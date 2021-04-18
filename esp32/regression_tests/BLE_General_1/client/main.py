from network import Bluetooth
import time

BLE_SERVER_NAME = 'My_BLE_Server'
BLE_SERVER_CHAR1_UUID = b'ab34567890123456'
BLE_SERVER_CHAR2_UUID = 4567

# Should not be called in this Test
def characteristics_callback(obj, i):
    print("On characteristic with UUID {} a new event arrived.".format(obj.uuid()))

print("Starting...")

bt = Bluetooth()
bt.start_scan(-1)

while True:
    adv = bt.get_adv()
    if adv and bt.resolve_adv_data(adv.data, Bluetooth.ADV_NAME_CMPL) == BLE_SERVER_NAME:
        try:
            conn = bt.connect(adv.mac)
            print("Connected to {}".format(BLE_SERVER_NAME))
            services = conn.services()
            print("Listing services of {}:".format(BLE_SERVER_NAME))
            for service in services:
                time.sleep(0.050)
                if type(service.uuid()) == bytes:
                    print('Reading Characteristics of Service: {}'.format(service.uuid()))
                else:
                    print('Reading Characteristics of Service: %x' % service.uuid())
                chars = service.characteristics()
                for char in chars:
                    print("Characteristic's UUID: {}".format(char.uuid()))
                    if (char.properties() & Bluetooth.PROP_READ):
                        print('Characteristic {}\'s value: {}'.format(char.uuid(), char.read()))
                    
                    # Save the characteristic objects of the 2 characteristics we will write later 
                    if(char.uuid() == BLE_SERVER_CHAR1_UUID):
                        my_char_1 = char
                        my_char_1.callback(trigger=Bluetooth.CHAR_NOTIFY_EVENT, handler=characteristics_callback)
                    if(char.uuid() == BLE_SERVER_CHAR2_UUID):
                        my_char_2 = char
                        my_char_2.callback(trigger=Bluetooth.CHAR_NOTIFY_EVENT, handler=characteristics_callback)
            break
        except:
            pass
    else:
        time.sleep(0.050)

print("Writing values to {} and {} characteristics".format(BLE_SERVER_CHAR1_UUID, BLE_SERVER_CHAR2_UUID))
for i in range (20):
    print("Writing value {} to {}".format(i, BLE_SERVER_CHAR1_UUID))
    my_char_1.write(str(i))
    # Wait so the order of messages printed by Server and Client will be preserved
    time.sleep(0.01)
    print("Writing value {} to {}".format(i*2, BLE_SERVER_CHAR2_UUID))
    my_char_2.write(str(i*2))
    print("Value of {} after read back: {}".format(BLE_SERVER_CHAR1_UUID, my_char_1.read()))
    print("Value of {} after read back: {}".format(BLE_SERVER_CHAR2_UUID, my_char_2.read()))

conn.disconnect()
bt.deinit()

# Indicate to the Server that we have finished
prtf_send_command(PRTF_COMMAND_GO)