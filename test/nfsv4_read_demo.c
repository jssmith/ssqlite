#include "nfs4.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

 int main(int argc, char **argv) {
     nfs4 client;
     char *server;
     // create a nfs4 client
     if ((server = getenv("NFS4_SERVER"))) {
        if (nfs4_create(server, &client)) {
            printf ("open client fail %s\n", nfs4_error_string(client));
            exit(1);
        }
    }

    // open a file via nfs4
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE; // TODO: need to understand this flag
    p.mode = 0666; // TODO
    nfs4_file f;
    if (nfs4_open(client, "/sample.txt", NFS4_RDONLY, &p, &f) != NFS4_OK) {// TODO: file path
        printf("Failed to open /efs/sample.txt\n");
        exit(1);
    } 
    
    int status = nfs4_fstat(f, &p);
    // read a file via nfs4
    assert(f != NULL);
    void *buffer = calloc(100, 1); // 100 bytes buffer
    if (nfs4_pread(f, buffer, 0, 50) != NFS4_OK) { // read first 50 bytes from that file
        puts("Failed to read /efs/sample.txt\n");
        exit(1);
    } 
    char *content = (char *)buffer;
    printf("----%s----\n", content);


    return 0;
 }
 
