#include <string.h>
#include "test_runtime.h"
#include "cachedvfs.h"
#include "memvfs.h"

typedef struct cache_test_env {
    cvfs cache;
    vfs underlying_fs;
    vfs_fh ufh;
    cvfs_h ch;
} *cache_test_env;

static void setup(cache_test_env env, size_t cache_page_size, int cache_num_pages)
{
    RC_OK(memvfs_allocate(&env->underlying_fs));
    RC_OK(vfs_cache_allocate(&env->cache, cache_page_size, cache_num_pages));
    env->ch = NULL;
    env->ufh = NULL;
}

static int fill_fn(void *fh, void *data, size_t offset, size_t len, size_t *out_len)
{
    return vfs_read_2(fh, data, offset, len, out_len);
}

static void setup_1h(cache_test_env env, size_t underlying_sz)
{
    setup(env, TCBL_TEST_CACHE_PAGE_SIZE, 5);
    char orig_data[underlying_sz];
    prep_data(orig_data, underlying_sz, 2342);
    RC_OK(vfs_open(env->underlying_fs, "test", &env->ufh));
    RC_OK(vfs_write(env->ufh, orig_data, 0, underlying_sz));
    RC_OK(vfs_cache_open(env->cache, &env->ch, fill_fn, env->ufh, NULL));
}

static void teardown(cache_test_env env)
{
    if (env->ch != NULL) {
        RC_OK(vfs_cache_close(env->ch));
        env->ch = NULL;
    }
    if (env->cache != NULL) {
        RC_OK(vfs_cache_free(env->cache));
        env->cache = NULL;
    }
    if (env->ufh != NULL) {
        RC_OK(vfs_close(env->ufh));
        env->ufh = NULL;
    }
    if (env->underlying_fs != NULL) {
        RC_OK(vfs_free(env->underlying_fs));
        env->underlying_fs = NULL;
    }
}

static void c_test_nothing(void **state)
{
    struct cache_test_env env;
    setup(&env, TCBL_TEST_PAGE_SIZE, 5);
    // do nothing
    teardown(&env);
}

static void c_test_one_block(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);
    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;

    size_t read_sz = TCBL_TEST_PAGE_SIZE;
    char underlying_data[read_sz];
    char data[read_sz];

    size_t read_sz_res;
    RC_OK(vfs_cache_get(ch, data, 0, read_sz, &read_sz_res));
    RC_OK(vfs_read(ufh, underlying_data, 0, read_sz));
    assert_int_equal(read_sz_res, read_sz);
    assert_memory_equal(data, underlying_data, read_sz);

    // get the same block again
    RC_OK(vfs_cache_get(ch, data, 0, read_sz, &read_sz_res));
    assert_int_equal(read_sz_res, read_sz);
    assert_memory_equal(data, underlying_data, read_sz);

    teardown(&env);
}

static void c_test_limit(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);
    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;

    size_t read_sz = TCBL_TEST_PAGE_SIZE;
    char underlying_data[read_sz];
    char data[read_sz];

    size_t read_sz_res;
    RC_OK(vfs_cache_get(ch, data, 0, read_sz, &read_sz_res));
    RC_OK(vfs_read(ufh, underlying_data, 0, read_sz));
    assert_int_equal(read_sz_res, read_sz);
    assert_memory_equal(data, underlying_data, read_sz);

    // read entirely out of bounds
    RC_EQ(vfs_cache_get(ch, data, underlying_sz, read_sz, &read_sz_res), TCBL_BOUNDS_CHECK);
    assert_int_equal(read_sz_res, 0);

    // read partially out of bounds
    size_t delta = TCBL_TEST_PAGE_SIZE / 2;
    RC_EQ(vfs_cache_get(ch, data, underlying_sz - delta, read_sz, &read_sz_res), TCBL_BOUNDS_CHECK);
    RC_OK(vfs_read(ufh, underlying_data, underlying_sz - delta, delta));
    assert_int_equal(read_sz_res, delta);
    memset(&underlying_data[delta], 0, read_sz - delta);
    assert_memory_equal(data, underlying_data, read_sz);

    teardown(&env);
}

static void c_test_multiple_blocks(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);

    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;


    size_t read_sz = TCBL_TEST_PAGE_SIZE;
    char underlying_data[read_sz];
    char data[read_sz];

    for (int i = 0; i < 5; i++) {
        size_t offs = i * read_sz;
        size_t read_sz_res;
        RC_OK(vfs_cache_get(ch, data, offs, read_sz, &read_sz_res));
        assert_int_equal(read_sz, read_sz_res);
        RC_OK(vfs_read(ufh, underlying_data, offs, read_sz));
        assert_memory_equal(data, underlying_data, read_sz);
    }

    teardown(&env);
}

static void c_test_fill_fully(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);

    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;


    size_t read_sz = TCBL_TEST_PAGE_SIZE;
    char underlying_data[read_sz];
    char data[read_sz];

    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 10; i++) {
            size_t offs = i * read_sz;
            size_t read_sz_res;
            RC_OK(vfs_cache_get(ch, data, offs, read_sz, &read_sz_res));
            assert_int_equal(read_sz, read_sz_res);
            RC_OK(vfs_read(ufh, underlying_data, offs, read_sz));
            assert_memory_equal(data, underlying_data, read_sz);
        }
    }

    teardown(&env);
}

static void c_test_unaligned(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);

    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;

    size_t read_sz = TCBL_TEST_PAGE_SIZE * 2 / 3;
    char underlying_data[read_sz];
    char data[read_sz];

    for (int j = 0; j < 3; j++) {
        size_t offs = TCBL_TEST_PAGE_SIZE * 1/3;
        while (offs + read_sz < underlying_sz) {
            size_t read_sz_res;
            RC_OK(vfs_cache_get(ch, data, offs, read_sz, &read_sz_res));
            assert_int_equal(read_sz, read_sz_res);
            RC_OK(vfs_read(ufh, underlying_data, offs, read_sz));
            assert_memory_equal(data, underlying_data, read_sz);
            offs += read_sz;
        }
    }

    teardown(&env);
}

static void c_test_unaligned_large(void **state)
{
    struct cache_test_env env;
    size_t underlying_sz = 10 * TCBL_TEST_CACHE_PAGE_SIZE;
    setup_1h(&env, underlying_sz);

    vfs_fh ufh = env.ufh;
    cvfs_h ch = env.ch;


    size_t read_sz = TCBL_TEST_PAGE_SIZE * 4 / 3;
    char underlying_data[read_sz];
    char data[read_sz];

    for (int j = 0; j < 3; j++) {
        size_t offs = TCBL_TEST_PAGE_SIZE * 1/6;
        while (offs + read_sz < underlying_sz) {
            size_t read_sz_res;
            RC_OK(vfs_cache_get(ch, data, offs, read_sz, &read_sz_res));
            assert_int_equal(read_sz, read_sz_res);
            RC_OK(vfs_read(ufh, underlying_data, offs, read_sz));
            assert_memory_equal(data, underlying_data, read_sz);
            offs += read_sz;
        }
    }

    teardown(&env);
}

int main(void) {
    int rc = 0;

    const struct CMUnitTest cache_tests[] = {
            cmocka_unit_test_setup_teardown(c_test_nothing, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_one_block, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_limit, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_multiple_blocks, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_fill_fully, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_unaligned, NULL, NULL),
            cmocka_unit_test_setup_teardown(c_test_unaligned_large, NULL, NULL)
    };
    rc = cmocka_run_group_tests(cache_tests, NULL, NULL);
    return rc;
}