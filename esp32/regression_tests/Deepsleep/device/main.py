import machine
import pycom

print("Starting...")

try:
    deepsleep = pycom.nvs_get("deepsleep")
except:
    deepsleep = 0
    pycom.nvs_set("deepsleep", 0)

# The machine.reset_cause() cannot be used to identify whether deepsleep happened because when the test framework enters
# into RAW REPL mode it issues Soft Reset which overwrites the returned value of machine.reset_cause()
if(deepsleep == 1) :
    print("Awake from deepsleep")
    pycom.nvs_erase("deepsleep")
else:
    print("Going to deepsleep for 5 seconds...")
    pycom.nvs_set("deepsleep", 1)
    prtf_send_command(PRTF_COMMAND_RESTART)
    machine.deepsleep(5000)
    # Just putting this here to make sure this line is never executed and once machine.deepsleep() returns the device restarts
    pycom.nvs_set("deepsleep", 0)

