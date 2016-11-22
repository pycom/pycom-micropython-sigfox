import machine


adc = machine.ADC()
print(adc)
apin = adc.channel(pin = 'P16')
print(apin)

#read single sample
val = apin.value()
print( val> -1)


apin = adc.channel(0)
print(apin)
apin = adc.channel(id=0)
print(apin)
