import ctypes
import io

c_helper = ctypes.CDLL('./libnfs4.so')


NFS4_RDONLY = 1
NFS4_WRONLY = 2
NFS4_RDWRITE = 3
NFS4_CREAT = 4
NFS4_TRUNC = 16

NFS4_OK = 0
NFS4_ENOENT = 2
NFS4_EACCES = 13
NFS4ERR_OPENMODE = 10038
NFS4_PROP_MODE = 1 << 33


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
c_helper.nfs4_error_num.argtypes = [Nfs4]
c_helper.nfs4_error_num.restype = ctypes.c_int
c_helper.nfs4_error_string.argtypes = [Nfs4]
c_helper.nfs4_error_string.restype = ctypes.c_char_p
c_helper.nfs4_open.argtypes = [
    Nfs4, ctypes.c_char_p, ctypes.c_int, Nfs4_properties, ctypes.POINTER(Nfs4_file)]
c_helper.nfs4_pread.argtypes = [
    Nfs4_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]
c_helper.nfs4_pwrite.argtypes = [
    Nfs4_file, ctypes.c_void_p, ctypes.c_ulonglong, ctypes.c_ulonglong]

client = Nfs4()


def mount(host_ip):
    b_host_ip = host_ip.encode('utf-8')
    print("host_ip: " + host_ip)
    print("create client")
    if c_helper.nfs4_create(b_host_ip, ctypes.pointer(client)) != NFS4_OK:
        print("open client fail: " +
              c_helper.nfs4_error_string(client).decode(encoding='utf-8'))


