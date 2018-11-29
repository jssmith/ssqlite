import ctypes

c_helper = ctypes.CDLL('./pysfs.so')

c_helper.create_client_py.argtypes = [ctypes.c_char_p]
c_helper.open_file_py.argtypes = [ctypes.c_char_p]
c_helper.open_file_py.restype = ctypes.py_object
c_helper.read_file_py.argtypes = [ctypes.py_object, ctypes.c_int, ctypes.c_int]

def mount(host_ip):
    b_host_ip = host_ip.encode('utf-8')
    c_helper.create_client_py(b_host_ip)

def open(file_name):
    b_file_name = file_name.encode('utf-8')
    fp = c_helper.open_file_py(b_file_name)
    return FileObjectWrapper(fp)

def read(size):
    c_helper.read_file_py(size)

class FileObjectWrapper:
    def __init__(self, fp):
        self._fp = fp
        self._pos = 0

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        # TODO ensure file closed
        pass

    def seek(self, pos):
        self._pos = pos

    def read(self, size):
        c_helper.read_file_py(self._fp, self._pos, size)
        self._pos += size
