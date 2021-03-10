# Pycom firmware updater
Script for updating firmaware on Pycom modules

## Download firmware
Links for Pygate firmware for the available modules (WiPy, GPy, and LoPy4) can be found [here](https://docs.pycom.io/advance/downgrade/).

# Quickstart
Install and activate a `virtualenv`
```
python3.7 -m venv venv
source venv/bin/activate
```

Install dependencies
```
pip install -r requirements
deactivate
```

As root user, activate `venv` and run updater
```
sudo su
source venv/bin/activate
./updater.py -v -p /dev/ttyACM1 flash -t ~/path/to/firmware/LoPy4-1.20.2.rc11.tar.gz
```

Set WiFi Access point SSID and password
```
./updater.py -v -p /dev/ttyACM1 wifi --ssid <ssid name> --pwd <password>
```

Set LoRa Region
```
./updater.py -v -p /dev/ttyACM1 lpwan --region <'EU868', 'US915', 'AS923', 'AU915', or 'IN865'>
```

Set filesystem type
```
./updater.py -v -p /dev/ttyACM1 pycom --fs_type <'FatFS' or 'LittleFS'>
```
