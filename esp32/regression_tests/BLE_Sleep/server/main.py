from network import Bluetooth

BLE_SERVER_NAME = 'My_BLE_Server'
BLE_SERVER_CHAR1_UUID = b'ab34567890123456'
BLE_SERVER_CHAR2_UUID = 4567

def conn_cb (bt_o):
    events = bt_o.events()
    if  events & Bluetooth.CLIENT_CONNECTED:
        print("Client connected")
    elif events & Bluetooth.CLIENT_DISCONNECTED:
        print("Client disconnected")

def chr_cb_handler(m, n):
    (chr, uuid) = m
    events = chr.events()
    if  events & Bluetooth.CHAR_WRITE_EVENT:
        print("On characteristic with UUID {} a new write request with value = {} arrived.".format(uuid, chr.value()))
    else:
        print('On characteristic with UUID {} a new read request arrived.'.format(uuid))

print("Starting...")

bluetooth = Bluetooth()
bluetooth.set_advertisement(name=BLE_SERVER_NAME, service_uuid=BLE_SERVER_CHAR1_UUID)
bluetooth.callback(trigger=Bluetooth.CLIENT_CONNECTED | Bluetooth.CLIENT_DISCONNECTED, handler=conn_cb)

srv1 = bluetooth.service(uuid=BLE_SERVER_CHAR1_UUID, isprimary=True)
chr1 = srv1.characteristic(uuid=BLE_SERVER_CHAR1_UUID, value=5)
chr1.callback(trigger=Bluetooth.CHAR_READ_EVENT | Bluetooth.CHAR_WRITE_EVENT, handler=chr_cb_handler, arg=(chr1, BLE_SERVER_CHAR1_UUID))

srv2 = bluetooth.service(uuid=BLE_SERVER_CHAR2_UUID)
chr2 = srv2.characteristic(uuid=BLE_SERVER_CHAR2_UUID, value=0x1234)
chr2.callback(trigger=Bluetooth.CHAR_READ_EVENT | Bluetooth.CHAR_WRITE_EVENT, handler=chr_cb_handler, arg=(chr2, BLE_SERVER_CHAR2_UUID))
srv2.start()

bluetooth.advertise(True)
print("Advertisement has been started.")

# Wait for the Client to finish before we exit
prtf_wait_for_command(PRTF_COMMAND_STOP)

bluetooth.deinit()
