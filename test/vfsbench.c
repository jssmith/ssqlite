#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sqlite3.h"

void usage()
{
    fprintf(stderr, "Usage: vfsbench vfs_name data_path\n"
            "    Note that this program does not create the file at data_path\n"
            "    so you must populate it, e.g.:\n"
            "    head -c 100000000 /dev/urandom > /tmp/testfile\n");
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
    printf("%s %s, block size %lld: %lld blocks in %lld ns\n    %'lld blocks per sec, %'lld bytes per sec\n",
        mode, op_name, block_size, num_blocks, elapsed_ns, blocks_per_sec, bytes_per_sec);
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

int op_sequentially(sqlite3_file* pFile,
                    int (*op)(sqlite3_file*,void*,int,sqlite_int64),
                    const char* op_name,
                    u_int64_t block_size, u_int64_t n_reads)
{
    u_int64_t pos = 0;
    u_int64_t ct = 0;
    char *zBuf = sqlite3_malloc(block_size);
    fill_random(zBuf, block_size);
    int rc = SQLITE_OK;
    Timing t;
    timing_start(&t);
    while( ct < n_reads ){
        rc = (*op)(pFile, zBuf, block_size, pos);
        if( rc  ){
            fprintf(stderr, "Error in sequential %s\n", op_name);
            break;
        }
        pos += block_size;
        ct++;
    }
    timing_finish(&t);
    print_perf("sequential", op_name, block_size, ct, &t);
    sqlite3_free(zBuf);
    return rc;
}

int op_randomly(sqlite3_file* pFile,
                int (*op)(sqlite3_file*,void*,int,sqlite_int64),
                const char* op_name,
                u_int64_t block_size, u_int64_t n_reads, u_int64_t block_limit)
{
    u_int64_t pos = 0;
    u_int64_t ct = 0;
    char *zBuf = sqlite3_malloc(block_size);
    fill_random(zBuf, block_size);
    int rc = SQLITE_OK;
    Timing t;
    timing_start(&t);
    while( ct < n_reads ){
        pos = block_size * (random() % block_limit);
        rc = (*op)(pFile, zBuf, block_size, pos);
        if( rc  ){
            fprintf(stderr, "Error in random %s, pos: %d, size: %d\n", op_name, pos, block_size);
            break;
        }
        ct++;
    }
    timing_finish(&t);
    print_perf("random", op_name, block_size, ct, &t);
    sqlite3_free(zBuf);
    return rc;
}

int main(int argc, char* argv[])
{
    if( argc != 3 ){
        usage();
        exit(1);
    }
    char* vfs_name = argv[1];
    char* data_path = argv[2];

    srandom(time(0));

    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    if( rc ){
        fprintf(stderr, "Unable to open initialization database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return (1);
    }

    if( strcmp("nfs4", vfs_name) == 0 ){
        rc = sqlite3_enable_load_extension(db, 1);
        if( rc ){
            fprintf(stderr, "Unable to enable extension loading");
            exit(1);
        }

        rc = sqlite3_load_extension(db, "./nfs4", 0, &zErrMsg);
        if( rc ){
            fprintf(stderr, "Unable to load extension nfs4 %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            exit(1);
        }
    }

    sqlite3_vfs *vfs = sqlite3_vfs_find(vfs_name);
    printf("Loaded vfs %s\n", vfs->zName);
    printf("Opening file %s\n", data_path);
    sqlite3_file* pFile = sqlite3_malloc(vfs->szOsFile);
    int flags = SQLITE_OPEN_READWRITE;
    rc = vfs->xOpen(vfs, data_path, pFile, flags, 0);
    if( rc ){
        fprintf(stderr, "Unable to open file %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_free(pFile);
        exit(1);
    }

    sqlite3_int64 file_size;
    pFile->pMethods->xFileSize(pFile, &file_size);
    if( rc ){
        fprintf(stderr, "Unable to get file size %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_free(pFile);
        exit(1);
    }

    printf("Have file size %lld\n", file_size);
    u_int64_t n_reads_limit = 1000;
    u_int64_t total_read = 128 * 1024 * 1024;
    u_int64_t block_sizes[] = { 1024, 4096, 8192, 16384, 32768, 65536 };
    for(int i=0; i< sizeof(block_sizes)/sizeof(block_sizes[0]); i++){
        u_int64_t block_size = block_sizes[i];
        u_int64_t n_reads = total_read / block_size;
        if( n_reads > n_reads_limit ){
            n_reads = n_reads_limit;
        }
        if( n_reads > file_size/block_size ){
            n_reads = file_size/block_size;
        }
        for(int i=0; i<2; i++){
            op_sequentially(pFile,
                pFile->pMethods->xRead,
                "read",
                block_size,
                n_reads);
            op_randomly(pFile,
                pFile->pMethods->xRead,
                "read",
                block_size,
                n_reads,
                file_size / block_size);
            op_sequentially(pFile,
                (int (*)(struct sqlite3_file*,void*,int,sqlite_int64)) pFile->pMethods->xWrite,
                "write",
                block_size,
                n_reads);
            op_randomly(pFile,
                (int (*)(struct sqlite3_file*,void*,int,sqlite_int64)) pFile->pMethods->xWrite,
                "write",
                block_size,
                n_reads,
                file_size / block_size);
        }
    }
    sqlite3_free(zErrMsg);
    sqlite3_free(pFile);
    return 0;
}
