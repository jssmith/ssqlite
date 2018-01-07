#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "sqlite3.h"

void usage()
{
    fprintf(stderr, "Usage: vfsbench experiment_spec vfs_name data_path block_size blocks_per_thread, num_threads mode\n"
            "    Note that this program does not create the file at data_path\n"
            "    so you must populate it, e.g.:\n"
            "    head -c 100000000 /dev/urandom > /tmp/testfile\n");
}

typedef struct Timing {
    struct timespec start;
    struct timespec end;
} *Timing;

typedef struct TestResult {
    struct Timing overall_timing;
#ifdef TIMING_DETAIL
    struct Timing detail_timing[];
#endif
} *TestResult;

typedef struct test_config {
    const char* vfs_name;
    const char* mode;
    const char* data_path;
    int block_size;
    int blocks_per_thread;
    TestResult test_result;
} *test_config;


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
    return (u_int64_t) (t->end.tv_sec - t->start.tv_sec) * 1000000000
        + ((t->end.tv_nsec - t->start.tv_nsec) % 1000000000);
}

void print_perf(const char* mode, u_int64_t block_size, u_int64_t num_blocks, const Timing t)
{
    u_int64_t elapsed_ns = timing_elapsed_ns(t);
    u_int64_t blocks_per_sec = num_blocks * 1000000000 / elapsed_ns;
    u_int64_t bytes_per_sec = blocks_per_sec * block_size;
    printf("%s, block size %lld: %lld blocks in %lld ns\n    %'lld blocks per sec, %'lld bytes per sec\n",
        mode, block_size, num_blocks, elapsed_ns, blocks_per_sec, bytes_per_sec);
}

void save_perf(const char* file_name, const char* experiment_spec,
    const char* mode, u_int64_t block_size, u_int64_t blocks_per_thread,
    int num_threads, TestResult *results)
{
    FILE *f = fopen(file_name, "a");
    fprintf(f, "{");
    fprintf(f, "\"experiment_spec\":%s,", experiment_spec);
    fprintf(f, "\"mode\":\"%s\",", mode);
    fprintf(f, "\"block_size\":%d,", block_size);
    fprintf(f, "\"blocks_per_thread\":%d,", blocks_per_thread);
    fprintf(f, "\"num_threads\":%d,", num_threads);
    fprintf(f, "\"results\":[", num_threads);
    for (int t=0; t<num_threads; t++) {
        if (t>0) {
            fprintf(f, ",");
        }
        fprintf(f, "{\"thread_start_time\":%d.%09d,",
            results[t]->overall_timing.start.tv_sec,
            results[t]->overall_timing.start.tv_nsec);
        fprintf(f, "\"thread_end_time\":%d.%09d",
            results[t]->overall_timing.end.tv_sec,
            results[t]->overall_timing.end.tv_nsec);
#ifdef TIMING_DETAIL
        fprintf(f, ",\"op_detail\":[");
        for (int i=0; i<blocks_per_thread; i++) {
            if (i>0) {
                fprintf(f, ",");
            }
            fprintf(f, "{\"start_time\":%d.%09d,",
                results[t]->detail_timing[i].start.tv_sec,
                results[t]->detail_timing[i].start.tv_nsec);
            fprintf(f, "\"end_time\":%d.%09d}",
                results[t]->detail_timing[i].end.tv_sec,
                results[t]->detail_timing[i].end.tv_nsec);
        }
        fprintf(f, "]"); // end details
#endif
        fprintf(f, "}"); // end of thread result
    }
    fprintf(f, "]", num_threads); // end results
    fprintf(f, "}\n");
    fclose(f);
}

void fill_random(char* zBuf, u_int64_t size)
{
    // Put some data in the buffer just so it isn't all zeros.
    // Note that random only generates numbers up to 2^31-1 and
    // that we may not fill the last few bytes.
    for (u_int32_t *p=(u_int32_t*)zBuf; p<(u_int32_t*)(zBuf+size); p++) {
        *p = random();
    }
}

int op_sequentially(sqlite3_file* pFile,
                    int (*op)(sqlite3_file*,void*,int,sqlite_int64),
                    const char* op_name,
                    u_int64_t block_size, u_int64_t n_reads,
                    TestResult result, bool sync)
{
    u_int64_t pos = 0;
    u_int64_t ct = 0;
    char *zBuf = sqlite3_malloc(block_size);
    fill_random(zBuf, block_size);
    int rc = SQLITE_OK;
    timing_start(&result->overall_timing);
    while( ct < n_reads) {
#ifdef TIMING_DETAIL
        timing_start(&result->detail_timing[ct]);
#endif
        rc = (*op)(pFile, zBuf, block_size, pos);
#ifdef TIMING_DETAIL
        timing_finish(&result->detail_timing[ct]);
#endif
        if (rc) {
            fprintf(stderr, "Error in sequential %s\n", op_name);
            break;
        }
        pos += block_size;
        ct++;
    }
    if (sync) {
        pFile->pMethods->xSync(pFile, SQLITE_SYNC_NORMAL);
    }
    timing_finish(&result->overall_timing);
    // memcpy(&result->overall_timing, &t, sizeof(struct Timing));
    // print_perf("sequential", op_name, block_size, ct, &t);
    sqlite3_free(zBuf);
    return rc;
}

