from machine import DAC


dac_pins = ['P22','P21']

for pin in dac_pins:
    dac=DAC(pin)
    print (dac)
    dac.write(0.0)
    dac.write(1.0)
    dac.write(2)
    dac.tone(125,0)
    dac.tone(20000,0)
    dac.tone(125,1)
    dac.deinit()
    print (dac)


try:
    dac=DAC()
except Exception:
    print("Exception")

dac=DAC('P22')
try:
    dac.write()
except Exception:
    print("Exception")

try:
    dac.tone()
except Exception:
    print("Exception")

try:
    dac.tone(1)
except Exception:
    print("Exception")

try:
    dac.tone(0,0)
except Exception:
    print("Exception")

try:
    dac.tone(21000,0)
except Exception:
    print("Exception")
