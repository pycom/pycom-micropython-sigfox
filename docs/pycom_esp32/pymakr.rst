

.. _Pymakr:

Pymakr
======

Here are some basic tips on how to further use Pymakr to upload code to your modules. The easiest and most used way to work is by creating projects:

Creating a project
------------------

Pymakr has a feature to sync and run your code on your device. This is mostly done using projects. The following steps will get you started.

#. In Pymakr, go to Project > New project.
#. Give it a name and select a folder for your project, either a new of existing one.
#. Now you are ready to place your own code. For fun, lets try again to build a traffic light. Add the following code to the main.py file:

::

    import pycom
    import time
    pycom.heartbeat(False)
    for cycles in range(10): # stop after 10 cycles 
        pycom.rgbled(0x007f00) # green
        time.sleep(5)
        pycom.rgbled(0x7f7f00) # yellow
        time.sleep(1.5)
        pycom.rgbled(0x7f0000) # red
        time.sleep(4)

#. Make sure the connection to your board is open in the Pycom Console
#. Press the sync button on the top toolbar. Any progress will be shown in the console.

Here is the expected result:

.. image:: images/traffic.gif
    :alt: Traffic light
    :align: center
    :scale: 60 %


You now have a traffic light in your hands! To stop it, just do a right click
on the console and press ``Reset`` or use ctrl-c.


.. Warning::

    While the module is busy executing code, Pymakr cannot control it. You can regain control of it by using ctrl-c (or right clicking in the console and pressing Reset) or physically press the reset button.
    If your board is running code at boot time, you might need to boot it in :ref:`safe mode <safeboot>`.

.. #todo: add link to safeboot


Without creating a project
--------------------------

If you just want to test some code on the module, you can create a new file or open an existing one and press the 'run' button.


.. Warning::
    
    The changes you make to your file won't be automatically saved to the device on execution.





.. #todo: add link to safeboot