int op_randomly(sqlite3_file* pFile,
                int (*op)(sqlite3_file*,void*,int,sqlite_int64),
                const char* op_name,
                u_int64_t block_size, u_int64_t n_reads, u_int64_t block_limit,
                TestResult result, bool sync)
{
    u_int64_t pos = 0;
    u_int64_t ct = 0;
    char *zBuf = sqlite3_malloc(block_size);
    fill_random(zBuf, block_size);
    int rc = SQLITE_OK;
    timing_start(&(result->overall_timing));
    while( ct < n_reads) {
        pos = block_size * (random() % block_limit);
#ifdef TIMING_DETAIL
        timing_start(&result->detail_timing[ct]);
#endif
        rc = (*op)(pFile, zBuf, block_size, pos);
#ifdef TIMING_DETAIL
        timing_finish(&result->detail_timing[ct]);
#endif
        if (rc) {
            fprintf(stderr, "Error in random %s, pos: %d, size: %d\n", op_name, pos, block_size);
            break;
        }
        ct++;
    }
    if (sync) {
        pFile->pMethods->xSync(pFile, SQLITE_SYNC_NORMAL);
    }
    timing_finish(&result->overall_timing);
    // print_perf("random", op_name, block_size, ct, &t);
    sqlite3_free(zBuf);
    return rc;
}

void run_test(test_config config)
{
    char* zErrMsg = 0;
    int rc;

    sqlite3_vfs *vfs = sqlite3_vfs_find(config->vfs_name);
    printf("Loaded vfs %s\n", vfs->zName);
    printf("Opening file %s\n", config->data_path);
    sqlite3_file* pFile = sqlite3_malloc(vfs->szOsFile);
    int flags = SQLITE_OPEN_READWRITE;
    rc = vfs->xOpen(vfs, config->data_path, pFile, flags, 0);
    if (rc) {
        fprintf(stderr, "Unable to open file %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_free(pFile);
        exit(1);
    }

    sqlite3_int64 file_size;
    pFile->pMethods->xFileSize(pFile, &file_size);
    if (rc) {
        fprintf(stderr, "Unable to get file size %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_free(pFile);
        exit(1);
    }

    if (strcmp(config->mode, "sr") == 0) {
        op_sequentially(pFile,
            pFile->pMethods->xRead,
            "read",
            config->block_size,
            config->blocks_per_thread,
            config->test_result,
	    false);
    } else if (strcmp(config->mode, "rr") == 0) {
        op_randomly(pFile,
            pFile->pMethods->xRead,
            "read",
            config->block_size,
            config->blocks_per_thread,
            file_size / config->block_size,
            config->test_result,
	    false);
    } else if (strcmp(config->mode, "sw") == 0) {
        op_sequentially(pFile,
            (int (*)(struct sqlite3_file*,void*,int,sqlite_int64)) pFile->pMethods->xWrite,
            "write",
            config->block_size,
            config->blocks_per_thread,
            config->test_result, true);
    } else if (strcmp(config->mode, "rw") == 0) {
        op_randomly(pFile,
            (int (*)(struct sqlite3_file*,void*,int,sqlite_int64)) pFile->pMethods->xWrite,
            "write",
            config->block_size,
            config->blocks_per_thread,
            file_size / config->block_size,
            config->test_result, true);
    } else {
        printf("Unknown mode %s\n", config->mode);
        exit(1);
    }

    sqlite3_free(zErrMsg);
    sqlite3_free(pFile);
}

void* run_test_thread(void* thread_data)
{
    test_config config = (test_config) thread_data;
    run_test(config);
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc != 8) {
        usage();
        exit(1);
    }
    char* experiment_spec = argv[1];
    char* vfs_name = argv[2];
    char* data_path = argv[3];
    int block_size = atoi(argv[4]);
    int blocks_per_thread = atoi(argv[5]);
    int num_threads = atoi(argv[6]);
    char* mode = argv[7];

    srandom(time(0));

    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    rc = sqlite3_open(":memory:", &db);
    if (rc) {
        fprintf(stderr, "Unable to open initialization database %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return (1);
    }

    if (strcmp("nfs4", vfs_name) == 0) {
        rc = sqlite3_enable_load_extension(db, 1);
        if (rc) {
            fprintf(stderr, "Unable to enable extension loading");
            exit(1);
        }

        rc = sqlite3_load_extension(db, "./nfs4", 0, &zErrMsg);
        if (rc) {
            fprintf(stderr, "Unable to load extension nfs4 %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
            exit(1);
        }
    }

    test_config config = malloc(sizeof(struct test_config));
    config->vfs_name = vfs_name;
    config->mode = mode;
    config->data_path = data_path;
    config->block_size = block_size;
    config->blocks_per_thread = blocks_per_thread;

    int t;
    TestResult results[num_threads];
    for (int t=0; t<num_threads; t++) {
#ifdef TIMING_DETAIL
        results[t] = malloc(sizeof(struct TestResult) + blocks_per_thread * sizeof(struct Timing));
#else
        results[t] = malloc(sizeof(struct TestResult));
#endif
    }
    if (num_threads > 1) {
        pthread_t threads[num_threads];
        for (t=0; t<num_threads; t++) {
            test_config thread_config = malloc(sizeof(struct test_config));
            memcpy(thread_config, config, sizeof(struct test_config));
            thread_config->test_result = results[t];
            rc = pthread_create(&threads[t], NULL, run_test_thread, thread_config);
            if (rc) {
                printf("Error in pthread_create %d\n", rc);
                exit(1);
            }
        }
        for (t=0; t<num_threads; t++) {
            pthread_join(threads[t], NULL);
        }
    } else {
        config->test_result = results[0];
        run_test(config);
    }

    // Result printing
    for (int t=0; t<num_threads; t++) {
        printf("thread %d\n", t);
        print_perf(mode, block_size, blocks_per_thread, &results[t]->overall_timing);
    }
    save_perf("perf_log.json", experiment_spec, mode, block_size, blocks_per_thread, num_threads, results);
    return 0;
}
