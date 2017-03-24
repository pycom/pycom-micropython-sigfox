import time, sys
import RPi.GPIO as GPIO
import sys

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

BOOTLOADER = 2
NRESET = 3
SAFEBOOT = 4

SHORT = 0.1
LONG = 0.5
VERYLONG = 1.0
LASTBOOTDELAY = 1.5
PREVBOOTDELAY = 5
FACTORYBOOTDELAY = 8

def setup():
    print "GPIO version: " + str(GPIO.VERSION)
    print "Pi revision " + str(GPIO.RPI_REVISION)

def clean():
    print "cleaning up..."
    GPIO.cleanup()
    print "done."

def reset_mcu():
    GPIO.setup(NRESET, GPIO.OUT)
    GPIO.output(NRESET, False)
    time.sleep(SHORT)
    GPIO.output(NRESET, True)
    GPIO.setup(NRESET, GPIO.IN)
    time.sleep(SHORT)

def enterbootloader():
    GPIO.setup(BOOTLOADER, GPIO.OUT)
    GPIO.output(BOOTLOADER, False)
    time.sleep(SHORT)

    reset_mcu()

    time.sleep(SHORT)
    GPIO.output(BOOTLOADER, True)
    time.sleep(LONG)

    GPIO.setup(BOOTLOADER, GPIO.IN)

def enterSafeBoot(modeDelay):
    GPIO.setup(SAFEBOOT, GPIO.OUT)
    GPIO.output(SAFEBOOT, False)
    time.sleep(SHORT)

    reset_mcu()

    time.sleep(SHORT)
    GPIO.output(SAFEBOOT, True)
    time.sleep(modeDelay)

    GPIO.setup(SAFEBOOT, GPIO.IN)

def safeBootLast():
    enterSafeBoot(LASTBOOTDELAY)

def safeBootPrev():
    enterSafeBoot(PREVBOOTDELAY)

def safeBootFactory():
    enterSafeBoot(FACTORYBOOTDELAY)

def releasePins():
    GPIO.setup(NRESET, GPIO.IN)
    GPIO.setup(SAFEBOOT, GPIO.IN)
    GPIO.setup(BOOTLOADER, GPIO.IN)

def main(argv):
    setup()
    if len(argv)>0:
        cmd = argv[0]
        if cmd == 'reset':
            reset_mcu()
        elif cmd == 'bootloader':
            enterbootloader()
        elif cmd == 'safeBoot':
            safeBootPrev()
        elif cmd == 'safeBootPrev':
            safeBootPrev()
        elif cmd == 'safeBootFactory':
            safeBootFactory()
        elif cmd == 'releasePins':
            releasePins()
        else:
            pass

if __name__ == "__main__":
    main(sys.argv[1:])
