
import machine
import os
from network import WLAN


mch = os.uname().machine
wifi = WLAN()

print(machine)
machine.idle()

if machine.freq() < 80000000 or machine.freq() > 240000000:
    print("CPU frequency out of range")

print(machine.unique_id() == wifi.mac()[0])

machine.main('main.py')

rand_nums = []
for i in range(0, 100):
    rand = machine.rng()
    if rand not in rand_nums:
        rand_nums.append(rand)
    else:
        print('RNG number repeated')
        break

for i in range(0, 10):
    machine.idle()

print("Active")

#print(machine.reset_cause() >= 0)
#print(machine.wake_reason() >= 0)

try:
    machine.main(123456)
except:
    print('Exception')

try:
    machine.main("other_main.py")
except:
    print('Exception')

# Test machine.RTC
from machine import RTC

rtc = RTC()
#reset memory
rtc.memory(b'')
print(rtc.memory())
rtc.memory(b'10101010')
print(rtc.memory())
