#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <sys/param.h>

#define TCBL_TEST_PAGE_SIZE 64

#include "memvfs.h"
#include "tcbl_vfs.h"
#include "runtime.h"


#define RC_OK(__x) assert_int_equal(__x, TCBL_OK)
#define RC_NOT_OK(__x) assert_int_not_equal(__x, TCBL_OK)
#define RC_EQ(__x, __erc) assert_int_equal(__x, __erc)

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

static void test_memvfs_create()
{
    int rc;
    vfs memvfs;

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_memvfs_open()
{
    vfs memvfs;
    vfs_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, "/test-file", &fh));
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

    RC_OK(vfs_close(fh));

    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_write_read()
{
    vfs memvfs;
    vfs_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, "/test-file", &fh));
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

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

    RC_OK(vfs_close(fh));

    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_write_read_by_char()
{
    vfs memvfs;
    vfs_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, "/test-file", &fh));
    assert_non_null(fh);

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

    RC_OK(vfs_close(fh));

    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_write_read_multi_fh()
{
    vfs memvfs;
    vfs_fh fh1;
    vfs_fh fh2;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, "/test-file", &fh1));
    assert_non_null(fh1);

    RC_OK(vfs_open(memvfs, "/test-file", &fh2));
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
    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_write_gap()
{
    vfs memvfs;
    vfs_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, "/test-file", &fh));
    assert_non_null(fh);

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

    RC_OK(vfs_close(fh));
    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_reopen()
{
    vfs memvfs;
    vfs_fh fh;
    char *test_file_name = "/test_file";

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, test_file_name, &fh));
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    RC_OK(vfs_write(fh, data_in, 0, data_size));

    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(memvfs, test_file_name, &fh));
    assert_non_null(fh);

    RC_OK(vfs_read(fh, data_out, 0, data_size));

    assert_memory_equal(data_in, data_out, data_size);

    RC_OK(vfs_close(fh));

    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_two_files()
{
    vfs memvfs;
    vfs_fh fh;
    char *test_file_name_1 = "/test_file";
    char *test_file_name_2 = "/another_file";

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(vfs_open(memvfs, test_file_name_1, &fh));
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

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

    RC_OK(vfs_open(memvfs, test_file_name_2, &fh));
    assert_non_null(fh);

    RC_OK(vfs_write(fh, data_in_2, 0, data_size));

    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(memvfs, test_file_name_1, &fh));
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_size));
    assert_memory_equal(data_in_1, data_out, data_size);
    RC_OK(vfs_close(fh));

    RC_OK(vfs_open(memvfs, test_file_name_2, &fh));
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_size));
    assert_memory_equal(data_in_2, data_out, data_size);
    RC_OK(vfs_close(fh));

    RC_OK(vfs_free(memvfs));
}

static void test_memvfs_truncate()
{
    vfs memvfs;
    vfs_fh fh;
    char *test_file_name = "/test_file";

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);
    RC_OK(vfs_open(memvfs, test_file_name, &fh));

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

    size_t file_size;
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, nwrite * sz);

    RC_OK(vfs_truncate(fh, 3 * sz));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 3 * sz);
    pos = 0;
    for (int i = 0; i < 3; i++) {
        prep_data(expected_buff, sz, (uint64_t) i * 459);
        RC_OK(vfs_read(fh, buff, pos, sz));
        assert_memory_equal(buff, expected_buff, sz);
        pos += sz;
    }
    RC_NOT_OK(vfs_read(fh, buff, pos, sz));

    RC_OK(vfs_truncate(fh, 5 * sz));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 5 * sz);
    pos = 0;
    for (int i = 0; i < 5; i++) {
        if (i < 3) {
            prep_data(expected_buff, sz, (uint64_t) i * 459);
        } else {
            memset(expected_buff, 0, sz);
        }
        RC_OK(vfs_read(fh, buff, pos, sz));
        assert_memory_equal(buff, expected_buff, sz);
        pos += sz;
    }

    RC_OK(vfs_truncate(fh, 0));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, 0);
    for (int i = 0; i < nwrite; i++) {
        RC_NOT_OK(vfs_read(fh, buff, pos, sz));
        pos += sz;
    }

    RC_OK(vfs_close(fh));
    RC_OK(vfs_free(memvfs));
}

