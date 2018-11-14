#include "nfs4_internal.h"
#include <string.h>

char* host_ip;
char* file_name;
client c;
heap h;
status s;
file f;
char data[4096];

void create_client_py(char* h_ip, char* f_name)
{
    host_ip = h_ip;
    file_name = f_name;
  
    printf("argument\n");    
   // heap h = 0;
   // client c;
   // status s;
   // file f;
   // char data[4096];
    printf("host_ip: %s\n", host_ip);
    printf("&c:%d\n", &c);
    s = create_client(host_ip, &c);
    printf("create client");
    if (s != STATUS_OK) {
        printf("failed to open connection to NFS server\n");
        exit(1);
    }
}

void create_file_py() {
    vector fv = allocate_buffer(h, strlen(file_name) + 1);
    push_bytes(fv, file_name, strlen(file_name));
    vector path = allocate_buffer(h, 10);
    vector_push(path, fv);

    printf("file create");
    s = file_create(c, path, &f);
    if (s != STATUS_OK) {
        exit(1);
    }
}

void read_file_py() {
    printf("readfile");
    s = readfile(f, data, 0, 4096);
    if (s != STATUS_OK) {
        exit(1);
    }
    data[4095] = 0;
    printf("read: %s\n", data);
    file_close(f);
}
