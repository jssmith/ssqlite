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
  } else {
    printf ("failed to get nfs4 server from env\n");
    exit(1);
  }

  // open a dir via nfs4
  struct nfs4_properties p;
  nfs4_dir dir;
  if (nfs4_opendir(client, "/", &dir) != NFS4_OK) {
      printf("Failed to open /\n");
      exit(1);
  } 
  
  // read dir
  assert(dir != NULL);
  if (nfs4_readdir(d, p) != NFS4_OK) { 
      puts("failed to read /\n");
      exit(1);
  } 
  return 0;
}
 
