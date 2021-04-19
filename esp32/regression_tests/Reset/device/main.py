import machine
import pycom

print("Starting...")

try:
    deepsleep = pycom.nvs_get("reset")
except:
    deepsleep = 0
    pycom.nvs_set("reset", 0)

# The machine.reset_cause() cannot be used to identify whether reset happened because when the test framework enters
# into RAW REPL mode it issues Soft Reset which overwrites the returned value of machine.reset_cause()
if(deepsleep == 1) :
    print("Alive after reset")
    pycom.nvs_erase("reset")
else:
    print("Resetting the device...")
    pycom.nvs_set("reset", 1)
    prtf_send_command(PRTF_COMMAND_RESTART)
    machine.reset()
    # Just putting this here to make sure this line is never executed and once machine.reset() returns the device restarts
    pycom.nvs_set("reset", 0)