def open(file_name, mode='r', buffering=io.DEFAULT_BUFFER_SIZE):
    # TODO: needs to validate MODE
    p = Nfs4_properties_struct()
    p.mask = NFS4_PROP_MODE
    p.mode = 0o666

    binary_mode = False
    flags = 0
    for c in mode:
        if c == 'r':
            flags |= NFS4_RDONLY
        elif c == 'w':
            flags |= (NFS4_TRUNC | NFS4_WRONLY | NFS4_CREAT)
        elif c == 'a':
            flags |= (NFS4_WRONLY | NFS4_CREAT)
        elif c == 'b':
            binary_mode = True
        else:
            raise NotImplementedError(
                "flags other than 'r', 'w' and 'a' not supported yet")

    f_ptr = ctypes.pointer(Nfs4_file())
    error_code = c_helper.nfs4_open(client, file_name.encode(
        'utf-8'), flags, ctypes.byref(p), f_ptr)
    if error_code != NFS4_OK:
        if error_code == NFS4_ENOENT:
            raise FileNotFoundError(
                "[Errno 2] No such file or directory: " + "'" + file_name + "'")
        if error_code == NFS4_EACCES:
            raise PermissionError(
                "[Errno 13] Permission denied: " + "'" + file_name + "'")
        print("Failed to open " + file_name + ": " +
              c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
        return
    f = FileObjectWrapper(f_ptr.contents, flags)

    if buffering > 1:
        buffered_f = None
        if bool(flags & NFS4_RDONLY) and bool(flags & NFS4_WRONLY):
            buffered_f = io.BufferedRandom(f, buffering)
        elif (flags & NFS4_RDONLY):
            buffered_f = io.BufferedReader(f, buffering)
        elif (flags & NFS4_WRONLY):
            buffered_f = io.BufferedWriter(f, buffering)
        else:
            raise ValueError("illegal combination of flags")

        if binary_mode:
            return buffered_f

        return io.TextIOWrapper(buffered_f)

    elif buffering == 1:
        raise NotImplementedError("line buffering is not supported yet")
    else:
        return f


class FileObjectWrapper(io.RawIOBase):
    def __init__(self, f, flags):
        self._file = f
        self._pos = 0
        self._closed = False
        self._flags = flags

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.close()

    def close(self):
        """Flush and close this stream."""
        # TODO support nfs4_close
        self._closed = True

    @property
    def closed(self):
        return self._closed

    def fileno(self):
        """Return the underlying file descriptor"""
        raise io.UnsupportedOperation

    def flush(self):
        """
        Flush the write buffers of the stream if applicable. 

        This does nothing for read-only and non-blocking streams.
        """
        pass

    def isatty(self):
        """Return True if the stream is interactive."""
        return False

    def readable(self):
        """Return True if the stream can be read from."""
        return bool(self._flags & NFS4_RDONLY)

    def readline(self, size=-1):
        """
        Read and return one line from the stream. 

        If size is specified, at most size bytes will be read.
        The line terminator is always b'\n' for binary files; for text files, the newline argument to open() can be used to select the line terminator(s) recognized.
        """
        raise io.UnsupportedOperation

    def readlines(self, hint=-1):
        """
        Read and return a list of lines from the stream. 

        hint can be specified to control the number of lines read: no more lines will be read if the total size (in bytes/characters) of all lines so far exceeds hint.
        """
        raise io.UnsupportedOperation

    def seek(self, offset, whence=io.SEEK_SET):
        """Change the stream position to the given byte offset. 

        offset is interpreted relative to the position indicated by whence. The default value for whence is SEEK_SET. Values for whence are:

            SEEK_SET or 0 - start of the stream (the default); offset should be zero or positive
            SEEK_CUR or 1 - current stream position; offset may be negative
            SEEK_END or 2 - end of the stream; offset is usually negative

        Return the new absolute position.
        """
        if whence == io.SEEK_SET:
            if offset < 0:
                raise ValueError("illegal value of offset")
            self._pos = offset
        elif whence == io.SEEK_CUR:
            self._pos += offset
        elif whence == io.SEEK_END:
            raise NotImplementedError("seeking from end not yet implemented")
        else:
            raise ValueError("illegal value of whence")
        return self._pos

    def seekable(self):
        """Return True if the stream supports random access."""
        return True

    def tell(self):
        """Return the current stream position."""
        return self._pos

    def truncate(self, size=None):
        """
        Resize the stream to the given size in bytes (or the current position if size is not specified). 

        The current stream position isn't changed. This resizing can extend or reduce the current file size. In case of extension, the contents of the new file area depend on the platform (on most systems, additional bytes are zero-filled). The new file size is returned.
        """
        raise io.UnsupportedOperation

    def writable(self):
        """Return True if the stream supports writing."""
        return bool(self._flags & NFS4_WRONLY)

    def writelines(self, lines):
        """
        Write a list of lines to the stream.

        Line separators are not added, so it is usual for each of the lines provided to have a line separator at the end.
        """
        total_bytes_written = 0
        for line in lines:
            bytes_written = self.write(line)
            if bytes_written < 0:
                return total_bytes_written
            total_bytes_written += bytes_written
        return total_bytes_written

    def read(self, size=-1):
        """
        Read up to size bytes from the object and return them.

        If size is -1, all bytes until EOF are returned with possibly 
        multiple underlying calls. If size is specified, only one call will be
        made.

        If 0 bytes are returned and size was not 0, the end of file has been reached.
        """
        if size == -1:
            return self.readall()
        elif size < 0:
            raise ValueError("size must be >= -1")
        buffer = ctypes.create_string_buffer(size)
        bytes_read = c_helper.nfs4_pread(self._file, buffer, self._pos, size)
        if bytes_read < 0:
            if c_helper.nfs4_error_num(client) == NFS4ERR_OPENMODE:
                raise io.UnsupportedOperation("not readable")
            print("Failed to read file: " +
                  c_helper.nfs4_error_string(client).decode(encoding='utf-8'))
            return
        self._pos += len(buffer.value)
        return buffer.value

    def readall(self):
        """
        Read and return all the bytes from the stream until EOF.

        Use multiple calls if necessary. Return None upon error.
        """
        segments = b''

        segment = self.read(io.DEFAULT_BUFFER_SIZE)
        while segment:
            segments += segment
            segment = self.read(io.DEFAULT_BUFFER_SIZE)
        return segments

    def readinto(self, b):
        """
        Read bytes into a pre-allocated, writable bytes-like object b, and return the number of bytes read. 

        If the object is in non-blocking mode and no bytes are available, None is returned.
        """
        length = len(b)
        data = self.read(length)
        b[:len(data)] = data
        return len(data) or None

    def write(self, content_bytes):
        array = ctypes.c_byte * len(content_bytes)
        bytes_written = c_helper.nfs4_pwrite(
            self._file,
            array.from_buffer_copy(content_bytes),
            self._pos,
            len(content_bytes))
        if bytes_written < 0:
            if c_helper.nfs4_error_num(client) == NFS4ERR_OPENMODE:
                raise io.UnsupportedOperation("not writable")
            print("Failed to write: ", c_helper.nfs4_error_string(
                client).decode(encoding='utf-8'))
            return None
        self._pos += bytes_written
        return bytes_written
