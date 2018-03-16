#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <sys/param.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define TCBL_TEST_PAGE_SIZE 64
#define TCBL_TEST_MAX_FH 2

#include "tcbl_vfs.h"
#include "test_tcbl.h"
#include "memvfs.h"
#include "unixvfs.h"
#include "runtime.h"

#define TCBL_TEST_FILENAME "/test-file"
#define TCBL_UNIX_TEST_DIR "/tmp/test-tcbl"

#define RC_OK(__x) assert_int_equal(__x, TCBL_OK)
#define RC_NOT_OK(__x) assert_int_not_equal(__x, TCBL_OK)
#define RC_EQ(__x, __erc) assert_int_equal(__x, __erc)

#define verify_length(__fh, __el) { size_t file_size; RC_OK(vfs_file_size(__fh, &file_size)); assert_int_equal(file_size, __el); }
#define verify_data(__fh, __ed, __offs, __len) { char buff[__len]; RC_OK(vfs_read(__fh, buff, __offs, __len)); assert_memory_equal(buff, __ed, __len); }
#define verify_file(__fh, __ed, __el) { verify_length(__fh, __el); verify_data(__fh, __ed, 0, __el); }


void prep_data(char* data, size_t data_len, uint64_t seed)
{
    // TODO make sure this is good
    uint64_t m = ((uint64_t) 2)<<32;
    uint64_t a = 1103515245;
    uint64_t c = 12345;
    for (int i = 0; i < data_len; i++) {
        seed = (a * seed + c) % m;
        data[i] = (char) seed;
    }
}

static void test_vfs_open(void **state)
{
    test_env env = *state;
    vfs_fh fh;

    RC_OK(vfs_open(env->test_vfs, TCBL_TEST_FILENAME, &fh));
    assert_non_null(fh);
    assert_ptr_equal(env->test_vfs, fh->vfs);

    RC_OK(vfs_close(fh));

    RC_OK(env->cleanup(env));
}

static void test_vfs_exists(void **state)
{
    test_env env = *state;
    vfs test_vfs = env->test_vfs;
    vfs_fh fh;

    int exists;
    char *fn1 = TCBL_TEST_FILENAME;
    char *fn2 = "/test-file-2";

    RC_OK(vfs_exists(test_vfs, fn1, &exists));
    assert_false(exists);

    RC_OK(vfs_exists(test_vfs, fn2, &exists));
    assert_false(exists);

    RC_OK(vfs_open(test_vfs, fn1, &fh));
    assert_non_null(fh);
    assert_ptr_equal(test_vfs, fh->vfs);

    RC_OK(vfs_exists(test_vfs, fn1, &exists));
    assert_true(exists);

    RC_OK(vfs_close(fh));

    RC_OK(vfs_exists(test_vfs, fn1, &exists));
    assert_true(exists);

    RC_OK(vfs_exists(test_vfs, fn2, &exists));
    assert_false(exists);

    RC_OK(env->cleanup(env));
}

static void test_write_read(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    RC_OK(vfs_write(fh, data_in, 0, data_size));

    RC_OK(vfs_read(fh, data_out, 0, data_size));

    assert_memory_equal(data_in, data_out, data_size);

    RC_OK(env->cleanup(env));
}

static void test_write_read_by_char(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t sz = 100;
    char buff[sz];
    for (int i = 0; i < sz; i++) {
        buff[0] = (char) i;
        RC_OK(vfs_write(fh, buff, i, 1));
    }
    RC_OK(vfs_read(fh, buff, 0, sz));
    for (int i = 0; i < sz; i++) {
        assert_int_equal((char) i, buff[i]);
    }

    RC_OK(env->cleanup(env));
}

static void test_vfs_write_read_multi_fh(void **state)
{
    test_env env = *state;
    vfs test_vfs = env->test_vfs;

    vfs_fh fh1;
    vfs_fh fh2;

    RC_OK(vfs_open(test_vfs, TCBL_TEST_FILENAME, &fh1));
    assert_non_null(fh1);

    RC_OK(vfs_open(test_vfs, TCBL_TEST_FILENAME, &fh2));
    assert_non_null(fh2);

    size_t sz = 72;
    char data_in_1[sz];
    char data_in_2[sz];
    char data_out[sz];

    prep_data(data_in_1, sz, 577);
    prep_data(data_in_2, sz, 986);

    RC_OK(vfs_write(fh1, data_in_1, 0, sz));
    RC_OK(vfs_write(fh2, data_in_2, sz, sz));

    memset(data_out, 0, sz);
    RC_OK(vfs_read(fh2, data_out, 0, sz));
    assert_memory_equal(data_in_1, data_out, sz);

    RC_OK(vfs_read(fh1, data_out, sz, sz));
    assert_memory_equal(data_in_2, data_out, sz);

    RC_OK(vfs_close(fh2));
    RC_OK(vfs_close(fh1));

    RC_OK(env->cleanup(env));
}

static void test_partial_read(void **state) {
    test_env env = *state;
    vfs_fh fh = env->fh[0];

    size_t sz = TCBL_TEST_PAGE_SIZE / 2;
    char data_in[sz];
    prep_data(data_in, sz, 98437);
    RC_OK(vfs_write(fh, data_in, 0, sz));

    verify_file(fh, data_in, sz);

    size_t sz2 = TCBL_TEST_PAGE_SIZE;
    char data_out[sz2];
    char data_expected[sz2];
    memset(data_expected, 0, sz2);
    memcpy(data_expected, data_in, sz);
    RC_EQ(vfs_read(fh, data_out, 0, sz2), TCBL_BOUNDS_CHECK);
    assert_memory_equal(data_out, data_expected, sz2);

    char data_in_2[sz2];
    prep_data(data_in_2, sz2, 10435);
    vfs_write(fh, data_in_2, 0, sz2);
    verify_file(fh, data_in_2, sz2);

    int rc = vfs_truncate(fh, sz);
    if (rc == TCBL_NOT_IMPLEMENTED) {
        goto exit;
    }
    RC_OK(rc);
    memset(data_expected, 0, sz2);
    memcpy(data_expected, data_in_2, sz);
    RC_EQ(vfs_read(fh, data_out, 0, sz2), TCBL_BOUNDS_CHECK);
    assert_memory_equal(data_out, data_expected, sz2);

    exit:
    RC_OK(env->cleanup(env));
}

static void test_tcbl_underlying_fidelity(void **state)
{
    test_env env = *state;
    vfs_fh fh = env->fh[0];

    size_t sz = TCBL_TEST_PAGE_SIZE / 2;
    char data_in[sz];
    prep_data(data_in, sz, 98437);
    RC_OK(vfs_write(fh, data_in, 0, sz));
    verify_file(fh, data_in, sz);
    RC_OK(vfs_checkpoint(fh));
    verify_file(fh, data_in, sz);

    vfs_fh ufh;
    RC_OK(vfs_open(env->base_vfs, TCBL_TEST_FILENAME, &ufh));
    verify_file(ufh, data_in, sz);
    RC_OK(vfs_close(ufh));

    RC_OK(memvfs_free(env->base_vfs));
}

static void test_write_gap(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t sz = 200;
    size_t gap_sz = 800;
    size_t expected_size = 2 * sz + gap_sz;
    char data_in_1[sz];
    char data_in_2[sz];
    char data_expected[expected_size];
    char data_out[expected_size];

    prep_data(data_in_1, sz, 577);
    prep_data(data_in_2, sz, 986);
    memcpy(data_expected, data_in_1, sz);
    memcpy(&data_expected[sz + gap_sz], data_in_2, sz);
    memset(&data_expected[sz], 0, gap_sz);

    size_t file_size;
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_write(fh, data_in_2, sz + gap_sz, sz));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, expected_size);

    RC_OK(vfs_write(fh, data_in_1, 0, sz));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, expected_size);

    RC_OK(vfs_read(fh, data_out, 0, expected_size));
    assert_memory_equal(data_out, data_expected, expected_size);

    RC_OK(env->cleanup(env));
}

