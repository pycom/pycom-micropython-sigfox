#To run this test an SD card is needed
from machine import SD

f_path = "/flash/t.txt"
sd_path = "/sd/t.txt" 

def file_write(path, mode, text=""):
    f = open(path, mode)
    print("{0}, f.write(mode={1}): {2}".format(path, mode, f.write(text)))
    f.close()

def file_read(path, mode):
    f = open(path, mode)
    print("{0}, f.read(mode={1}): {2}".format(path, mode, f.read()))
    f.close()

def file_read_size_test(path):
    f = open(path, "w")
    f.write("0123456789")
    f.close()

    f = open(path, "r")
    b = bytearray() 
    for c in range(0,10):
        b.append(ord(f.read(1)))

    print(b)
    f.close()

    f = open(path, "r")
    print("{0}, f.read(2): {1}".format(path, f.read(2)))
    print("{0}, f.read(3): {1}".format(path, f.read(3)))
    print("{0}, f.read(5): {1}".format(path, f.read(5)))
    f.close()

def file_readinto_test(path):
    f = open(path, "r")
    b_1 = bytearray(5)
    length = f.readinto(b_1,5)
    print("{0}, f.readinto(b): {1}, length: {2}".format(path, b_1, length))
    b_2 = bytearray(2)
    length = f.readinto(b_2,2)
    print("{0}, f.readinto(b,2): {1}, length: {2}".format(path, b_2, length))
    f.close()

def file_readline_test(path):
    f = open(path, "w")
    f.write("01234\n56789")
    f.close()
    f = open(path, "r")
    print("{0}, f.readline(10): {1}".format(path, f.readline(10)))
    f.close()
    f = open(path, "r")
    print("{0}, f.readlines(): {1}".format(path, f.readlines()))
    f.close()

def file_read_write(path, mode, text=""):
    f = open(path, mode)
    print("{1}, f.write(mode={2}): {0}".format(f.write(text), path, mode))
    f.seek(0)
    print("{1}, f.read(mode={2}): {0}".format(f.read(), path, mode))
    f.close()

def file_seek_test(path):
    f = open(path, "r")
    f.seek(3)
    print("{0}, f.seek({1}, {2}): {3}".format(path, 3, 0, f.read(1)))
    print("{0}, f.tell(): {1}".format(path, f.tell()))
    f.seek(2, 1)
    print("{0}, f.seek({1}, {2}): {3}".format(path, 2, 1, f.read(1)))
    print("{0}, f.tell(): {1}".format(path, f.tell()))
    f.seek(-1, 2)
    print("{0}, f.seek({1}, {2}): {3}".format(path, 1, 2, f.read(1)))
    print("{0}, f.tell(): {1}".format(path, f.tell()))
    f.close()

def file_double_close_test(path):
    f = open(path)
    # Nothing should be printed out
    f.close()
    f.close()
    f.close()

def file_operations_after_close_test(path):
    f = open(path, "rw")
    f.close()
    try:
        f.read()
    except Exception as e:
        print("{0}, Exception after f.read: {1}".format(path, e))
    try:
        f.write("aaa")
    except Exception as e:
        print("{0}, Exception after f.write: {1}".format(path, e))
    try:
        f.flush()
    except Exception as e:
        print("{0}, Exception after f.flush: {1}".format(path, e))
    # No exception should be dropped
    f.seek(12)
    f.tell()


sd = SD()
sd_fat_fs = os.mkfat(sd)
os.mount(sd_fat_fs, "/sd")

#Test modes of file opening 
file_write(f_path, "w", "Test text.")
file_write(sd_path, "w", "Test text.")
file_read(f_path, "r")
file_read(sd_path, "r")

file_write(f_path, "a", "Appended text!")
file_write(sd_path, "a", "Appended text!")
file_read(f_path, "r")
file_read(sd_path, "r")

file_write(f_path, "w")
file_write(sd_path, "w")
file_read(f_path, "r")
file_read(sd_path, "r")

os.remove(f_path)
os.remove(sd_path)

file_write(f_path, "x", "Test text.")
file_write(sd_path, "x", "Test text.")
file_read(f_path, "r")
file_read(sd_path, "r")

try:
    file_write(f_path, "x", "New text")
except OSError as e:
    print(repr(e))
        
try:
    file_write(sd_path, "x", "New text")
except OSError as e:
    print(repr(e))

file_read(f_path, "r")
file_read(sd_path, "r")

file_read_write(f_path, "w+", "New text 1.")
file_read_write(sd_path, "w+", "New text 1.")

file_read_write(f_path, "r+", "New text 2.")
file_read_write(sd_path, "r+", "New text 2.")

try:
    file_write(f_path, "x+", "New text")
except OSError as e:
    print(repr(e))
        
try:
    file_write(sd_path, "x+", "New text")
except OSError as e:
    print(repr(e))

os.remove(f_path)
os.remove(sd_path)

file_read_write(f_path, "x+", "New text 3.")
file_read_write(sd_path, "x+", "New text 3.")

file_read_write(f_path, "a+", "Appended text!")
file_read_write(sd_path, "a+", "Appended text!")

#Test file.read(size)
file_read_size_test(f_path)
file_read_size_test(sd_path)

#Test file.seek, file.tell
file_seek_test(f_path)
file_seek_test(sd_path)

#Test file.readinto
file_readinto_test(f_path)
file_readinto_test(sd_path)

#Test file.readline, file.readlines
file_readline_test(f_path)
file_readline_test(sd_path)

#Test multiple file.close
file_double_close_test(f_path)
file_double_close_test(sd_path)

#Test other operations after file.close()
file_operations_after_close_test(f_path)
file_operations_after_close_test(sd_path)

os.remove(f_path)
os.remove(sd_path)
os.umount("/sd")
sd.deinit()