import ctypes
import io

c_helper = ctypes.CDLL('../nfsv4/libnfs4.so')

NFS4_RDONLY = 1
NFS4_WRONLY = 2
NFS4_RDWRITE = 3
NFS4_CREAT = 4
NFS4_TRUNC = 16

NFS4_OK = 0
NFS4_ENOENT = 2
NFS4_EACCES = 13
NFS4ERR_OPENMODE = 10038
NFS4_PROP_MODE = 1<<33

class Nfs4_struct(ctypes.Structure):
    pass
Nfs4 = ctypes.POINTER(Nfs4_struct) 

class Nfs4_properties_struct(ctypes.Structure):
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
Nfs4_properties = ctypes.POINTER(Nfs4_properties_struct)

class Nfs4_file_struct(ctypes.Structure):
    pass
Nfs4_file = ctypes.POINTER(Nfs4_file_struct)

c_helper.nfs4_create.argtypes = [ctypes.c_char_p, ctypes.POINTER(Nfs4)]
c_helper.nfs4_error_string.argtypes = [Nfs4]
c_helper.nfs4_error_string.restype = ctypes.c_char_p
c_helper.nfs4_open.argtypes = [Nfs4, ctypes.c_char_p, ctypes.c_int, Nfs4_properties, ctypes.POINTER(Nfs4_file)]
c_helper.nfs4_pread.argtypes = [Nfs4_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]
c_helper.nfs4_write.argtypes = [Nfs4_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]

client = Nfs4()

def mount(host_ip):
    b_host_ip = host_ip.encode('utf-8')
    print("host_ip: " + host_ip)
    print("create client")
    if c_helper.nfs4_create(b_host_ip, ctypes.pointer(client)) != NFS4_OK:
        print("open client fail: " + c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
    
def open(file_name, mode='r'):
    #TODO: needs to validate MODE
    p = Nfs4_properties_struct()
    p.mask = NFS4_PROP_MODE
    p.mode = 0o666
    
    flags = 0
    for c in mode:
        if c == 'r':
            flags |= NFS4_RDONLY
        elif c == 'w':
            flags |= (NFS4_TRUNC | NFS4_WRONLY | NFS4_CREAT)
        elif c == 'a':
            flags |= (NFS4_WRONLY | NFS4_CREAT)
        else:
            raise NotImplementedError("flags other than 'r', 'w' and 'a' not supported yet")
    
    f_ptr = ctypes.pointer(Nfs4_file()) 
    error_code = c_helper.nfs4_open(client, file_name.encode('utf-8'), flags, ctypes.byref(p), f_ptr)
    if error_code != NFS4_OK:
        if error_code == NFS4_ENOENT:
            raise FileNotFoundError("[Errno 2] No such file or directory: " + "'" + file_name + "'")
        if error_code == NFS4_EACCES:
            raise PermissionError("[Errno 13] Permission denied: " + "'" + file_name + "'")
        print("Failed to open " + file_name + ": " + c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
        return
    return FileObjectWrapper(f_ptr.contents)

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

    def read(self, size=-1):
        buffer = ctypes.create_string_buffer(size)
        read_status = c_helper.nfs4_pread(self._file, buffer, self._pos, size)
        if read_status != NFS4_OK:
            if read_status == NFS4ERR_OPENMODE:
                raise io.UnsupportedOperation("not readable") 
            print("Failed to read file: " + c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
            return
        self._pos += size
        return buffer.value

    def write(self, content_bytes):
        content_len = len(content_bytes)
        cp = ctypes.c_char_p(bytes(content_bytes, 'utf-8'))
        write_status = c_helper.nfs4_write(self._file, ctypes.cast(cp, ctypes.c_void_p), self._pos, content_len)
        if write_status != NFS4_OK:
            if write_status == NFS4ERR_OPENMODE:
                raise io.UnsupportedOperation("not writable") 
            print("Failed to write: ", c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
            return
        self._pos += content_len
        return content_len