static void test_nothing(void **state)
{
    // noop for setup and teardown testing
    test_env env = *state;
    RC_OK(env->cleanup(env));
}

static void test_begin_end(void **state)
{
    test_env env = *state;
    vfs_fh fh = env->fh[0];
    env->after_change(fh);
    // no change
    env->after_change(fh);
    RC_OK(env->cleanup(env));
}

static void test_vfs_bounds(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t data_len = 5 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    prep_data(data_in, data_len, 964);

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    char data_out[data_len];
    size_t read_offs, read_len;
    read_offs = 7 * TCBL_TEST_PAGE_SIZE / 2;
    read_len = 3 * TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_read(fh, data_out, read_offs, read_len));
    assert_memory_equal(&data_in[read_offs], data_out, read_len);

    read_len += 1;
    RC_EQ(vfs_read(fh, data_out, read_offs, read_len), TCBL_BOUNDS_CHECK);

    RC_OK(env->cleanup(env));
}

static void test_vfs_delete(void **state)
{
    test_env env = *state;
    vfs vfs = env->test_vfs;
    char *file_name = "test_file";
    vfs_fh fh, fh2;

    int exists;
    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_false(exists);

    RC_OK(vfs_open(vfs, file_name, &fh));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_true(exists);

    RC_OK(vfs_close(fh));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_true(exists);

    RC_OK(vfs_delete(vfs, file_name));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_false(exists);

    RC_EQ(vfs_delete(vfs, file_name), TCBL_FILE_NOT_FOUND);

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_false(exists);

    RC_OK(vfs_open(vfs, file_name, &fh));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_true(exists);

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    prep_data(data_in, data_len, 4391);

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    RC_OK(vfs_delete(vfs, file_name));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_false(exists);

    RC_OK(vfs_write(fh, data_in, data_len, data_len));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_false(exists);

    size_t new_file_size = 2 * data_len;
    char data_expected[new_file_size];
    memcpy(data_expected, data_in, data_len);
    memcpy(&data_expected[data_len], data_in, data_len);
    verify_file(fh, data_expected, new_file_size);

    RC_OK(vfs_open(vfs, file_name, &fh2));

    RC_OK(vfs_exists(vfs, file_name, &exists));
    assert_true(exists);

    verify_length(fh2, 0);
    verify_length(fh, new_file_size);

    char data_in_2[data_len];
    prep_data(data_in_2, data_len, 6054);

    RC_OK(vfs_write(fh2, data_in_2, 0, data_len));

    RC_OK(vfs_close(fh2));

    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(vfs, file_name, &fh));

    verify_file(fh, data_in_2, data_len);

    RC_OK(vfs_close(fh));

    RC_OK(env->cleanup(env));
}

static void test_vfs_reopen(void **state)
{
    test_env env = *state;
    vfs test_vfs = env->test_vfs;

    vfs_fh fh;
    char *test_file_name = TCBL_TEST_FILENAME;

    RC_OK(vfs_open(test_vfs, test_file_name, &fh));
    assert_non_null(fh);
    assert_ptr_equal(test_vfs, fh->vfs);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    RC_OK(vfs_write(fh, data_in, 0, data_size));

    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(test_vfs, test_file_name, &fh));
    assert_non_null(fh);

    RC_OK(vfs_read(fh, data_out, 0, data_size));

    assert_memory_equal(data_in, data_out, data_size);

    RC_OK(vfs_close(fh));

    RC_OK(env->cleanup(env));
}

static void test_vfs_two_files(void **state)
{
    test_env env = *state;
    vfs test_vfs = env->test_vfs;

    vfs_fh fh;
    char *test_file_name_1 = "/test_file";
    char *test_file_name_2 = "/another_file";

    RC_OK(vfs_open(test_vfs, test_file_name_1, &fh));
    assert_non_null(fh);
    assert_ptr_equal(test_vfs, fh->vfs);

    size_t data_size = 100;
    char data_in_1[data_size];
    char data_in_2[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in_1[i] = (char) i;
    }

    RC_OK(vfs_write(fh, data_in_1, 0, data_size));

    RC_OK(vfs_close(fh));

    for (int i = 0; i < data_size; i++) {
        data_in_2[i] = (char) (2 * i);
    }

    RC_OK(vfs_open(test_vfs, test_file_name_2, &fh));
    assert_non_null(fh);

    RC_OK(vfs_write(fh, data_in_2, 0, data_size));

    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(test_vfs, test_file_name_1, &fh));
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_size));
    assert_memory_equal(data_in_1, data_out, data_size);
    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(test_vfs, test_file_name_2, &fh));
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_size));
    assert_memory_equal(data_in_2, data_out, data_size);
    RC_OK(vfs_close(fh));

    RC_OK(env->cleanup(env));
}

static void test_truncate(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    int rc = vfs_truncate(fh, 0);
    if (rc == TCBL_NOT_IMPLEMENTED) {
        env->cleanup(env);
        return;
    }
    RC_OK(rc);
    verify_length(fh, 0);

    size_t sz = 100;
    char buff[sz];
    char expected_buff[sz];

    int nwrite = 10;
    size_t pos = 0;
    for (int i = 0; i < nwrite; i++) {
        prep_data(buff, sz, (uint64_t) i * 459);
        RC_OK(vfs_write(fh, buff, pos, sz));
        pos += sz;
    }

    verify_length(fh, nwrite * sz);

    RC_OK(vfs_truncate(fh, 3 * sz));
    verify_length(fh, 3 * sz);
    pos = 0;
    for (int i = 0; i < 3; i++) {
        prep_data(expected_buff, sz, (uint64_t) i * 459);
        verify_data(fh, expected_buff, pos, sz);
        pos += sz;
    }
    RC_NOT_OK(vfs_read(fh, buff, pos, sz));

    RC_OK(vfs_truncate(fh, 5 * sz));
    verify_length(fh, 5 * sz);
    pos = 0;
    for (int i = 0; i < 5; i++) {
        if (i < 3) {
            prep_data(expected_buff, sz, (uint64_t) i * 459);
        } else {
            memset(expected_buff, 0, sz);
        }
        verify_data(fh, expected_buff, pos, sz);
        pos += sz;
    }

    RC_OK(vfs_truncate(fh, 0));
    verify_length(fh, 0);
    for (int i = 0; i < nwrite; i++) {
        RC_NOT_OK(vfs_read(fh, buff, pos, sz));
        pos += sz;
    }

    RC_OK(env->cleanup(env));
}

struct test_page {
    int id;
    int prev_id;
    char data[0];
};

struct paired_update_args {
    test_env env;
    size_t block_size;
    int len_blocks;
    int checkpoint_interval;
    int id;
    unsigned seed;
};

static void *test_paried_updates_changefn(void* args)
{
    struct paired_update_args *a = args;
    test_env env = a->env;
    size_t block_size = a->block_size;
    int len_blocks = a->len_blocks;
    int checkpoint_interval = a->checkpoint_interval;
    int id = a->id;
    unsigned seed = a->seed;

    vfs_fh fh = env->fh[id];

    char buff_1[block_size];
    char buff_2[block_size];
    struct test_page *tp = (struct test_page *) buff_1;
    struct test_page *tp2 = (struct test_page *) buff_2;

    pthread_t self = pthread_self();

    // make some changes
    int update_ct = 0;
    for (int i = 0; i < 25; i++) {
//        usleep(rand_r(&seed) % 100000);
        printf("thread %d (%lu) at iteration %d\n", id, self, i);
        if (env->has_txn) RC_OK(vfs_txn_begin(fh));
        int cb = rand_r(&seed) % (len_blocks - 1);
        size_t pos1 = cb * block_size;
        size_t pos2 = (cb + 1) * block_size;
        vfs_read(fh, buff_1, pos1, block_size);
        vfs_read(fh, buff_2, pos2, block_size);
        tp->id = rand_r(&seed);
        tp2->prev_id = tp->id;
        vfs_write(fh, buff_1, pos1, block_size);
        vfs_write(fh, buff_2, pos2, block_size);
        if (env->has_txn) RC_OK(vfs_txn_commit(fh));
        update_ct += 1;
        if (update_ct % checkpoint_interval == 0) {
            if (env->has_txn) RC_OK(vfs_checkpoint(fh));
        }
    }
    printf("thread %d (%lu) completed\n", id, self);
    return NULL;
}

