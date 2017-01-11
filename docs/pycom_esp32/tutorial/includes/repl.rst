.. _repl_tutorial:

Using the REPL prompt
---------------------

Now let's try running some MicroPython code directly on the LoPy.

With Pymakr open and your board connected or your preferred serial program (PuTTY, screen, picocom, etc) you may see a blank screen with a flashing cursor.  Press Enter and you should be presented with a MicroPython prompt, i.e. ``>>>``.  Let's make sure it is working with the obligatory test::

    >>> print("Hello LoPy!")
    Hello LoPy!

In the above, you should not type in the ``>>>`` characters.  They are there to
indicate that you should type the text after it at the prompt.  In the end, once
you have entered the text ``print("Hello LoPy!")`` and pressed Enter, the output
on your screen should look like it does above.

If you already know some Python you can now try some basic commands here.

If any of this is not working you can try either a hard reset or a soft reset;
see below.

Go ahead and try typing in some other commands.  For example::

    >>> from machine import Pin
    >>> led = Pin('G16', mode=Pin.OUT, value=1)
    >>> led(0)
    >>> led(1)
    >>> led.toggle()
    >>> 1 + 2
    3
    >>> 5 / 2
    2.5
    >>> 20 * 'py'
    'pypypypypypypypypypypypypypypypypypypypy'

Resetting the board
^^^^^^^^^^^^^^^^^^^

If something goes wrong, you can reset the board in two ways. The first is to press CTRL-D
at the MicroPython prompt, which performs a soft reset.  You will see a message something like::

    >>>
    PYB: soft reboot
    MicroPython v1.4.6-146-g1d8b5e5 on 2016-10-21; LoPy with ESP32
    Type "help()" for more information.
    >>>

If that isn't working you can perform a hard reset (turn-it-off-and-on-again) by pressing the
RST switch (the small black button next to the RGB LED). During telnet, this will end
your session, disconnecting whatever program that you used to connect to the LoPy.
