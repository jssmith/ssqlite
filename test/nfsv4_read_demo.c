#include "nfs4.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

 int main(int argc, char **argv) {
     nfs4 client;
     char *server;
     char *filename = "/sample.txt";

     // create a nfs4 client
     server = getenv("NFS4_SERVER");
     if (server == NULL) {
        printf("Failed to read NFS4_SERVER from environment");
        exit(1);
     }

    if (nfs4_create(server, &client)) {
        printf ("open client fail %s\n", nfs4_error_string(client));
        exit(1);
    }

    // open a file via nfs4
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE; // TODO: need to understand this flag
    p.mode = 0666; // TODO
    nfs4_file f;
    
    if (nfs4_open(client, filename, NFS4_RDONLY, &p, &f) != NFS4_OK) {// TODO: file path
        printf("Failed to open %s\n", filename);
        exit(1);
    } 
    
    // read a file via nfs4
    assert(f != NULL);
    void *buffer = calloc(100, 1); // 100 bytes buffer
    int read_status = nfs4_pread(f, buffer, 0, 51);
    if (read_status != NFS4_OK) { // read first 50 bytes from that file
        printf("Failed to read %s\n", filename);
        exit(1);
    } 
    char *content = (char *)buffer;
    printf("result:\t%s\n", content);
    return 0;
 }
 