static void test_paired_updates(void **state)
{
    test_env env = *state;
    vfs_fh fh = env->fh[0];

    // each block must contain a copy of data from
    // the previous block
    size_t block_size = TCBL_TEST_PAGE_SIZE;
    int len_blocks = 10;

    char buff[block_size];
    struct test_page *tp = (struct test_page *) buff;
    unsigned int seed = 23823094;
    size_t data_len = block_size - sizeof(struct test_page);
    int prev_id = 0;
    for (int i = 0; i < len_blocks; i++) {
        tp->id = rand_r(&seed);
        tp->prev_id = prev_id;
        prep_data(tp->data, data_len, (uint64_t) rand_r(&seed));
        RC_OK(vfs_write(fh, buff, i * block_size, block_size));
        prev_id = tp->id;
    }

    // make this multithreaded
    int checkpoint_interval = 13;
    unsigned seed_seq = 123;
    for (int n = 0; n < 10; n++) {
        // verify integrity
        printf("beginning integrity verfication\n");
        prev_id = 0;
        if (env->has_txn) RC_OK(vfs_txn_begin(fh));
        for (int i = 0; i < len_blocks; i++) {
            RC_OK(vfs_read(fh, buff, i * block_size, block_size));
            assert_int_equal(tp->prev_id, prev_id);
            prev_id = tp->id;
        }
        if (env->has_txn) RC_OK(vfs_txn_abort(fh));
        printf("ended integrity verfication\n");

        if (env->num_fh == 1) {
            struct paired_update_args args;
            args.env = env;
            args.block_size = block_size;
            args.len_blocks = len_blocks;
            args.checkpoint_interval = checkpoint_interval;
            args.id = 0;
            args.seed = seed_seq;
            seed_seq += 1;
            test_paried_updates_changefn(&args);
        } else {
            // multithreaded execution
            pthread_t threads[env->num_fh];
            struct paired_update_args args[env->num_fh];
            for (int i = 0; i < env->num_fh; i++) {
                args[i].env = env;
                args[i].block_size = block_size;
                args[i].len_blocks = len_blocks;
                args[i].checkpoint_interval = checkpoint_interval;
                args[i].id = i;
                args[i].seed = seed_seq;
                seed_seq += 1;
                pthread_create(&threads[i], NULL, test_paried_updates_changefn, &args[i]);
            }
            for (int i = 0; i < env->num_fh; i++) {
                int rc = pthread_join(threads[i], NULL);
                if (rc) {
                    printf("error joining thread\n");
                } else {
                    printf("successfully joined thread %d\n", i);
                }
            }
        }
    }

    RC_OK(env->cleanup(env));
}

struct fuzz_update_args {
    vfs_fh fh;
    size_t block_size;
    int len_blocks;
    int checkpoint_interval;
    int id;
    bool has_txn;
    unsigned seed;
};

static int fuzz_updates_initialize(vfs_fh fh, size_t block_size, int len_blocks)
{
    char buff[block_size];
    struct test_page *tp = (struct test_page *) buff;
    unsigned int seed = 23823094;
    size_t data_len = block_size - sizeof(struct test_page);
    int prev_id = 0;
    for (int i = 0; i < len_blocks; i++) {
        tp->id = rand_r(&seed);
        tp->prev_id = prev_id;
        prep_data(tp->data, data_len, (uint64_t) rand_r(&seed));
        RC_OK(vfs_write(fh, buff, i * block_size, block_size));
        prev_id = tp->id;
    }
    return TCBL_OK;
}

static int fuzz_updates_verify(vfs_fh fh, bool has_txn, size_t block_size, int len_blocks)
{
    char buff[block_size];
    struct test_page *tp = (struct test_page *) buff;
    unsigned int seed = 23823094;
    size_t data_len = block_size - sizeof(struct test_page);
    int prev_id = 0;
    if (has_txn) RC_OK(vfs_txn_begin(fh));
    for (int i = 0; i < len_blocks; i++) {
        RC_OK(vfs_read(fh, buff, i * block_size, block_size));
        assert_int_equal(tp->prev_id, prev_id);
        prev_id = tp->id;
    }
    if (has_txn) RC_OK(vfs_txn_abort(fh));
    return TCBL_OK;
}

static void *fuzz_updates_changefn(void* args)
{
    struct fuzz_update_args *a = args;
    size_t block_size = a->block_size;
    int len_blocks = a->len_blocks;
    int checkpoint_interval = a->checkpoint_interval;
    int id = a->id;
    unsigned seed = a->seed;
    bool has_txn = a->has_txn;

    vfs_fh fh = a->fh;

    char buff_1[block_size];
    char buff_2[block_size];
    struct test_page *tp = (struct test_page *) buff_1;
    struct test_page *tp2 = (struct test_page *) buff_2;

    pthread_t self = pthread_self();

    // make some changes
    int update_ct = 0;
    for (int i = 0; i < 25; i++) {
//        usleep(rand_r(&seed) % 100000);
        printf("thread %d (%lu) at iteration %d\n", id, self, i);
        if (has_txn) RC_OK(vfs_txn_begin(fh));
        int cb = rand_r(&seed) % (len_blocks - 1);
        size_t pos1 = cb * block_size;
        size_t pos2 = (cb + 1) * block_size;
        vfs_read(fh, buff_1, pos1, block_size);
        vfs_read(fh, buff_2, pos2, block_size);
        tp->id = rand_r(&seed);
        tp2->prev_id = tp->id;
        vfs_write(fh, buff_1, pos1, block_size);
        vfs_write(fh, buff_2, pos2, block_size);
        if (has_txn) RC_OK(vfs_txn_commit(fh));
        update_ct += 1;
        if (update_ct % checkpoint_interval == 0) {
            if (has_txn) RC_OK(vfs_checkpoint(fh));
        }
    }
    printf("thread %d (%lu) completed\n", id, self);
    return NULL;
}

