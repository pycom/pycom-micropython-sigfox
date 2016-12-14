
Reset and boot modes
--------------------

There are soft resets and hard resets.

A soft reset simply clears the state of the MicroPython virtual machine,
but leaves hardware peripherals unaffected. To do a soft reset, simply press
**Ctrl+D** on the REPL, or within a script do::

   >>> import sys
   >>> sys.exit()

A hard reset is the same as performing a power cycle to the board. In order to
hard reset the LoPy, press the switch on the board or::

   >>> import machine
   >>> machine.reset()

Safe boot
^^^^^^^^^

If something goes wrong with your LoPy, don't panic!  It is almost
impossible for you to break the LoPy by programming the wrong thing.

The first thing to try is to boot in safe mode: this temporarily skips
execution of ``boot.py`` and ``main.py`` and sets the default WLAN configuration.

If you have problems with the filesystem you can :ref:`format the internal flash
drive <pycom_factory_reset>`.

To boot in safe mode, follow the detailed instructions described :ref:`here <pycom_boot_modes>`.

In safe mode, the ``boot.py`` and ``main.py`` files are not executed, and so
the LoPy boots up with default settings.  This means you now have access
to the filesystem, and you can edit ``boot.py`` and ``main.py`` to fix any problems.

Entering safe mode is temporary, and does not make any changes to the
files on the LoPy.

.. _pycom_factory_reset:

Factory reset the filesystem
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you LoPy's filesystem gets corrupted (very unlikely, but possible), you
can format it very easily by doing::

   >>> import os
   >>> os.mkfs('/flash')

Resetting the `flash` filesystem deletes all files inside the internal LoPy
storage (not the SD card), and restores the files ``boot.py`` and ``main.py``
back to their original states after the next reset.
