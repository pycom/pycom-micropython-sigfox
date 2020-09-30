# stress test for creating many threads

try:
    import utime as time
except ImportError:
    import time
import _thread


def thread_entry(n):
    pass


thread_num = 0
# On FIPY with esp-idf 4.x only 9 threads can be created maximum, beyond that the device crashes
while thread_num < 9:
    try:
        _thread.start_new_thread(thread_entry, (thread_num,))
        thread_num += 1
    except MemoryError:
        time.sleep(0.01)

# wait for the last threads to terminate
time.sleep(1)
print("done")
