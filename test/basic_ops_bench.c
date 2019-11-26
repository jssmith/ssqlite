#include "nfs4_internal.h"
// #include "nfs4.h"
// #include "nfs4xdr.h"
#include <time.h>
#include <string.h>


void usage()
{
    fprintf(stderr, "Usage: basic_ops_bench host_name file_name block_size runtime_sec mode\n"
            "    Note that this program does not create the file at data_path\n"
            "    so you must populate it, e.g.:\n"
            "    head -c 100000000 /dev/urandom > /efs/testfile\n");
}


void fh_str(char* dst, struct file* f)
{
    for (int i = 0; i < NFS4_FHSIZE; i++) {
        sprintf(&(dst[2*i]), "%02x", f->filehandle[i]);
    }
    dst[2*NFS4_FHSIZE] = 0;
}

typedef struct Timing {
    struct timespec start;
    struct timespec end;
} *Timing;

void timing_start(Timing t)
{
    clock_gettime(CLOCK_MONOTONIC, &(t->start));
}

void timing_finish(Timing t)
{
    clock_gettime(CLOCK_MONOTONIC, &(t->end));
}

u_int64_t timing_elapsed_ns(const Timing t)
{
    return (u_int64_t) (t->end.tv_sec - t->start.tv_sec) * 1000000000 + (t->end.tv_nsec - t->start.tv_nsec);
}

/*
void fill_random(char* zBuf, u_int64_t size)
{
    // Put some data in the buffer just so it isn't all zeros.
    // Note that random only generates numbers up to 2^31-1 and
    // that we may not fill the last few bytes.
    for (u_int32_t *p=(u_int32_t*)zBuf; p<(u_int32_t*)(zBuf+size); p++) {
        *p = random();
    }
}
*/

void do_benchmark(char *hostname_str, char *filename_str, int block_size, int runtime_sec, char *mode)
{
    heap h = 0;
    client c;
    file f, jf;
    status s;
    char buff[block_size];

    s = create_client(hostname_str, &c);
    if (s != STATUS_OK) {
        exit(1);
    }

    vector filename = allocate_buffer(h, strlen(filename_str) + 1);
    push_bytes(filename, filename_str, strlen(filename_str));
    printf("have length of filename %d\n", strlen(filename_str));
    vector path = allocate_buffer(h, 10);
    vector_push(path, filename);

    printf("file open write\n");
    s = file_open_write(c, path, &f);
    if (s != STATUS_OK) {
        exit(1);
    }

    u_int64_t pos = 0;
    struct Timing t;
    timing_start(&t);
    u_int64_t elapsed_time_ns = 0;
    u_int64_t op_ct = 0;
    printf("running for %d sec\n", runtime_sec);
    while (elapsed_time_ns / 1000000000 < runtime_sec) {
        for (int i = 0; i < 10; i++) {
            s = readfile(f, buff, pos, block_size);
            if (s != STATUS_OK) {
                exit(1);
            }
            op_ct += 1;
        }
        timing_finish(&t);
        elapsed_time_ns = timing_elapsed_ns(&t);
    }
    printf(">> %d %lld\n", op_ct, elapsed_time_ns);

    file_close(f);
}


int main(int argc, char* argv[])
{
    if (argc != 6) {
        usage();
        exit(1);
    }
    char* hostname_str = argv[1];
    char* filename_str = argv[2];
    int block_size = atoi(argv[3]);
    int runtime_sec = atoi(argv[4]);
    char* mode = argv[5];
    do_benchmark(hostname_str, filename_str, block_size, runtime_sec, mode);
}
