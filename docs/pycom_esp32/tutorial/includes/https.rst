

HTTPS
-----

Basic connection using :mod:`ssl.wrap_socket <ssl.wrap_socket>`.

::

	import socket
	import ssl
	s = socket.socket()
	ss = ssl.wrap_socket(s)
	ss.connect(socket.getaddrinfo('www.google.com', 443)[0][-1])

Basic example using certificates with the blynk cloud. 

Certificate was downloaded from the `blynk examples folder <https://github.com/wipy/wipy/tree/master/examples/blynk>`_ and placed in ``/flash/cert/`` on the board.

::

	import socket
	import ssl
	s = socket.socket()
	ss = ssl.wrap_socket(s, cert_reqs=ssl.CERT_REQUIRED, ca_certs='/flash/cert/ca.pem')
	ss.connect(socket.getaddrinfo('cloud.blynk.cc', 8441)[0][-1])


For more info, check the :mod:`ssl module <ussl>` in the API reference. 