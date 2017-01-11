
Threading
---------

MicroPython supports spawning threads by the ``_thread`` module. The following example
demonstrates the use of this module. A thread is simply defined as a function that can
receive any number of parameters. Below 3 threads are started, each one perform a print
at a different interval.

::

    import _thread
    import time

    def th_func(delay, id):
        while True:
            time.sleep(delay)
            print('Running thread %d' % id)

    for i in range(3):
        _thread.start_new_thread(th_func, (i + 1, i))


**Using locks:**

::

    import _thread

    a_lock = _thread.allocate_lock()

    with a_lock:
        print("a_lock is locked while this executes")

