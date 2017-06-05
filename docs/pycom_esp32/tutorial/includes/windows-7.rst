.. _windows_7_driver:

Windows 7 Driver
================

Pytrack and Pysense will work out of the box for Windows 8/10/+, Mac OS as well as Linux. If you are using Windows 7, you will need to install the following drivers to start using the board.

Please follow the instructions below to install the required drivers.

Download
--------

Please download the driver software from the link below.

:download:`Download Driver<downloads/pycom.inf>`

Installation
------------

First navigate open the Windows start menu and search/navigate to ``Device Manager``. You should see your Pytrack/Pysense in the dropdown under **other devices**.

.. image:: images/win7-1.png
    :alt: Win7 1
    :align: center
    :scale: 60 %

Right click the device and select ``Update Driver Software``.

.. image:: images/win7-2.png
    :alt: Win7 2
    :align: center
    :scale: 60 %

Select the option to **Browse my computer for driver software**.

.. image:: images/win7-3.png
    :alt: Win7 3
    :align: center
    :scale: 60 %

Next you will need to navigate to where you downloaded the driver to (e.g. **Downloads** Folder).

.. image:: images/win7-4.png
    :alt: Win7 4
    :align: center
    :scale: 60 %

Specify the folder in which the drivers are contained. If you haven't extracted the .zip file, please do this before selecting the folder.

.. image:: images/win7-5.png
    :alt: Win7 5
    :align: center
    :scale: 60 %

You may receive a warning, suggesting that windows can't verify the publisher of this driver. Click ``Install this driver software anyway`` as this link points to our official driver.

.. image:: images/win7-6.png
    :alt: Win7 6
    :align: center
    :scale: 60 %

If the installation was successful, you should now see a window specifying that the driver was correctly installed.

.. image:: images/win7-7.png
    :alt: Win7 7
    :align: center
    :scale: 60 %

To confirm that the installation was correct, navigate back to the ``Device Manager`` and click the dropdown for other devices. The warning label should now be gone and Pytrack/Pysense should be installed.

.. image:: images/win7-8.png
    :alt: Win7 8
    :align: center
    :scale: 60 %
