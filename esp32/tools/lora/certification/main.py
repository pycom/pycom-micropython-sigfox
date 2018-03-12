from network import LoRa
from certification import Compliance

regions = [LoRa.EU868, LoRa.AS923, LoRa.US915, LoRa.AU915]
activations = [LoRa.OTAA, LoRa.ABP]

regions_str = ['EU868', 'AS923', 'US915', 'AU915']
activations_str = ['OTAA', 'ABP']

region = None
activation = None

print('<<< Device ready for LoRaWAN certification testing >>>')
print('Press ENTER to continue')
print('')
input()

while True:
    print('Please select the region:')
    print('     (1) EU868')
    print('     (2) AS923')
    print('     (3) US915')
    print('     (4) AU915')

    try:
        region_in = int(input()) - 1
    except Exception:
        region_in = 0

    if region_in >= 0:
        try:
            region = regions[region_in]
            break
        except Exception:
            pass

while True:
    print('Please select an activation method:')
    print('     (1) OTAA')
    print('     (2) ABP')

    try:
        activation_in = int(input()) - 1
    except Exception:
        activation_in = 0

    if activation_in >= 0:
        try:
            activation = activations[activation_in]
            break
        except Exception:
            pass

print('<<< Certification test started for region {} using {} >>>'.format(regions_str[region_in], activations_str[activation_in]))
print('')

compliance = Compliance(region=region, activation=activation)
compliance.run()
