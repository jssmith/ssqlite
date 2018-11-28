#include "nfs4_internal.h"
#include <string.h>

char* host_ip;
char* file_name;
heap h;
status s;
char data[4096];

nfs4 client;
nfs4_file f;

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

// Open file_name into nfs4_file f with read only access.
void open_file_py(char* file_name) {
  printf("open file\n");
  struct nfs4_properties p;
  p.mask = NFS4_PROP_MODE; 
  p.mode = 0666; 
  
  if (nfs4_open(client, file_name, NFS4_RDONLY, &p, &f) != NFS4_OK) {
    printf("Failed to open %s:%s\n", file_name, nfs4_error_string(client));
    exit(1);
  } 
  assert(f != NULL);
}

void read_file_py(int size) {
  void *read_dst = calloc(size + 1, 1); 
  int read_status = nfs4_pread(f, read_dst, 0, size);
  if (read_status != NFS4_OK) { 
    printf("Failed to read file:%s\n", nfs4_error_string(client));
    exit(1);
  } 
  printf("---content---\n%s\n-------------\n", (char*) read_dst);
  free(read_dst);
}
