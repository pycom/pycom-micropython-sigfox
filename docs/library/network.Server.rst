.. currentmodule:: network

class Server
============

The ``Server`` class controls the behaviour and the configuration of the FTP and telnet
services running on the WiPy. Any changes performed using this class' methods will
affect both.

Example::

    import network
    server = network.Server()
    server.deinit() # disable the server
    # enable the server again with new settings
    server.init(login=('user', 'password'), timeout=600)

Constructors
------------

.. class:: network.Server(id, ...)

   Create a server instance, see ``init`` for parameters of initialization.

Methods
-------

.. method:: server.init(\*, login=('micro', 'python'), timeout=300)

   Init (and effectively start the server). Optionally a new ``user``, ``password``
   and ``timeout`` (in seconds) can be passed.

.. method:: server.deinit()

   Stop the server

.. method:: server.timeout([timeout_in_seconds])

   Get or set the server timeout.

.. method:: server.isrunning()

   Returns ``True`` if the server is running (connected or accepting connections), ``False`` otherwise.
