# test single hash operation lock

import hashlib

try:
    h1 = hashlib.sha256()
    h2 = hashlib.sha256()
except OSError as e:
    print(e)
finally:
    h1.digest()
