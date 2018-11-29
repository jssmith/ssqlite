import ctypes

c_helper = ctypes.CDLL('./pysfs.so')

c_helper.create_client_py.argtypes = [ctypes.c_char_p]
c_helper.open_file_py.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
c_helper.open_file_py.restype = ctypes.py_object
c_helper.read_file_py.argtypes = [ctypes.py_object, ctypes.c_int, ctypes.c_int]
c_helper.read_file.restype = ctypes.py_object
c_helper.write_file_py.argtypes = [ctypes.py_object, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]

def mount(host_ip):
    b_host_ip = host_ip.encode('utf-8')
    c_helper.create_client_py(b_host_ip)

def open(file_name, mode):
    fp = c_helper.open_file_py(file_name.encode('utf-8'), mode.encode('utf-8'))
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

    def seek(self, pos, whence=0):
        self._pos = pos + whence

    def read(self, size):
        res = c_helper.read_file_py(self._fp, self._pos, size)
        self._pos += size
        return res

    def write(self, content_bytes):
        content_len = len(content_bytes)
        c_helper.write_file_py(self._fp, content_bytes, self._pos, content_len)
        self._pos += content_len
