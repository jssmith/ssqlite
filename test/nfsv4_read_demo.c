#include "nfs4.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

 int main(int argc, char **argv) {
     nfs4 client;
     char *server;
     char *filename = "/aeneid.txt";
     int block_size = 100;
     int num_blocks = 100;
     int num_bytes = block_size * num_blocks;
     struct timespec tstart={0,0}, tend={0,0};

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

    // pad a byte for the \0
    void *buffer = calloc(block_size + 1, 1); 
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (int bytes_read = 0; bytes_read < num_bytes; bytes_read += block_size) {
      int read_status = nfs4_pread(f, buffer, bytes_read, block_size);
      if (read_status != NFS4_OK) { 
          printf("Failed to read %s\n", filename);
          exit(1);
      } 
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    double diff = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
    char *content = (char *)buffer;
    printf("last block:\t%s\n", content);
    printf(" %.5f seconds. \t%lld blocks per second. \t%lld bytes per second\n", diff, (long long)(num_blocks/diff), (long long)(num_bytes/diff) );

    return 0;
 }
