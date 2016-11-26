****************************************
:mod:`ucrypto` --- Cryptography
****************************************

.. module:: ucrypto

   :synopsis: Cryptographic algorithms and functions.

This module provides native support for cryptographic algorithms.
It's loosely based on `PyCrypto <http://pythonhosted.org/pycrypto/>`_ .

Right now it supports:

.. toctree::
    :maxdepth: 1

    ucrypto.AES

.. warning::
    Cryptography is not a trivial business. Doing things the wrong way could
    quickly result in decreased or no security. Please document yourself in the
    subject if you are depending on encryption to secure important information.
