import time
import machine

DAYS_PER_MONTH = [0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]

def is_leap(year):
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)

def test():
    seconds = 0
    wday = 3    # Jan 1, 1970 was a Thursday
    for year in range(1970, 2038):
        print("Testing %d" % year)
        yday = 1
        for month in range(1, 13):
            if month == 2 and is_leap(year):
                DAYS_PER_MONTH[2] = 29
            else:
                DAYS_PER_MONTH[2] = 28
            for day in range(1, DAYS_PER_MONTH[month] + 1):
                secs = time.mktime((year, month, day, 0, 0, 0, 0, 0))
                if secs != seconds:
                    print("mktime failed for %d-%02d-%02d got %d expected %d" % (year, month, day, secs, seconds))
                tuple = time.localtime(seconds)
                secs = time.mktime(tuple)
                if secs != seconds:
                    print("localtime failed for %d-%02d-%02d got %d expected %d" % (year, month, day, secs, seconds))
                    return
                seconds += 86400
                if yday != tuple[7]:
                    print("locatime for %d-%02d-%02d got yday %d, expecting %d" % (year, month, day, tuple[7], yday))
                    return
                if wday != tuple[6]:
                    print("locatime for %d-%02d-%02d got wday %d, expecting %d" % (year, month, day, tuple[6], wday))
                    return
                yday += 1
                wday = (wday + 1) % 7

def spot_test(seconds, expected_time):
    actual_time = time.localtime(seconds)
    for i in range(len(actual_time)):
        if actual_time[i] != expected_time[i]:
            print("time.localtime(", seconds, ") returned", actual_time, "expecting", expected_time)
            return
    print("time.localtime(", seconds, ") returned", actual_time, "(pass)")

test()

spot_test(          0,  (1970,  1,  1,  0,  0,  0, 3,   1))
spot_test(          1,  (1970,  1,  1,  0,  0,  1, 3,   1))
spot_test(         59,  (1970,  1,  1,  0,  0, 59, 3,   1))
spot_test(         60,  (1970,  1,  1,  0,  1,  0, 3,   1))
spot_test(       3599,  (1970,  1,  1,  0, 59, 59, 3,   1))
spot_test(       3600,  (1970,  1,  1,  1,  0,  0, 3,   1))
spot_test(         -1,  (1969, 12, 31, 23, 59, 59, 2, 365))
spot_test(  447549467,  (1984,  3,  7, 23, 17, 47, 2,  67))
spot_test( -940984933,  (1940,  3,  7, 23, 17, 47, 3,  67))
spot_test( 1394234267,  (2014,  3,  7, 23, 17, 47, 4,  66))

# times before 1970 (specially before 1940) need all sorts of adjustments to comply with reality back them. Trivial calculation performed by micropython is not gonna cut it
# spot_test(-1072915199,  (1936,  1,  1, 23, 19, 33, 2,   1))
# spot_test(-1072915200,  (1936,  1,  1, 23, 19, 32, 2,   1))
# spot_test(-1072915201,  (1936,  1,  1, 23, 19, 31, 2,   1))

# the next test is performed this way to overcome what it looks some sort of timeout in run-tests. If it receives something every 5 seconds, it seems to be happy
t1 = time.time()
for i in range(0, 12):
    time.sleep(5)
    print(i)
t2 = time.time()

print(abs(t2 - t1 - 60) <= 1)

t1 = time.ticks_ms()
time.sleep_ms(100)
t2 = time.ticks_ms()
print(abs(t2 - t1 - 100) <= 10)

irqs = machine.disable_irq()
t1 = time.ticks_us()
time.sleep_us(900)
t2 = time.ticks_us()
irqs = machine.enable_irq(irqs)
print((abs(t2 - t1 - 900)) < 100)
