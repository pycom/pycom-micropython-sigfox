Registering the SiPy with Sigfox
--------------------------------

To ensure your device has been provisioned with **Device ID** and **PAC number**, please update to the latest :ref:`firmware <firmware_upgrades>`.

In order to send a Sigfox message, you will need to register your SiPy with the Sigfox Backend. Navigate to https://backend.sigfox.com/activate and you will find a list of Sigfox enabled development kits. Select **Pycom** to proceed.

Next you will need to choose a Sigfox Operator for the country in which you will be activating your SiPy. Find your country and select the operator to continue.

Now you will need to enter the SiPy's **Device ID** and **PAC number**. These are retrievable through a couple of commands via the REPL.

::

    from network import Sigfox
    import binascii

    # initalise Sigfox for RCZ1 (You may need a different RCZ Region)
    sigfox = Sigfox(mode=Sigfox.SIGFOX, rcz=Sigfox.RCZ1)

    # print Sigfox Device ID
    print(binascii.hexlify(sigfox.id()))

    # print Sigfox PAC number
    print(binascii.hexlify(sigfox.pac()))

See :class:`Sigfox <.Sigfox>` for more info about the Sigfox Class and which RCZ region to use.

Once you have retrieved and entered your SiPy's Device ID and PAC number, you will need to create an account for yourself. Provide the required information including email address and click to continue.

You should now receive an email confirming the creation of your Sigfox Backend account and the successful registration of your SiPy.
