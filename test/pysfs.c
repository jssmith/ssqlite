#include "nfs4_internal.h"
#include <string.h>

#include <Python.h>

char* host_ip;
char* file_name;
heap h;
status s;
char data[4096];

nfs4 client;

// Initialize the client global variable.
void create_client_py(const char* host_ip)
{
    printf("host_ip: %s\n", host_ip);
    printf("create client\n");

    if (nfs4_create(host_ip, &client)) {
        printf ("open client fail %s\n", nfs4_error_string(client));
        exit(1);
    }
}

void freeFile(PyObject *obj) {
    free(PyCapsule_GetPointer(obj, "File"));
}

// Open file_name into nfs4_file f with read only access.
PyObject *open_file_py(const char* file_name, const char* mode) {
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;
    int flags = file_open_mode(mode);

    nfs4_file f = malloc(sizeof(struct nfs4_file));
    if (nfs4_open(client, file_name, flags, &p, &f) != NFS4_OK) {
        printf("Failed to open %s:%s\n", file_name, nfs4_error_string(client));
        exit(1);
    }
    assert(f != NULL);
    PyObject *obj = PyCapsule_New(f, "File", &freeFile);
    return obj;
}

PyObject *read_file_py(PyObject *obj, int offset, int size) {
    nfs4_file f = PyCapsule_GetPointer(obj, "File");
    char buff[size];
    int read_status = nfs4_pread(f, buff, offset, size);
    if (read_status != NFS4_OK) {
        printf("Failed to read file:%s\n", nfs4_error_string(client));
        exit(1);
    }
    return PyBytes_FromStringAndSize(buff, size);
}

void write_file_py(PyObject *obj, const void *data, int offset, int len) {
    nfs4_file f = PyCapsule_GetPointer(obj, "File");
    int write_status = nfs4_pwrite(f, data, offset, len);
    if (write_status != NFS4_OK) {
        printf("Failed to write:%s\n", nfs4_error_string(client));
        exit(1);
    }
}

/* Assume MODE has been validated before this function get called*/
int file_open_mode(const char *mode) {
    char *ptr = mode;
    int flags = 0;
    while (*ptr != '\0') {
        switch (*ptr) {
            case 'r': // open for reading
                flags |= NFS4_RDONLY;
                break;
            case 'w': // open for writing, truncating the file first if it already exists
                flags |= (NFS4_TRUNC | NFS4_WRONLY | NFS4_CREAT);
                break 
            case 'a': // open for writing, appending if it exits
                flags |= (NFS4_WRONLY | NFS4_CREAT);
                break;
            case 'x':
                break;
            case 'b':
                break;
            case 't':
                break;
            case '+':
                break;
            case 'U':
                break;
        }
        ptr++;
    }
    return flags;
}