static void fuzz_updates_direct(void **state) {

    // begin configuration
    bool shared_memvfs = true;
    bool memvfs_only = false;
    int num_testers = 2;
    // end configuration

    int tester_offset = shared_memvfs ? 1 : 0;
    int num_fh = num_testers + tester_offset;

    int num_memvfs = shared_memvfs ? 1 : num_testers;
    vfs memvfs[num_memvfs];
    vfs tcbl[num_fh];
    vfs_fh fh[num_fh];
    int start_vfh = tester_offset;
    int end_vfh = num_testers;
    bool has_txn = !memvfs_only;
    bool verify = !memvfs_only;
    if (shared_memvfs) {
        RC_OK(memvfs_allocate(&memvfs[0]));
        assert_non_null(memvfs[0]);
    }
    for (int i = 0; i < num_fh; i++) {
        vfs fh_memvfs;
        if (shared_memvfs) {
            fh_memvfs = memvfs[0];
        } else {
            RC_OK(memvfs_allocate(&memvfs[i]));
            assert_non_null(memvfs[i]);
            fh_memvfs = memvfs[i];
        }
        vfs fh_vfs;
        if (memvfs_only) {
            fh_vfs = fh_memvfs;
            tcbl[i] = NULL;
        } else {
            RC_OK(tcbl_allocate(&tcbl[i], fh_memvfs, TCBL_TEST_PAGE_SIZE));
            fh_vfs = tcbl[i];
        }
        RC_OK(vfs_open(fh_vfs, TCBL_TEST_FILENAME, &fh[i]));
    }


    // each block must contain a copy of data from
    // the previous block
    size_t block_size = TCBL_TEST_PAGE_SIZE;
    int len_blocks = 10;

    for (int i = start_vfh; i < end_vfh; i++) {
        RC_OK(fuzz_updates_initialize(fh[i], block_size, len_blocks));
    }

    // make this multithreaded
    int checkpoint_interval = 13;
    unsigned seed_seq = 123;
    for (int n = 0; n < 100; n++) {
        // verify integrity
        if (verify) {
            for (int i = start_vfh; i < end_vfh; i++) {
                RC_OK(fuzz_updates_verify(fh[i], has_txn, block_size, len_blocks));
            }
        }

        if (false && num_testers == 1) {
            struct fuzz_update_args args;
            args.fh = fh[1];
            args.block_size = block_size;
            args.len_blocks = len_blocks;
            args.checkpoint_interval = checkpoint_interval;
            args.id = 0;
            args.seed = seed_seq;
            args.has_txn = has_txn;
            seed_seq += 1;
            fuzz_updates_changefn(&args);
        } else {
            // multithreaded execution
            pthread_t threads[num_testers];
            struct fuzz_update_args args[num_fh];
            for (int i = 0; i < num_testers; i++) {
                args[i].fh = fh[i + tester_offset];
                args[i].block_size = block_size;
                args[i].len_blocks = len_blocks;
                args[i].checkpoint_interval = checkpoint_interval;
                args[i].id = i;
                args[i].seed = seed_seq;
                args[i].has_txn = has_txn;
                seed_seq += 1;
                pthread_create(&threads[i], NULL, fuzz_updates_changefn, &args[i]);
            }
            for (int i = 0; i < num_testers; i++) {
                int rc = pthread_join(threads[i], NULL);
                if (rc) {
                    printf("error joining thread\n");
                } else {
                    printf("successfully joined thread %d\n", i);
                }
            }
        }
    }

    for (int i = 0; i < num_fh; i++) {
        if (tcbl[i]) {
            RC_OK(vfs_free((vfs) tcbl[i]));
        }
    }
    for (int i = 0; i < num_memvfs; i++) {
        RC_OK(vfs_free(memvfs[i]));
    }
}

static void test_tcbl_open_close(void **state)
{
    vfs memvfs;
    vfs tcbl;
    tcbl_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(tcbl_allocate((tvfs*) &tcbl, memvfs, TCBL_TEST_PAGE_SIZE));

    RC_OK(vfs_open(tcbl, "test-file", (vfs_fh *) &fh));
    assert_ptr_equal(tcbl, fh->vfs);

    RC_OK(vfs_close((vfs_fh) fh));
    RC_OK(vfs_free(tcbl));
    RC_OK(vfs_free(memvfs));
}

static void test_tcbl_write_read(void **state)
{
    vfs memvfs;
    vfs tcbl;
    tcbl_fh fh;

    RC_OK(memvfs_allocate(&memvfs));

    RC_OK(tcbl_allocate((tvfs*) &tcbl, memvfs, TCBL_TEST_PAGE_SIZE));

    RC_OK(vfs_open(tcbl, "test-file", (vfs_fh *) &fh));

    size_t data_size = TCBL_TEST_PAGE_SIZE;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    RC_OK(vfs_write((vfs_fh) fh, data_in, 0, data_size));

    RC_OK(vfs_read((vfs_fh) fh, data_out, 0, data_size));

    assert_memory_equal(data_in, data_out, data_size);

    RC_OK(vfs_close((vfs_fh) fh));

    RC_OK(vfs_free(tcbl));

    RC_OK(vfs_free(memvfs));
}

static int tcbl_setup(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->base_vfs));
    assert_non_null(env->base_vfs);
    RC_OK(tcbl_allocate((tvfs*) &env->test_vfs, (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));

    *state = env;
    return 0;
}

static int tcbl_teardown(void **state)
{
    test_env env = *state;
    RC_OK(vfs_free((vfs) env->test_vfs));
    RC_OK(vfs_free((vfs) env->base_vfs));
    tcbl_free(NULL, env, sizeof(struct test_env));

    return 0;
}

static int tcbl_setup_1fh(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->base_vfs));
    assert_non_null(env->base_vfs);
    RC_OK(tcbl_allocate((tvfs*) &env->test_vfs, (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));


    char *test_filename = TCBL_TEST_FILENAME;
    RC_OK(vfs_open((vfs) env->test_vfs, test_filename, &env->fh[0]));

    *state = env;
    return 0;
}

static int tcbl_teardown_1fh(void **state)
{
    test_env env = *state;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_free((vfs) env->test_vfs));
    RC_OK(vfs_free((vfs) env->base_vfs));
    tcbl_free(NULL, env, sizeof(struct test_env + sizeof(vfs_fh)));
    return 0;
}

static int tcbl_setup_2fh_1vfs(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + 2 * sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->base_vfs));
    assert_non_null(env->base_vfs);
    RC_OK(tcbl_allocate((tvfs*) &env->test_vfs, (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));


    char *test_filename = TCBL_TEST_FILENAME;
    RC_OK(vfs_open((vfs) env->test_vfs, test_filename, &env->fh[0]));
    RC_OK(vfs_open((vfs) env->test_vfs, test_filename, &env->fh[1]));

    *state = env;
    return 0;
}

static int tcbl_teardown_2fh_1vfs(void **state)
{
    test_env env = *state;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_close(env->fh[1]));
    RC_OK(vfs_free((vfs) env->test_vfs));
    RC_OK(vfs_free((vfs) env->base_vfs));
    tcbl_free(NULL, env, sizeof(struct test_env) + 2 * sizeof(vfs_fh));
    return 0;
}

static int tcbl_setup_2fh_2vfs(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + 2 * sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->base_vfs));
    assert_non_null(env->base_vfs);
    tvfs vfs1, vfs2;
    RC_OK(tcbl_allocate(&vfs1, (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));
    RC_OK(tcbl_allocate(&vfs2, (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));

    char *test_filename = TCBL_TEST_FILENAME;
    RC_OK(vfs_open((vfs) vfs1, test_filename, &env->fh[0]));
    RC_OK(vfs_open((vfs) vfs2, test_filename, &env->fh[1]));

    env->test_vfs = NULL;
    *state = env;
    return 0;
}

static int tcbl_teardown_2fh_2vfs(void **state)
{
    test_env env = *state;
    vfs vfs1 = env->fh[0]->vfs;
    vfs vfs2 = env->fh[1]->vfs;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_close(env->fh[1]));
    RC_OK(vfs_free(vfs1));
    RC_OK(vfs_free(vfs2));
    RC_OK(vfs_free((vfs) env->base_vfs));
    tcbl_free(NULL, env, sizeof(struct test_env) + 2 * sizeof(vfs_fh));
    return 0;
}

