#include "nfs4.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

int main(int argc, char **argv) {

   nfs4 client;
   char *server;
   char *filename = "/sample.txt";
   int block_size = 100;
   char block_content[block_size];
   int num_blocks = 100;
   int num_bytes = block_size * num_blocks;
   struct timespec tstart={0,0}, tend={0,0};

   bool test_read = false;
   if (test_read) {
     printf("Conducting read test\n");
   } else {
     printf("Conducting write test\n");
   }

   if (!test_read) {
     // fill the block we will repreatedly write
     for (int i = 0; i < block_size; i++) {
       block_content[i] = 'a' + i % 26;
     }
   }

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
  p.mask = NFS4_PROP_MODE; 
  p.mode = 0666; 
  nfs4_file f;
  
  int open_flags;
  if (test_read) {
    open_flags = NFS4_RDONLY;
  } else {
    open_flags = NFS4_RDWRITE | NFS4_CREAT;
  }

  if (nfs4_open(client, filename, open_flags, &p, &f) != NFS4_OK) {
      printf("Failed to open %s:%s\n", filename, nfs4_error_string(client));
      exit(1);
  } 
  assert(f != NULL);

  // pad a byte for the \0
  void *read_dst = calloc(block_size + 1, 1); 

  clock_gettime(CLOCK_MONOTONIC, &tstart);
  for (int bytes_traversed = 0; bytes_traversed < num_bytes; bytes_traversed += block_size) {
    if (test_read) {
      int read_status = nfs4_pread(f, read_dst, bytes_traversed, block_size);
      if (read_status != NFS4_OK) { 
          printf("Failed to read %s:%s\n", filename, nfs4_error_string(client));
          exit(1);
      } 
    } else {
      int write_status = nfs4_pwrite(f, (void*) block_content, bytes_traversed, block_size);
      if (write_status != NFS4_OK) { 
          printf("Failed to write to %s:%s\n", filename, nfs4_error_string(client));
          exit(1);
      } 
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &tend);

  double diff = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
  long long block_speed = num_blocks / diff;
  long long byte_speed  = num_bytes / diff;

  char *results_format = " %.5f seconds. \t%lld blocks per second. \t%lld bytes per second\n";
  printf(results_format, diff, block_speed, byte_speed);

  return 0;
}