static void test_tcbl_open_close()
{
    vfs memvfs;
    tvfs tcbl;
    tcbl_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(tcbl_allocate(&tcbl, memvfs, TCBL_TEST_PAGE_SIZE));

    RC_OK(vfs_open((vfs) tcbl, "test-file", (vfs_fh *) &fh));
    assert_ptr_equal(tcbl, fh->vfs);

    RC_OK(vfs_close((vfs_fh) fh));
    RC_OK(vfs_free((vfs) tcbl));
    RC_OK(vfs_free(memvfs));
}

static void test_tcbl_write_read()
{
    vfs memvfs;
    tvfs tcbl;
    tcbl_fh fh;

    RC_OK(memvfs_allocate(&memvfs));

    RC_OK(tcbl_allocate(&tcbl, memvfs, TCBL_TEST_PAGE_SIZE));

    RC_OK(vfs_open((vfs) tcbl, "test-file", (vfs_fh *) &fh));

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

    RC_OK(vfs_free((vfs) tcbl));

    RC_OK(vfs_free(memvfs));
}

typedef struct tcbl_test_env {
    tvfs tvfs;
    vfs memvfs;
    vfs_fh fh[0];
} *tcbl_test_env;

static int tcbl_setup(void **state)
{
    tcbl_test_env env = tcbl_malloc(NULL, sizeof(struct tcbl_test_env));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->memvfs));
    assert_non_null(env->memvfs);
    RC_OK(tcbl_allocate(&env->tvfs, (vfs) env->memvfs, TCBL_TEST_PAGE_SIZE));

    *state = env;
    return 0;
}

static int tcbl_teardown(void **state)
{
    tcbl_test_env env = *state;
    RC_OK(vfs_free((vfs) env->tvfs));
    RC_OK(vfs_free((vfs) env->memvfs));
    tcbl_free(NULL, env, sizeof(struct tcbl_test_env));

    return 0;
}

static int tcbl_setup_1fh(void **state)
{
    tcbl_test_env env = tcbl_malloc(NULL, sizeof(struct tcbl_test_env) + sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->memvfs));
    assert_non_null(env->memvfs);
    RC_OK(tcbl_allocate(&env->tvfs, (vfs) env->memvfs, TCBL_TEST_PAGE_SIZE));


    char *test_filename = "/test-file";
    RC_OK(vfs_open((vfs) env->tvfs, test_filename, &env->fh[0]));

    *state = env;
    return 0;
}

static int tcbl_teardown_1fh(void **state)
{
    tcbl_test_env env = *state;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_free((vfs) env->tvfs));
    RC_OK(vfs_free((vfs) env->memvfs));
    tcbl_free(NULL, env, sizeof(struct tcbl_test_env + sizeof(vfs_fh)));
    return 0;
}

static int tcbl_setup_2fh_1vfs(void **state)
{
    tcbl_test_env env = tcbl_malloc(NULL, sizeof(struct tcbl_test_env) + 2 * sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->memvfs));
    assert_non_null(env->memvfs);
    RC_OK(tcbl_allocate(&env->tvfs, (vfs) env->memvfs, TCBL_TEST_PAGE_SIZE));


    char *test_filename = "/test-file";
    RC_OK(vfs_open((vfs) env->tvfs, test_filename, &env->fh[0]));
    RC_OK(vfs_open((vfs) env->tvfs, test_filename, &env->fh[1]));

    *state = env;
    return 0;
}

static int tcbl_teardown_2fh_1vfs(void **state)
{
    tcbl_test_env env = *state;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_close(env->fh[1]));
    RC_OK(vfs_free((vfs) env->tvfs));
    RC_OK(vfs_free((vfs) env->memvfs));
    tcbl_free(NULL, env, sizeof(struct tcbl_test_env + 2 * sizeof(vfs_fh)));
    return 0;
}

static int tcbl_setup_2fh_2vfs(void **state)
{
    tcbl_test_env env = tcbl_malloc(NULL, sizeof(struct tcbl_test_env) + 2 * sizeof(vfs_fh));
    assert_non_null(env);

    RC_OK(memvfs_allocate((vfs*)&env->memvfs));
    assert_non_null(env->memvfs);
    tvfs vfs1, vfs2;
    RC_OK(tcbl_allocate(&vfs1, (vfs) env->memvfs, TCBL_TEST_PAGE_SIZE));
    RC_OK(tcbl_allocate(&vfs2, (vfs) env->memvfs, TCBL_TEST_PAGE_SIZE));

    char *test_filename = "/test-file";
    RC_OK(vfs_open((vfs) vfs1, test_filename, &env->fh[0]));
    RC_OK(vfs_open((vfs) vfs2, test_filename, &env->fh[1]));

    env->tvfs = 0;
    *state = env;
    return 0;
}