static void test_tcbl_txn_nothing_commit(void **state)
{
    test_env env = *state;
    vfs tcbl = env->test_vfs;
    vfs_fh fh;
    char *test_filename = TCBL_TEST_FILENAME;

    RC_OK(vfs_open(tcbl, test_filename, &fh));
    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_txn_commit(fh));
    RC_OK(vfs_close(fh));
    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_nothing_abort(void **state)
{
    test_env env = *state;
    vfs tcbl = env->test_vfs;
    vfs_fh fh;
    char *test_filename = TCBL_TEST_FILENAME;

    RC_OK(vfs_open(tcbl, test_filename, &fh));
    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_txn_abort(fh));
    RC_OK(vfs_close(fh));
    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_write_read_commit(void **state)
{
    test_env env = *state;
    char *test_filename = TCBL_TEST_FILENAME;

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);

    vfs_fh fh;
    RC_OK(vfs_open(env->test_vfs, test_filename, &fh));

    RC_OK(vfs_txn_begin(fh));

    size_t file_size;
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_txn_commit(fh));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_write_read_abort(void **state)
{
    test_env env = *state;

    char *test_filename = TCBL_TEST_FILENAME;

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open(env->test_vfs, test_filename, &fh));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_txn_begin(fh));

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_txn_abort(fh));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_multiblock(void **state)
{
    test_env env = *state;
    vfs tcbl = env->test_vfs;

    char *test_filename = TCBL_TEST_FILENAME;

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open(tcbl, test_filename, &fh));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_txn_begin(fh));

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_txn_commit(fh));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_reopen(void **state)
{
    test_env env = *state;
    vfs tcbl = env->test_vfs;

    char *test_filename = TCBL_TEST_FILENAME;

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open(tcbl, test_filename, &fh));
    RC_OK(vfs_write(fh, data_in, 0, data_len));
    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(tcbl, test_filename, &fh));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_overwrite(void **state)
{
    test_env env = *state;
    vfs tcbl = env->test_vfs;

    char *test_filename = TCBL_TEST_FILENAME;

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open(tcbl, test_filename, &fh));
    RC_OK(vfs_write(fh, data_in, 0, data_len));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);


    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_write(fh, data_in, TCBL_TEST_PAGE_SIZE, TCBL_TEST_PAGE_SIZE));
    RC_OK(vfs_txn_abort(fh));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_write(fh, data_in, TCBL_TEST_PAGE_SIZE, TCBL_TEST_PAGE_SIZE));
    RC_OK(vfs_txn_commit(fh));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, TCBL_TEST_PAGE_SIZE);
    assert_memory_equal(data_in, &data_out[TCBL_TEST_PAGE_SIZE], TCBL_TEST_PAGE_SIZE);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

void test_read_unaligned(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t data_len = 10 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    prep_data(data_in, data_len, 378);

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    char data_out[data_len];

    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_read(fh, data_out, 0, 1));
    assert_memory_equal(data_in, data_out, 1);

    RC_OK(vfs_read(fh, data_out, 1, 1));
    assert_memory_equal(&data_in[1], data_out, 1);

    RC_OK(vfs_read(fh, data_out, 10, 1));
    assert_memory_equal(&data_in[10], data_out, 1);

    size_t rs = TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_read(fh, data_out, 0, rs));
    assert_memory_equal(data_in, data_out, rs);

    RC_OK(vfs_read(fh, data_out, 1, rs));
    assert_memory_equal(&data_in[1], data_out, rs);

    RC_OK(vfs_read(fh, data_out, 10, rs));
    assert_memory_equal(&data_in[10], data_out, rs);

    RC_OK(vfs_read(fh, data_out, rs, rs));
    assert_memory_equal(&data_in[rs], data_out, rs);

    RC_OK(vfs_read(fh, data_out, TCBL_TEST_PAGE_SIZE - 1, rs));
    assert_memory_equal(&data_in[TCBL_TEST_PAGE_SIZE - 1], data_out, rs);

    rs = 3 * TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_read(fh, data_out, 0, rs));
    assert_memory_equal(data_in, data_out, rs);

    RC_OK(vfs_read(fh, data_out, 1, rs));
    assert_memory_equal(&data_in[1], data_out, rs);

    RC_OK(vfs_read(fh, data_out, 10, rs));
    assert_memory_equal(&data_in[10], data_out, rs);

    RC_OK(vfs_read(fh, data_out, rs, rs));
    assert_memory_equal(&data_in[rs], data_out, rs);

    RC_OK(vfs_read(fh, data_out, TCBL_TEST_PAGE_SIZE - 1, rs));
    assert_memory_equal(&data_in[TCBL_TEST_PAGE_SIZE - 1], data_out, rs);

    rs = 7 * TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_read(fh, data_out, 0, rs));
    assert_memory_equal(data_in, data_out, rs);

    RC_OK(vfs_read(fh, data_out, 1, rs));
    assert_memory_equal(&data_in[1], data_out, rs);

    RC_OK(vfs_read(fh, data_out, 10, rs));
    assert_memory_equal(&data_in[10], data_out, rs);

    RC_OK(vfs_read(fh, data_out, rs, rs));
    assert_memory_equal(&data_in[rs], data_out, rs);

    RC_OK(vfs_read(fh, data_out, TCBL_TEST_PAGE_SIZE - 1, rs));
    assert_memory_equal(&data_in[TCBL_TEST_PAGE_SIZE - 1], data_out, rs);

    RC_OK(env->cleanup(env));
}

void test_write_unaligned(void **state)
{
    test_env env = *state;
    struct change_fh cfh;
    create_change_fh(env, &cfh);
    vfs_fh fh = (vfs_fh) &cfh;

    size_t sz = TCBL_TEST_PAGE_SIZE;
    size_t data_len = 10 * sz;
    char data_in[data_len];
    char data_expected[data_len];
    prep_data(data_in, data_len, 379);
    memcpy(data_expected, data_in, data_len);

    RC_OK(vfs_write(fh, data_in, 0, data_len - sz - 1));
    verify_file(fh, data_expected, data_len - sz - 1);

    RC_OK(vfs_write(fh, &data_in[data_len - sz + 1], data_len - sz + 1, sz - 2));
    memset(&data_expected[data_len - sz - 1], 0, 2);
    verify_file(fh, data_expected, data_len - 1);

    RC_OK(vfs_write(fh, &data_in[data_len - sz - 1], data_len - sz - 1, 2));
    memcpy(&data_expected[data_len - sz - 1], &data_in[data_len - sz - 1], 2);
    verify_file(fh, data_expected, data_len - 1);

    RC_OK(vfs_write(fh, &data_in[data_len - 1], data_len - 1, 1));
    verify_file(fh, data_expected, data_len);

    char data_in_2[data_len];
    prep_data(data_in_2, data_len, 6098);
    assert_memory_not_equal(data_in_2, data_in, data_len);

    RC_OK(vfs_write(fh, data_in_2, 0, 1));
    memcpy(&data_expected[0], &data_in_2[0], 1);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[1], 1, 1));
    memcpy(&data_expected[1], &data_in_2[1], 1);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[10], 10, 1));
    memcpy(&data_expected[10], &data_in_2[10], 1);
    verify_file(fh, data_expected, data_len);

    size_t rs = TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_write(fh, &data_in_2[0], 0, rs));
    memcpy(&data_expected[0], &data_in_2[0], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[1], 1, rs));
    memcpy(&data_expected[1], &data_in_2[1], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[10], 10, rs));
    memcpy(&data_expected[10], &data_in_2[10], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[rs], rs, rs));
    memcpy(&data_expected[rs], &data_in_2[rs], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[TCBL_TEST_PAGE_SIZE - 1], TCBL_TEST_PAGE_SIZE - 1, rs));
    memcpy(&data_expected[TCBL_TEST_PAGE_SIZE - 1], &data_in_2[TCBL_TEST_PAGE_SIZE - 1], rs);
    verify_file(fh, data_expected, data_len);

    rs = 3 * TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_write(fh, &data_in_2[0], 0, rs));
    memcpy(&data_expected[0], &data_in_2[0], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[1], 1, rs));
    memcpy(&data_expected[1], &data_in_2[1], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[10], 10, rs));
    memcpy(&data_expected[10], &data_in_2[10], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[rs], rs, rs));
    memcpy(&data_expected[rs], &data_in_2[rs], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[TCBL_TEST_PAGE_SIZE - 1], TCBL_TEST_PAGE_SIZE - 1, rs));
    memcpy(&data_expected[TCBL_TEST_PAGE_SIZE - 1], &data_in_2[TCBL_TEST_PAGE_SIZE - 1], rs);
    verify_file(fh, data_expected, data_len);

    rs = 7 * TCBL_TEST_PAGE_SIZE / 2;
    RC_OK(vfs_write(fh, &data_in_2[0], 0, rs));
    memcpy(&data_expected[0], &data_in_2[0], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[1], 1, rs));
    memcpy(&data_expected[1], &data_in_2[1], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[10], 10, rs));
    memcpy(&data_expected[10], &data_in_2[10], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[rs], rs, rs));
    memcpy(&data_expected[rs], &data_in_2[rs], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(vfs_write(fh, &data_in_2[TCBL_TEST_PAGE_SIZE - 1], TCBL_TEST_PAGE_SIZE - 1, rs));
    memcpy(&data_expected[TCBL_TEST_PAGE_SIZE - 1], &data_in_2[TCBL_TEST_PAGE_SIZE - 1], rs);
    verify_file(fh, data_expected, data_len);

    RC_OK(env->cleanup(env));
}

