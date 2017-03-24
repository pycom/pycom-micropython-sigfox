import hashlib

# test trivial operations
h = hashlib.sha256()
h.update(b'pycom')
print(h.digest())

print(hashlib.sha256(b'pycom').digest())

h = hashlib.sha256(b'pycom')
h.update(b'pycom')
h.update(b'pycom')
print(h.digest())

# test other functions
print(hashlib.md5(b'pycom').digest())
print(hashlib.sha1(b'pycom').digest())
print(hashlib.sha224(b'pycom').digest())
print(hashlib.sha384(b'pycom').digest())
print(hashlib.sha512(b'pycom').digest())

# test buffer operation
buf = b''
for i in range(127):
    buf += bytes(str(i % 10), "UTF8")

h = hashlib.sha512(b'pycom')
h.update(buf)
h.update(buf)
h.update(buf)
print(h.digest())

# test string bigger than buffer
buf = b''
for i in range(255):
    buf += bytes(str(i % 10), "UTF8")

h = hashlib.sha512(b'pycom')
h.update(buf)
h.update(buf)
print(h.digest())