key = b'pycomisthekey...'
IV = b'thisisthetestIV '


test_count = 0
def test_print(x):
    global test_count
    test_count = test_count + 1
    print("at test %d " % test_count, x)

# is this script running in micropython or cpython
try:
    import machine
    interpreter = 'upython'
except:
    interpreter = 'cpython'


# general setup

if interpreter == 'upython':
    from crypto import AES
    aes = AES
else:
    from Crypto.Cipher import AES
    from Crypto.Util import Counter
    import struct
    aes = AES.new


# ECB mode fixture
def setup_ecb():
    return aes(key, AES.MODE_ECB)

# ECB mode tests
cipher = setup_ecb()
result = cipher.encrypt(b'testing ecb 123.')
test_print(result)

cipher = setup_ecb()
result = cipher.decrypt(result)
test_print(result)

cipher = setup_ecb()
text = "this is a test.."
result1 = cipher.encrypt(text)
result2 = cipher.encrypt(text)
test_print(result1 == result2)
cipher = setup_ecb()
result = cipher.encrypt(text + text)
test_print(result == result1 + result2)
cipher = setup_ecb()
result = cipher.decrypt(result1 + result2)
test_print(result)
test_print(result == text + text)

# CFB mode fixture
def setup_cfb(s):
    return aes(key, AES.MODE_CFB, IV, segment_size=s)

# CFB mode tests
cipher = setup_cfb(8)
result = cipher.encrypt("this is a test..")
test_print(result)
cipher = setup_cfb(8)
result = cipher.decrypt(result)
test_print(result)

cipher = setup_cfb(128)
result = cipher.encrypt("this is a test..")
test_print(result)
cipher = setup_cfb(128)
result = cipher.decrypt(result)
test_print(result)

cipher = setup_cfb(8)
text1 = "this is a test.."
text2 = "and another one."
result1 = cipher.encrypt(text1)
result2 = cipher.encrypt(text2)
cipher = setup_cfb(8)
result = cipher.encrypt(text1 + text2)
test_print(result == result1 + result2)
cipher = setup_cfb(8)
result = cipher.decrypt(result1 + result2)
test_print(result)
test_print(result == text1 + text2)


# CBC mode fixture
def setup_cbc():
    return aes(key, AES.MODE_CBC, IV)

# CBC mode tests
cipher = setup_cbc()
result = cipher.encrypt(b'testing cbc 123.')
test_print(result)

cipher = setup_cbc()
result = cipher.decrypt(result)
test_print(result)

cipher = setup_cbc()
text1 = b'this is a test..'
text2 = b'and another one.'
result1 = cipher.encrypt(text1)
result2 = cipher.encrypt(text2)
cipher = setup_cbc()
result = cipher.encrypt(text1 + text2)
test_print(result == result1 + result2)
cipher = setup_cbc()
result = cipher.decrypt(result1 + result2)
test_print(result)
test_print(result == text1 + text2)

# CTR mode fixture
def setup_ctr():
    if interpreter == 'upython':
        ctr = b'0123456789012345'
    else:
        ctr = Counter.new(64, b'01234567', initial_value=struct.unpack(">Q", b'89012345')[0])
    return aes(key, AES.MODE_CTR, counter=ctr)

# CTR mode tests
cipher = setup_ctr()
result = cipher.encrypt("this is a ctr test")
test_print(result)

cipher = setup_ctr()
test_print(cipher.decrypt(result))

# test CTR ability to continue
cipher = setup_ctr()
cipher.encrypt("lets repeat the test")
test_print(cipher.encrypt("but this time let's continue"))

# test default parameter values

# default mode should be ECB
cipher = aes(key)
test_print(cipher.encrypt(b'testing ecb 123.'))

# test exceptions
try:
    cipher = aes(b'shorterkey')
except Exception as e:
    test_print(e)

try:
    cipher = aes(key, -1)
except Exception as e:
    test_print(e)

try:
    cipher = aes(key, AES.MODE_CBC, IV='shortIV')
except Exception as e:
    test_print(e)