static void test_tcbl_txn_2fh_snapshot(void **state)
{
    test_env env = *state;
    vfs_fh fh1 = env->fh[0];
    vfs_fh fh2 = env->fh[1];

    size_t data_len = 5 * TCBL_TEST_PAGE_SIZE;
    char data_in_1[data_len];
    char data_in_2[data_len];
    prep_data(data_in_1, data_len, 98345);
    prep_data(data_in_2, data_len, 1212);
    assert_memory_not_equal(data_in_1, data_in_2, data_len);

    RC_OK(vfs_txn_begin(fh2));
    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_write(fh1, data_in_1, 0, data_len));

    verify_length(fh1, data_len);
    verify_data(fh1, data_in_1, 0, data_len);

    verify_length(fh2, 0);

    RC_OK(vfs_txn_commit(fh1));

    verify_length(fh2, 0);

    RC_OK(vfs_txn_commit(fh2));

    verify_length(fh2, data_len);
    verify_data(fh2, data_in_1, 0, data_len);

    // Test that reader sees old data during txn, also after abort
    // sees new data after writer commits.
    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_txn_begin(fh2));

    size_t data_offset_2 = 3 * TCBL_TEST_PAGE_SIZE;
    size_t len_expected_2 = 8 * TCBL_TEST_PAGE_SIZE;
    char data_expected_2[len_expected_2];
    memcpy(data_expected_2, data_in_1, data_len);
    memcpy(&data_expected_2[data_offset_2], data_in_2, data_len);

    RC_OK(vfs_write(fh2, data_in_2, data_offset_2, data_len));
    verify_length(fh2, len_expected_2);
    verify_data(fh2, data_expected_2, 0, len_expected_2);

    verify_length(fh1, data_len);
    verify_data(fh1, data_in_1, 0, data_len);

    RC_OK(vfs_txn_abort(fh1));

    verify_length(fh1, data_len);
    verify_data(fh1, data_in_1, 0, data_len);

    RC_OK(vfs_txn_commit(fh2));

    verify_length(fh1, len_expected_2);
    verify_data(fh1, data_expected_2, 0, len_expected_2);

    // Test that reader sees old data during txn, also after
    // writer commits, new data after abort.
    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_txn_begin(fh2));

    size_t data_len_3 = 12 * TCBL_TEST_PAGE_SIZE;
    char data_in_3[data_len_3];
    prep_data(data_in_3, data_len_3, 8564);

    vfs_write(fh1, data_in_3, 0, data_len_3);

    verify_length(fh1, data_len_3);
    verify_data(fh1, data_in_3, 0, data_len_3);
    verify_length(fh2, len_expected_2);
    verify_data(fh2, data_expected_2, 0, len_expected_2);

    RC_OK(vfs_txn_commit(fh1));

    verify_length(fh1, data_len_3);
    verify_data(fh1, data_in_3, 0, data_len_3);
    verify_length(fh2, len_expected_2);
    verify_data(fh2, data_expected_2, 0, len_expected_2);

    RC_OK(vfs_txn_commit(fh2));

    verify_length(fh1, data_len_3);
    verify_data(fh1, data_in_3, 0, data_len_3);
    verify_length(fh2, data_len_3);
    verify_data(fh2, data_in_3, 0, data_len_3);

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_2fh_overwrite(void **state)
{
    test_env env = *state;
    vfs_fh fh1 = env->fh[0];
    vfs_fh fh2 = env->fh[1];

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in_1[data_len];
    char data_in_2[data_len];
    char data_out[data_len];
    prep_data(data_in_1, data_len, 98345);
    prep_data(data_in_2, data_len, 1212);
    assert_memory_not_equal(data_in_1, data_in_2, data_len);
    size_t file_size;

    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_write(fh1, data_in_1, 0, data_len));
    RC_OK(vfs_file_size(fh1, &file_size));
    assert_int_equal(file_size, data_len);

    RC_OK(vfs_file_size(fh2, &file_size));
    assert_int_equal(file_size, 0);

    RC_OK(vfs_txn_commit(fh1));

    RC_OK(vfs_file_size(fh2, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh2, data_out, 0, data_len));
    assert_memory_equal(data_in_1, data_out, data_len);

    RC_OK(vfs_txn_begin(fh2));
    RC_OK(vfs_write(fh1, data_in_2, 0, data_len));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh2, data_out, 0, data_len));
    assert_memory_equal(data_in_1, data_out, data_len);

    RC_OK(vfs_txn_abort(fh2));

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh2, data_out, 0, data_len));
    assert_memory_equal(data_in_2, data_out, data_len);

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_2fh_conflict(void **state)
{
    test_env env = *state;
    vfs_fh fh1 = env->fh[0];
    vfs_fh fh2 = env->fh[1];

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in_1[data_len];
    char data_in_2[data_len];
    char data_out[data_len];
    prep_data(data_in_1, data_len, 98345);
    prep_data(data_in_2, data_len, 1212);
    assert_memory_not_equal(data_in_1, data_in_2, data_len);
    size_t file_size;

    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_write(fh1, data_in_1, 0, data_len));
    RC_OK(vfs_file_size(fh1, &file_size));
    assert_int_equal(file_size, data_len);

    RC_OK(vfs_txn_begin(fh2));
    RC_OK(vfs_write(fh2, data_in_2, 0, data_len));
    RC_OK(vfs_txn_commit(fh2));

    RC_NOT_OK(vfs_txn_commit(fh1));

    RC_OK(vfs_file_size(fh2, &file_size));
    assert_int_equal(file_size, data_len);

    RC_OK(vfs_file_size(fh1, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh1, data_out, 0, data_len));
    assert_memory_equal(data_in_2, data_out, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh2, data_out, 0, data_len));
    assert_memory_equal(data_in_2, data_out, data_len);

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_checkpoint(void **state)
{
    test_env env = *state;
    vfs_fh fh = env->fh[0];

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    size_t file_size;
    char data_in[data_len];
    prep_data(data_in, data_len, 98345);

    RC_OK(vfs_write(fh, data_in, 0, data_len));

    RC_OK(vfs_file_size(fh, &file_size));

    verify_file(fh, data_in, data_len);

    RC_OK(vfs_checkpoint(fh));

    verify_file(fh, data_in, data_len);

    // add something to the file now
    RC_OK(vfs_write(fh, data_in, data_len, data_len));
    size_t expected_len_2 = 2 * data_len;
    char data_expected_2[expected_len_2];
    memcpy(data_expected_2, data_in, data_len);
    memcpy(&data_expected_2[data_len], data_in, data_len);

    verify_file(fh, data_expected_2, expected_len_2);

    RC_OK(vfs_checkpoint(fh));

    verify_file(fh, data_expected_2, expected_len_2);

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_checkpoint_activity(void **state)
{
    test_env env = *state;
    vfs_fh fh = env->fh[0];

    RC_OK(vfs_txn_begin(fh));
    // don't allow checkpoints on fh with txn open
    RC_NOT_OK(vfs_checkpoint(fh));

    size_t data_len = 10 * TCBL_TEST_PAGE_SIZE;
    size_t file_size;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);

    RC_OK(vfs_write(fh, data_in, 0, data_len));
    vfs_file_size(fh, &file_size);
    assert_int_equal(file_size, data_len);
    memset(data_out, 0, data_len);
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);
    RC_OK(vfs_txn_commit(fh));

    RC_OK(vfs_checkpoint(fh));
    vfs_file_size(fh, &file_size);
    assert_int_equal(file_size, data_len);
    memset(data_out, 0, data_len);
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static void test_tcbl_txn_checkpoint_activity_2(void **state)
{
    test_env env = *state;
    vfs_fh fh1 = env->fh[0];
    vfs_fh fh2 = env->fh[1];

    size_t data_len = 10 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    prep_data(data_in, data_len, 98345);

    // TODO a version of this that starts empty rather than first write some data as here
    RC_OK(vfs_write(fh1, data_in, 0, data_len));
    verify_file(fh1, data_in, data_len);
    RC_OK(vfs_checkpoint(fh1));
    verify_file(fh1, data_in, data_len);
    verify_file(fh2, data_in, data_len);

    RC_OK(vfs_txn_begin(fh2));
    verify_file(fh2, data_in, data_len);

    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_write(fh1, data_in, data_len, data_len));

    size_t expected_len_2 = 2 * data_len;
    char expected_data_2[expected_len_2];
    memcpy(expected_data_2, data_in, data_len);
    memcpy(&expected_data_2[data_len], data_in, data_len);

    verify_file(fh1, expected_data_2, expected_len_2);
    verify_file(fh2, data_in, data_len);

    RC_OK(vfs_txn_commit(fh1));

    verify_file(fh1, expected_data_2, expected_len_2);
    verify_file(fh2, data_in, data_len);

    RC_OK(vfs_checkpoint(fh1));

    verify_file(fh1, expected_data_2, expected_len_2);

    // confirm errors on fh2 operations
    size_t file_size;
    char buff[data_len];
    RC_EQ(vfs_file_size(fh2, &file_size), TCBL_SNAPSHOT_EXPIRED);
    RC_EQ(vfs_read(fh2, buff, 0, data_len), TCBL_SNAPSHOT_EXPIRED);

    RC_OK(vfs_txn_abort(fh2));

    verify_file(fh1, expected_data_2, expected_len_2);
    verify_file(fh2, expected_data_2, expected_len_2);

    // After a checkpoint (also without one) second transaction
    // should fail on commit, but not on write.
    RC_OK(vfs_txn_begin(fh1));
    RC_OK(vfs_txn_begin(fh2));

    RC_OK(vfs_write(fh1, data_in, 2 * data_len, data_len));
    RC_OK(vfs_txn_commit(fh1));
    RC_OK(vfs_checkpoint(fh1));

    // Writes cause a snapshot expired error because we need to read
    // to update the file size. Could perhaps get rid of this but then
    // computing file size requires scanning the log.
    RC_EQ(vfs_write(fh2, data_in, 0, data_len), TCBL_SNAPSHOT_EXPIRED);
    RC_OK(vfs_txn_abort(fh2));

    RC_OK(memvfs_free((vfs) env->base_vfs));
}

