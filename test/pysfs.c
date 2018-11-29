#include "nfs4_internal.h"
#include <string.h>

// TODO get Python include path right
#include "python3.7m/Python.h"

char* host_ip;
char* file_name;
heap h;
status s;
char data[4096];

nfs4 client;

// Initialize the client global variable.
void create_client_py(char* host_ip)
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
PyObject *open_file_py(char* file_name) {
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;

    nfs4_file f = malloc(sizeof(struct nfs4_file));
    if (nfs4_open(client, file_name, NFS4_RDONLY, &p, &f) != NFS4_OK) {
        printf("Failed to open %s:%s\n", file_name, nfs4_error_string(client));
        exit(1);
    }
    assert(f != NULL);
    PyObject *obj = PyCapsule_New(f, "File", &freeFile);
    return obj;
}

void read_file_py(PyObject *obj, int offset, int size) {
    nfs4_file f = PyCapsule_GetPointer(obj, "File");
    void *read_dst = calloc(size + 1, 1);
    int read_status = nfs4_pread(f, read_dst, offset, size);
    if (read_status != NFS4_OK) {
        printf("Failed to read file:%s\n", nfs4_error_string(client));
        exit(1);
    }
    printf("---content---\n%s\n-------------\n", (char*) read_dst);
    free(read_dst);
}
