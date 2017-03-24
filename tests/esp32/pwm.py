from machine import PWM

pwm_timers=range(3)
pwm_channels=range(7)


for timer in pwm_timers:
    for channel in pwm_channels:
        pwm = PWM(timer,frequency=5000)
        print (pwm)
        pwm_c = pwm.channel(channel, pin='P12', duty_cycle=0.5)
        print(pwm_c)
        pwm_c.duty_cycle(0.3)
        print(pwm_c)

pwm = PWM(0,frequency=5000)
pwm_c = pwm.channel(channel, pin='P12', duty_cycle=-1.0)
pwm_c.duty_cycle(0.0)
pwm_c.duty_cycle(10.0)
pwm_c.duty_cycle(-10.0)
pwm_c.duty_cycle(0)

try:
    pwm = PWM(4,frequency=5000)
except Exception:
    print("Exception")

try:
    pwm = PWM(4,frequency=5000)
    pwm_c = pwm.channel(8, pin='P12', duty_cycle=0.0)
except Exception:
    print("Exception")