static int prepare_test_dir(const char *test_dir) {
    int rc;
    struct stat s;
    if (stat(test_dir, &s) != 0) {
        rc = mkdir(test_dir, 0777);
        if (rc) {
            printf("problem creating test directory %s\n", strerror(errno));
            return TCBL_IO_ERROR;
        }
    }

    // Remove any files starting with the test file prefix
    // from the test directory
    DIR *d = opendir(test_dir);
    if (!d) {
        return TCBL_FILE_NOT_FOUND;
    }
    struct dirent *next_file;
    char filepath[4096];

    char *test_file_prefix = "test";
    while ((next_file = readdir(d)) != NULL) {
        if (strncmp(test_file_prefix, next_file->d_name, strlen(test_file_prefix)) == 0) {
            sprintf(filepath, "%s/%s", test_dir, next_file->d_name);
            rc = remove(filepath);
            if (rc) {
                return TCBL_IO_ERROR;
            }
        }
    }
    closedir(d);
    return TCBL_OK;
}

static int generic_setup_0fh(void **state) {
    test_env env = *state;
    switch (env->test_mode) {
        case MemVfs:
            assert_int_equal(env->num_test_vfs, 1);
            RC_OK(memvfs_allocate((vfs *) &env->base_vfs));
            assert_non_null(env->base_vfs);
            env->test_vfs = env->base_vfs;
            env->all_test_vfs[0] = env->test_vfs;
            env->all_test_vfs[1] = NULL;
            env->has_txn = false;
            break;
        case TcblVfs:
            // TESTING MODE

            for (int i = 0; i < env->num_test_vfs; i++) {
                RC_OK(memvfs_allocate((vfs *) &env->base_vfs));
                assert_non_null(env->base_vfs);
                RC_OK(tcbl_allocate((tvfs *) &env->all_test_vfs[i], (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));
            }
            // END TESTING MODE
            /*
            RC_OK(memvfs_allocate((vfs *) &env->base_vfs));
            assert_non_null(env->base_vfs);
            for (int i = 0; i < env->num_test_vfs; i++) {
                RC_OK(tcbl_allocate((tvfs *) &env->all_test_vfs[i], (vfs) env->base_vfs, TCBL_TEST_PAGE_SIZE));
            }
            */
            env->test_vfs = env->all_test_vfs[0];
            env->has_txn = true;
            break;
        case UnixVfs:
            assert_int_equal(env->num_test_vfs, 1);
            RC_OK(prepare_test_dir(TCBL_UNIX_TEST_DIR));
            RC_OK(unix_vfs_allocate(&env->base_vfs, TCBL_UNIX_TEST_DIR));
            env->test_vfs = env->base_vfs;
            env->all_test_vfs[0] = env->test_vfs;
            env->all_test_vfs[1] = NULL;
            env->has_txn = false;
            break;
        default:
            return 1;
    }
    env->num_fh = 0;

    *state = env;
    return 0;
}

static int generic_setup_1fh(void **state)
{
    int rc = generic_setup_0fh(state);
    if (rc) {
        return rc;
    }

    test_env env = *state;
    RC_OK(vfs_open((vfs) env->test_vfs, TCBL_TEST_FILENAME, &env->fh[0]));
    env->num_fh = 1;
    return 0;
}

static int generic_setup_2fh(void **state)
{
    int rc = generic_setup_0fh(state);
    if (rc) {
        return rc;
    }

    test_env env = *state;
    for (int i = 0; i < 2; i++) {
        RC_OK(vfs_open((vfs) env->all_test_vfs[i % env->num_test_vfs],
                       TCBL_TEST_FILENAME, &env->fh[i]));
    }
    env->num_fh = 2;
    return 0;
}

static int g_noop(vfs_fh fh) {
    return TCBL_OK;
}

static int g_begin_txn(vfs_fh fh) {
    return vfs_txn_begin(fh);
}

static int g_begin_txn_repeatok(vfs_fh fh) {
    int rc = vfs_txn_begin(fh);
    if (rc == TCBL_TXN_ACTIVE) {
        return TCBL_OK;
    }
    return rc;
}

static int g_commit_txn(vfs_fh fh) {
    return vfs_txn_commit(fh);
}

static int g_abort_txn(vfs_fh fh) {
    int rc = vfs_txn_abort(fh);
    if (rc == TCBL_NO_TXN_ACTIVE) {
        return TCBL_OK;
    }
    return rc;
}

static int g_end_txn_checkpoint(vfs_fh fh) {
    int rc = vfs_txn_commit(fh);
    if (rc) {
        return rc;
    }
    return vfs_checkpoint(fh);
}

static int g_cleanup(test_env env)
{
    for (int i = 0; i < env->num_fh; i++) {
        int rc = env->cleanup_change(env->fh[i]);
        if (rc) {
            return rc;
        }
    }
    switch (env->test_mode) {
        case MemVfs:
        case TcblVfs:
            return memvfs_free(env->base_vfs);
        case UnixVfs:
            return unix_vfs_free(env->base_vfs);
    }
    return TCBL_OK;
}

static int generic_teardown(void **state)
{
    test_env env = *state;
    for (int i = 0; i < env->num_fh; i++) {
        RC_OK(vfs_close(env->fh[i]));
    }
    assert_ptr_equal(env->test_vfs, env->all_test_vfs[0]);
    for (int i = 0; i < env->num_test_vfs; i++) {
        RC_OK(vfs_free((vfs) env->all_test_vfs[i]));
    }
    return 0;
}

static int generic_pre_group_memvfs(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = MemVfs;
    env->num_test_vfs = 1;
    env->before_change = g_noop;
    env->after_change = g_noop;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_unixvfs(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = UnixVfs;
    env->num_test_vfs = 1;
    env->before_change = g_noop;
    env->after_change = g_noop;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_tcbl_txn(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = TcblVfs;
    env->num_test_vfs = 1;
    env->before_change = g_begin_txn_repeatok;
    env->after_change = g_noop;
    env->cleanup_change = g_abort_txn;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_tcbl_commit(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = TcblVfs;
    env->num_test_vfs = 1;
    env->before_change = g_begin_txn;
    env->after_change = g_commit_txn;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_tcbl_commit_concurrent(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = TcblVfs;
    env->num_test_vfs = 2;
    // TODO create a slow underlying vfs shim
    env->before_change = g_begin_txn;
    env->after_change = g_commit_txn;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_tcbl_autocommit(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = TcblVfs;
    env->num_test_vfs = 1;
    env->before_change = g_noop;
    env->after_change = g_noop;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_pre_group_tcbl_checkpoint(void **state)
{
    test_env env = tcbl_malloc(NULL, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    *state = env;
    assert_non_null(env);
    env->test_mode = TcblVfs;
    env->num_test_vfs = 1;
    env->before_change = g_begin_txn;
    env->after_change = g_end_txn_checkpoint;
    env->cleanup_change = g_noop;
    env->cleanup = g_cleanup;
    return 0;
}

static int generic_post_group(void **state)
{
    test_env env = *state;
    tcbl_free(NULL, env, sizeof(struct test_env) + TCBL_TEST_MAX_FH * sizeof(vfs_fh));
    return 0;
}

int main(void)
{
    int rc = 0;
    bool stop_on_error = true;
/*
    const struct CMUnitTest base_vfs_tests[] = {
        cmocka_unit_test_setup_teardown(test_nothing, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_open, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_exists, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_write_read_multi_fh, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_delete, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_reopen, generic_setup_0fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_two_files, generic_setup_0fh, generic_teardown)
    };
    printf("\nbase memvfs tests\n");
    rc = cmocka_run_group_tests(base_vfs_tests, generic_pre_group_memvfs, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\nbase unixvfs tests\n");
    rc = cmocka_run_group_tests(base_vfs_tests, generic_pre_group_unixvfs, generic_post_group);
    if (stop_on_error && rc) return rc;

    const struct CMUnitTest generic_vfs_tests[] = {
        cmocka_unit_test_setup_teardown(test_nothing, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_begin_end, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_write_read, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_write_read_by_char, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_bounds, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_read_unaligned, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_write_unaligned, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_partial_read, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_write_gap, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_truncate, generic_setup_1fh, generic_teardown),
        cmocka_unit_test_setup_teardown(test_vfs_delete, generic_setup_0fh, generic_teardown),
    };
    printf("\ngeneric tests - memvfs\n");
    rc = cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_memvfs, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\ngeneric tests - unixvfs\n");
    rc = cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_unixvfs, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\ngeneric tests - uncommitted\n");
    rc = cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_tcbl_txn, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\ngeneric tests - committed\n");
    rc = cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_tcbl_commit, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\ngeneric tests - autocommit\n");
    rc = cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_tcbl_autocommit, generic_post_group);
    if (stop_on_error && rc) return rc;
    printf("\ngeneric tests - checkpointed\n");
    cmocka_run_group_tests(generic_vfs_tests, generic_pre_group_tcbl_checkpoint, generic_post_group);
    if (stop_on_error && rc) return rc;

    const struct CMUnitTest tcbl_tests[] = {
        cmocka_unit_test(test_tcbl_open_close),
        cmocka_unit_test(test_tcbl_write_read),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_nothing_commit, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_nothing_abort, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_write_read_commit, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_write_read_abort, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_multiblock, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_reopen, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_overwrite, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_underlying_fidelity, tcbl_setup_1fh, tcbl_teardown_1fh),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_snapshot, tcbl_setup_2fh_1vfs, tcbl_teardown_2fh_1vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_overwrite, tcbl_setup_2fh_1vfs, tcbl_teardown_2fh_1vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_conflict, tcbl_setup_2fh_1vfs, tcbl_teardown_2fh_1vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_snapshot, tcbl_setup_2fh_2vfs, tcbl_teardown_2fh_2vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_overwrite, tcbl_setup_2fh_2vfs, tcbl_teardown_2fh_2vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_2fh_conflict, tcbl_setup_2fh_2vfs, tcbl_teardown_2fh_2vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_checkpoint, tcbl_setup_1fh, tcbl_teardown_1fh),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_checkpoint_activity, tcbl_setup_1fh, tcbl_teardown_1fh),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_checkpoint_activity_2, tcbl_setup_2fh_1vfs, tcbl_teardown_2fh_1vfs),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_checkpoint_activity_2, tcbl_setup_2fh_2vfs, tcbl_teardown_2fh_2vfs),
    };
    printf("\ntcbl tests\n");
    rc = cmocka_run_group_tests(tcbl_tests, NULL, NULL);
    if (stop_on_error && rc) return rc;

    printf("\nfuzz tests\n");
    const struct CMUnitTest fuzz_vfs_tests[] = {
        cmocka_unit_test_setup_teardown(test_paired_updates, generic_setup_1fh, generic_teardown),
    };
    printf("\nfuzz tests - memvfs\n");
    rc = cmocka_run_group_tests(fuzz_vfs_tests, generic_pre_group_memvfs, generic_post_group);
    printf("\nfuzz tests - committed\n");
    rc = cmocka_run_group_tests(fuzz_vfs_tests, generic_pre_group_tcbl_commit, generic_post_group);
*/
//    const struct CMUnitTest fuzz_vfs_tests_concurrent[] = {
//            cmocka_unit_test_setup_teardown(test_paired_updates, generic_setup_2fh, generic_teardown),
//    };
//    printf("\nfuzz tests - concurrent\n");
//    rc = cmocka_run_group_tests(fuzz_vfs_tests_concurrent, generic_pre_group_tcbl_commit_concurrent, generic_post_group);

    const struct CMUnitTest fuzz_direct_tests[] = {
        cmocka_unit_test(fuzz_updates_direct),
    };
    rc = cmocka_run_group_tests(fuzz_direct_tests, NULL, NULL);

    return rc;
}