static int tcbl_teardown_2fh_2vfs(void **state)
{
    tcbl_test_env env = *state;
    vfs vfs1 = env->fh[0]->vfs;
    vfs vfs2 = env->fh[1]->vfs;
    RC_OK(vfs_close(env->fh[0]));
    RC_OK(vfs_close(env->fh[1]));
    RC_OK(vfs_free(vfs1));
    RC_OK(vfs_free(vfs2));
    RC_OK(vfs_free((vfs) env->memvfs));
    tcbl_free(NULL, env, sizeof(struct tcbl_test_env) + 2 * sizeof(vfs_fh));
    return 0;
}

static void test_tcbl_txn_nothing_commit(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;
    vfs_fh fh;
    char *test_filename = "/test-file";

    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));
    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_txn_commit(fh));
    RC_OK(vfs_close(fh));
    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_nothing_abort(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;
    vfs_fh fh;
    char *test_filename = "/test-file";

    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));
    RC_OK(vfs_txn_begin(fh));
    RC_OK(vfs_txn_abort(fh));
    RC_OK(vfs_close(fh));
    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_write_read_commit(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;

    char *test_filename = "/test-file";

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));

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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_write_read_abort(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;

    char *test_filename = "/test-file";

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));

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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_multiblock(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;

    char *test_filename = "/test-file";

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));

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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_reopen(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;

    char *test_filename = "/test-file";

    size_t data_len = TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));
    RC_OK(vfs_write(fh, data_in, 0, data_len));
    RC_OK(vfs_close(fh));

    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));
    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    memset(data_out, 0, sizeof(data_out));
    RC_OK(vfs_read(fh, data_out, 0, data_len));
    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_file_size(fh, &file_size));
    assert_int_equal(file_size, data_len);

    RC_OK(vfs_close(fh));

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_overwrite(void **state)
{
    tcbl_test_env env = *state;
    tvfs tcbl = env->tvfs;

    char *test_filename = "/test-file";

    size_t data_len = 2 * TCBL_TEST_PAGE_SIZE;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);
    size_t file_size;

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

#define verify_length(__fh, __el) { size_t file_size; RC_OK(vfs_file_size(__fh, &file_size)); assert_int_equal(file_size, __el); }
#define verify_data(__fh, __ed, __offs, __len) { char buff[__len]; RC_OK(vfs_read(__fh, buff, __offs, __len)); assert_memory_equal(buff, __ed, __len); }
#define verify_file(__fh, __ed, __el) { verify_length(__fh, __el); verify_data(__fh, __ed, 0, __el); }

static void test_tcbl_txn_2fh_snapshot(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_2fh_overwrite(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_2fh_conflict(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_checkpoint(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_checkpoint_activity(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

static void test_tcbl_txn_checkpoint_activity_2(void **state)
{
    tcbl_test_env env = *state;
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

    RC_OK(memvfs_free((vfs) env->memvfs));
}

//static void test_leak_memory()
//{
//    int *tmp = tcbl_malloc(NULL, sizeof(int));
//    *tmp = 0;
//}


int main(void)
{
    int rc = 0;
    const struct CMUnitTest memvfs_tests[] = {
//        cmocka_unit_test(test_leak_memory),
        cmocka_unit_test(test_memvfs_create),
        cmocka_unit_test(test_memvfs_open),
        cmocka_unit_test(test_memvfs_write_read),
        cmocka_unit_test(test_memvfs_write_read_by_char),
        cmocka_unit_test(test_memvfs_write_read_multi_fh),
        cmocka_unit_test(test_memvfs_write_gap),
        cmocka_unit_test(test_memvfs_reopen),
        cmocka_unit_test(test_memvfs_two_files),
        cmocka_unit_test(test_memvfs_truncate)
    };
    rc = cmocka_run_group_tests_name("memvfs", memvfs_tests, NULL, NULL);
    if (rc) {
        return rc;
    }
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
    return cmocka_run_group_tests_name("tcbl", tcbl_tests, NULL, NULL);
}
