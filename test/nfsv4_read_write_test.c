#include "nfs4.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void usage() {
  fprintf(stderr, "Usage nfsv4_test [ read | write ] file\n");
}

typedef struct {
  struct timespec start;
  struct timespec end;
} Timing;

void timing_start(Timing* t)
{
  clock_gettime(CLOCK_MONOTONIC, &(t->start));
}

void timing_finish(Timing* t)
{
  clock_gettime(CLOCK_MONOTONIC, &(t->end));
}

u_int64_t timing_elapsed_ns(const Timing* t)
{
  return (u_int64_t) (t->end.tv_sec - t->start.tv_sec) * 1000000000
    + ((t->end.tv_nsec - t->start.tv_nsec) % 1000000000);
}

void print_perf(const char* mode, const char* op_name, u_int64_t block_size, u_int64_t num_blocks, const Timing* t)
{
  u_int64_t elapsed_ns = timing_elapsed_ns(t);
  u_int64_t blocks_per_sec = num_blocks * 1000000000 / elapsed_ns;
  u_int64_t bytes_per_sec = blocks_per_sec * block_size;
  printf("%s %s, block size %ld: %ld blocks in %ld ns\n", 
    mode, op_name, block_size, num_blocks, elapsed_ns);
  printf("%'ld blocks per sec, %'ld bytes per sec\n", blocks_per_sec, bytes_per_sec);
}

void fill_random(char* zBuf, u_int64_t size)
{
  // Put some data in the buffer just so it isn't all zeros.
  // Note that random only generates numbers up to 2^31-1 and
  // that we may not fill the last few bytes.
  for(u_int32_t *p=(u_int32_t*)zBuf; p<(u_int32_t*)(zBuf+size); p++){
    *p = random();
  }
}

void create_client(nfs4* client) {
   char* server = getenv("NFS4_SERVER");
   if (server == NULL) {
     printf("Failed to read NFS4_SERVER from environment");
     exit(1);
   }

  if (nfs4_create(server, client)) {
    printf ("open client fail %s\n", nfs4_error_string(*client));
    exit(1);
  }
}

void open_file(nfs4 client, char* filename, int flags, nfs4_file* f) {
  struct nfs4_properties p;
  p.mask = NFS4_PROP_MODE; 
  p.mode = 0666; 
  
  if (nfs4_open(client, filename, flags, &p, f) != NFS4_OK) {
    printf("Failed to open %s:%s\n", filename, nfs4_error_string(client));
    exit(1);
  } 
  assert(f != NULL);
}

void test_sequential_write(nfs4 client, char* filename, u_int64_t num_blocks, u_int64_t block_size) {
  printf("Conducting write sequential test\n");

  char block_content[block_size];
  fill_random(block_content, block_size);

  nfs4_file f;
  open_file(client, filename, NFS4_RDWRITE | NFS4_CREAT, &f);

  Timing timing;
  timing_start(&timing);
  for (int bytes_traversed = 0; 
       bytes_traversed < block_size * num_blocks;
       bytes_traversed += block_size) {
    int write_status = nfs4_pwrite(f, (void*) block_content, bytes_traversed, block_size);
    if (write_status != NFS4_OK) { 
      printf("Failed to write to %s:%s\n", filename, nfs4_error_string(client));
      exit(1);
    }
  }
  timing_finish(&timing);

  print_perf("write", "sequential", block_size, num_blocks, &timing);
}

void test_sequential_read(nfs4 client, char* filename, u_int64_t num_blocks, u_int64_t block_size) {
  printf("Conducting read sequential test\n");

  nfs4_file f;
  open_file(client, filename, NFS4_RDONLY, &f);

  // pad a byte for the \0
  void *read_dst = calloc(block_size + 1, 1); 

  Timing timing;
  timing_start(&timing);
  for (int bytes_traversed = 0; 
       bytes_traversed < block_size * num_blocks; 
       bytes_traversed += block_size) {
    int read_status = nfs4_pread(f, read_dst, bytes_traversed, block_size);
    if (read_status != NFS4_OK) { 
      printf("Failed to read %s:%s\n", filename, nfs4_error_string(client));
      exit(1);
    } 
  }
  timing_finish(&timing);

  free(read_dst);

  print_perf("read", "sequential", block_size, num_blocks, &timing);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    usage();
    exit(1);
  }

  char* operation = argv[1];
  char* filename = argv[2];

  void (*test)(nfs4, char*, u_int64_t, u_int64_t);
  if (strcmp("read", operation) == 0) {
    test = test_sequential_read;
  } else if(strcmp("write", operation) == 0) {
    test = test_sequential_write;;
  } else {
    usage();
    exit(1);
  }

  nfs4 client;
  create_client(&client);
  u_int64_t block_size = 128;
  u_int64_t num_blocks = 128;

  test(client, filename, num_blocks, block_size);
}
