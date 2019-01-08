import ctypes

c_helper = ctypes.CDLL('../nfsv4/libnfs4.so')

NFS4_RDONLY = 1
NFS4_WRONLY = 2
NFS4_RDWRITE = 3
NFS4_CREAT = 4
NFS4_TRUNC = 16

NFS4_OK = 0
NFS4_ENOENT = -2

class Nfs4(ctypes.Structure):
    pass
nfs4Ptr = ctypes.POINTER(Nfs4)


class Nfs4_properties(ctypes.Structure):
    _fields_ = [
        ('mask', ctypes.c_ulonglong),
        ('name', ctypes.c_char * 256),
        ('ino', ctypes.c_char * 128),
        ('mode', ctypes.c_int),
        ('nlink', ctypes.c_int),
        ('user', ctypes.c_uint),
        ('group', ctypes.c_uint),
        ('size', ctypes.c_ulonglong),
        ('type', ctypes.c_int),
        ('access_time', ctypes.c_ulonglong),
        ('modify_time', ctypes.c_ulonglong)
    ]

class Stateid(ctypes.Structure):
    _fileds = [
        ('sequence', ctypes.c_uint),
        ('opaque', ctypes.c_char * 12)
    ]

class Nfs_file(ctypes.Structure):
    _fields_ = [
        ('c', Nfs4),
        ('path', ctypes.c_char_p),
        ('filehandle', ctypes.c_char * 64),
        ('latest_sid', Stateid),
        ('open_sid', Stateid),
        ('asynch_writes', ctypes.c_bool),
        ('expected_size', ctypes.c_ulong)
    ]

c_helper.nfs4_create.argtypes = [ctypes.c_char_p, nfs4Ptr]
c_helper.nfs4_error_string.argtypes = [nfs4Ptr]
c_helper.nfs4_error_string.restype = ctypes.c_char_p
c_helper.nfs4_open.argtypes = [Nfs4, ctypes.c_char_p, ctypes.c_int, Nfs4_properties, ctypes.POINTER(Nfs_file)]
c_helper.nfs4_pread.argtypes = [Nfs_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]
c_helper.nfs4_pwrite.argtypes = [Nfs_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]

client = Nfs4()

def mount(host_ip):
    b_host_ip = host_ip.encode('utf-8')
    print("host_ip: " + host_ip)
    print("create client")
    if c_helper.nfs4_create(b_host_ip, client) != 0:
        print("open client fail " + c_helper.nfs4_error_string(client))

def open(file_name, mode='r'):
    #TODO: needs to validate MODE
    p = Nfs4_properties()
    p.mask = 1<<33
    p.mode = 0o666
    
    flags = 0
    for c in mode:
        if c == 'r':
            flags |= NFS4_RDONLY
        elif c == 'w':
            flags |= (NFS4_TRUNC | NFS4_WRONLY | NFS4_CREAT)
        elif c == 'a':
            flags |= (NFS4_WRONLY | NFS4_CREAT)
    
    f = Nfs_file()
    error_code = c_helper.nfs4_open(client, file_name.encode('utf-8'), flags, p, ctypes.pointer(f))
    if error_code != NFS4_OK:
        if error_code == -NFS4_ENOENT:
            raise FileNotFoundError("File does not exist")
        print("Failed to open " + file_name.encode('utf-8') + ": " + c_helper.nfs4_error_string(client))
        return

    return FileObjectWrapper(f)


class FileObjectWrapper:
    def __init__(self, f):
        self._file = f
        self._pos = 0

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        # TODO ensure file closed
        pass

    def seek(self, pos, whence=0):
        if whence == 0:
            self._pos = pos
        elif whence == 1:
            self._pos += pos
        elif whence == 2:
            raise NotImplementedError("seeking from end not yet implemented")
        else:
            raise ValueError("illegal value of whence")

    def read(self, size):
        f = self._file
        buffer = (ctypes.c_char * size)()
        read_status = c_helper.nfs4_pread(f, ctypes.pointer(buffer), self._pos, size)
        if read_status != NFS4_OK:
            print("Failed to read file: " + c_helper.nfs4_error_string(client))
            return
        self._pos += size
        return str(bytearray(buffer))

    def write(self, content_bytes):
        content_len = len(content_bytes)
        write_status = c_helper.nfs4_pwrite(self._file, content_bytes, self._pos, content_len);
        if write_status != NFS4_OK:
            print("Failed to write: ", c_helper.nfs4_error_string(client));
            return
        self._pos += content_len
