### LoRa/Sigfox FCC/CE/RCM Certification Scripts

This directory contains script(s) that should be executed when the board boots, preparing it for the lab tests.

When beginning a test,  after the board has been powered up (either for Sigfox or LoRa), the first thing is to call one of these functions:

```
start_test_fcc()

start_test_rcm()

start_test_ce()
```

#### To begin test:

For Sigfox only these functions are used:

```
do_freq_hopping_fcc()

do_freq_hopping_rcm()

do_freq_hopping_ce()

do_continuos_transmit(<frequency>)
```

For LoRa these are the functions to use:

```
select_new_frequency():

sock_send_repeat(packt_len,num_packts,
delay_secs, 
fhss=False)

sock_send_all_channels(packt_len,delay_secs=0.1)
```

_Both LoRa and Sigfox can also use the normal socket functions to send single messages during testing._