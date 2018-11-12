#include "nfs4.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char **argv) {
   nfs4 client;
   char *server;
   char *filename = "/writer3.txt";
   int block_size = 100;
   char block_content[block_size];

   for (int i = 0; i < block_size; i++) {
     block_content[i] = 'a' + i % 26;
   }
   int num_blocks = 100;
   int num_bytes = block_size * num_blocks;

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
  
  if (nfs4_open(client, filename, NFS4_RDWRITE | NFS4_CREAT, &p, &f) != NFS4_OK) {
      printf("Failed to open %s\n", filename);
      exit(1);
  } 
  assert(f != NULL);

  for (int bytes_written = 0; bytes_written < num_bytes; bytes_written += block_size) {
      int write_status = nfs4_pwrite(f, (void*) block_content, bytes_written, block_size);
      if (write_status != NFS4_OK) { 
          printf("Failed to write to %s\n", filename);
          exit(1);
      } 
  }

  return 0;
}
 
