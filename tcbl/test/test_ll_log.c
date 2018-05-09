#include <string.h>
#include "test_runtime.h"
#include "ll_log.h"
#include "memvfs.h"

#define MAX_HANDLES 2

#define LOG_TYPE_MEM = 1
#define LOG_TYPE_SER = 2

/*
typedef struct ll_log_test_env {
    struct tcbl_log_mem log_mem;
    tcbl_log_h h[MAX_HANDLES];
    tcbl_log log;
    int config_handles;
    void (*configure)(struct ll_log_test_env *);
    void (*cleanup)(struct ll_log_test_env *);
} *ll_log_test_env;

static void test_nothing(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);
    // Let the text fixture open and close the log. Do nothing here.
    env->cleanup(env);
}

static void print_data(void* data, size_t len)
{
    char *d = data;
    size_t i;
    for (i = 0; i < len; i++) {
        if (i > 0) {
            if (i % 8 == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
        printf("%02x", d[i] & 0xff);
    }
    printf("\n");
}

static void test_append_one(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);

    tcbl_log_h lh = env->h[0];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    char buff[ll_log_entry_write_sz];
    for (int i = sizeof(struct ll_log_entry_write); i < ll_log_entry_write_sz; i++) {
        buff[i] = (char) 4;
    }

    ll_log_entry_write e = (ll_log_entry_write) buff;
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->offset = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);


    RC_OK(tcbl_log_append(lh, (ll_log_entry) e));

    ll_log_entry entry_out = NULL;
    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    RC_OK(tcbl_log_meld(lh));

    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));

    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);

    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    env->cleanup(env);
}

static void prep_write_entry(ll_log_entry_write e, size_t block_offs, uint64_t seed)
{
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->offset = block_offs;
    prep_data(&e->data, TCBL_TEST_PAGE_SIZE, seed);
}

static void prep_read_entry(ll_log_entry_read e, size_t block_offs)
{
    e->entry_type = LL_LOG_ENTRY_READ;
    e->offset = block_offs;
}

static void verify_log(tcbl_log_h lh, ll_log_entry *entries, int n_entries)
{
    RC_OK(tcbl_log_seek(lh, 0));
    ll_log_entry entry_out;
    for (int i = 0; i < n_entries; i++) {
        ll_log_entry e = entries[i];
        RC_OK(tcbl_log_next(lh, &entry_out));
        assert_non_null(entry_out);
        assert_int_equal(entry_out->entry_type, e->entry_type);
        size_t sz = ll_log_entry_size(lh->log, e);
        assert_memory_equal(entry_out, e, sz);
    }
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);
}

static void test_ser_no_conflict(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);

    tcbl_log_h lh1 = env->h[0];
    tcbl_log_h lh2 = env->h[1];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    char buff_w[2 * ll_log_entry_write_sz];
    ll_log_entry_write we = (ll_log_entry_write) buff_w;

    size_t ll_log_entry_read_sz = sizeof(struct ll_log_entry_read);
    char buff_r[2 * ll_log_entry_read_sz];
    ll_log_entry_read re = (ll_log_entry_read) buff_r;

    prep_read_entry(&re[0], 10);
    RC_OK(tcbl_log_append(lh1, (ll_log_entry) re));

    prep_write_entry(&we[0], 10, 1);
    RC_OK(tcbl_log_append(lh1, (ll_log_entry) we));

    prep_read_entry(&re[1], 12);
    RC_OK(tcbl_log_append(lh2, (ll_log_entry) re));

    prep_write_entry(&we[1], 12, 1);
    RC_OK(tcbl_log_append(lh2, (ll_log_entry) we));

    RC_OK(tcbl_log_meld(lh2));
    RC_OK(tcbl_log_meld(lh1));

    ll_log_entry entries_expected[4] = {
            (ll_log_entry) &re[1],
            (ll_log_entry) &we[1],
            (ll_log_entry) &re[0],
            (ll_log_entry) &we[0],
    };

    verify_log(lh1, entries_expected, 4);
    verify_log(lh2, entries_expected, 4);

    env->cleanup(env);
}

static void test_ser_conflict(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);

    env->cleanup(env);
}

static void init_entry_write(ll_log_entry_write e, unsigned i)
{
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->offset = 0x123 + i;
    memset(e->data, i, TCBL_TEST_PAGE_SIZE);
}

static void test_append_many(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);

    tcbl_log_h lh = env->h[0];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    char buff[ll_log_entry_write_sz];

    ll_log_entry_write e = (ll_log_entry_write) buff;

    for (unsigned i = 1; i <= 10; i++) {
        init_entry_write(e, i);
        RC_OK(tcbl_log_append(lh, (ll_log_entry) e));
    }

    RC_OK(tcbl_log_seek(lh, 0));
    ll_log_entry entry_out = NULL;
    for (unsigned i = 1; i <= 10; i++) {
        RC_OK(tcbl_log_next(lh, &entry_out));
        init_entry_write(e, i);
        assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    }

    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);


    RC_OK(tcbl_log_meld(lh));

    RC_OK(tcbl_log_seek(lh, 0));
    for (unsigned i = 1; i <= 10; i++) {
        RC_OK(tcbl_log_next(lh, &entry_out));
        init_entry_write(e, i);
        assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    }

    for (unsigned i = 11; i <= 15; i++) {
        init_entry_write(e, i);
        RC_OK(tcbl_log_append(lh, (ll_log_entry) e));
    }

    RC_OK(tcbl_log_seek(lh, 0));
    for (unsigned i = 1; i <= 15; i++) {
        RC_OK(tcbl_log_next(lh, &entry_out));
        init_entry_write(e, i);
        assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    }

    RC_OK(tcbl_log_meld(lh));
    RC_OK(tcbl_log_seek(lh, 0));
    for (unsigned i = 1; i <= 15; i++) {
        RC_OK(tcbl_log_next(lh, &entry_out));
        init_entry_write(e, i);
        assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    }

    env->cleanup(env);
}
static void test_append_reset(void **state)
{
    ll_log_test_env env = *state;
    env->configure(env);

    tcbl_log_h lh = env->h[0];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    char *buff[ll_log_entry_write_sz];
    ll_log_entry_write e = (ll_log_entry_write ) buff;
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->offset = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);

    RC_OK(tcbl_log_append(lh, (ll_log_entry) e));

    ll_log_entry entry_out = NULL;
    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    RC_OK(tcbl_log_reset(lh));

    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    env->cleanup(env);
}

static void create_file_handles(ll_log_test_env env)
{
    for (int i = 0; i < env->config_handles; i++) {
        env->h[i] = tcbl_malloc(NULL, env->log->log_h_size);
        RC_OK(tcbl_log_open(env->log, env->h[i]));
    }
}

static void cleanup_file_handles(ll_log_test_env env)
{
    for (int i = 0; i < env->config_handles; i++) {
        RC_OK(tcbl_log_close(env->h[i]));
        tcbl_free(NULL, env->h[i], env->log->log_h_size);
        env->h[i] = NULL;
    }
}

static void g_configure_mem(ll_log_test_env env)
{
    tcbl_log_init_mem((tcbl_log) &env->log_mem, TCBL_TEST_PAGE_SIZE);
    create_file_handles(env);
}

static void g_cleanup_mem(ll_log_test_env env)
{
    cleanup_file_handles(env);
    tcbl_log_free((tcbl_log) &env->log_mem);
}

static int generic_setup_1h(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;
    env->config_handles = 1;
    return 0;
}

static int generic_setup_2h(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;
    env->config_handles = 2;
    return 0;
}

static int generic_pre_group(void **state)
{
    ll_log_test_env env = tcbl_malloc(NULL, sizeof(struct ll_log_test_env));
    *state = env;
    assert_non_null(env);
    for (int i = 0; i < MAX_HANDLES; i++) {
        env->h[i] = NULL;
    }

//    tcbl_log_mem log = tcbl_malloc(NULL, sizeof(struct tcbl_log_mem));
//    assert_non_null(env);

    env->log = NULL;
    env->config_handles = 0;
    env->configure = NULL;
    env->cleanup = NULL;
}

static int generic_pre_group_mem(void **state)
{
    generic_pre_group(state);
    ll_log_test_env env = (ll_log_test_env) *state;
    env->configure = g_configure_mem;
    env->cleanup = g_cleanup_mem;
    env->log = (tcbl_log) &env->log_mem;
    return 0;
}


static int generic_post_group(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;

    tcbl_free(NULL, env, sizeof(struct ll_log_test_env));
    return 0;
}
*/
typedef struct bc_log_test_env {
    struct bc_log log;
    vfs vfs;
} *bc_log_test_env;

static void bc_test_nothing(void **state)
{
//    bc_log_test_env env = *state;
}

static void bc_test_write_read(void **state)
{
    bc_log_test_env env = *state;
    struct bc_log_h h;

    RC_OK(bc_log_txn_begin(&env->log, &h));

    size_t sz = TCBL_TEST_PAGE_SIZE;
    char buff[sz];
    prep_data(buff, sz, 1);
    RC_OK(bc_log_write(&h, 0, buff, sz));

    bool found;
    char* found_data;
    size_t found_sz;
    RC_OK(bc_log_read(&h, 0, &found, (void **) &found_data, &found_sz));
    assert_true(found);
    assert_non_null(found_data);
    assert_int_equal(found_sz, sz);
    assert_memory_equal(found_data, buff, sz);
}

static void bc_test_write_read_multiple(void **state)
{
    bc_log_test_env env = *state;
    struct bc_log_h h;

    RC_OK(bc_log_txn_begin(&env->log, &h));

    size_t sz = TCBL_TEST_PAGE_SIZE;
    int n = 5;
    char buff[sz];
    for (int i = 0; i < n; i++) {
        prep_data(buff, sz, i);
        RC_OK(bc_log_write(&h, i * sz, buff, (i + 1) * sz));
    }

    for (int i = 0; i < n; i++) {
        bool found;
        char *found_data;
        size_t found_sz;
        RC_OK(bc_log_read(&h, i * sz, &found, (void **) &found_data, &found_sz));
        assert_true(found);
        assert_non_null(found_data);
        assert_int_equal(found_sz, sz * (i + 1));
        prep_data(buff, sz, i);
        assert_memory_equal(found_data, buff, sz);
    }
}

static int generic_pre_group_bc(void **state)
{

    bc_log_test_env env = tcbl_malloc(NULL, sizeof(struct bc_log_test_env));
    *state = env;
    memvfs_allocate(&env->vfs);
    bc_log_create(&env->log, env->vfs, NULL, NULL, "TEST", TCBL_TEST_PAGE_SIZE);
    return 0;
}

static int generic_post_group_bc(void **state)
{
    bc_log_test_env env = (bc_log_test_env) *state;
    memvfs_free(env->vfs);
    return 0;
}

int main(void) {
    int rc = 0;
//    bool stop_on_error = true;
    /*
    const struct CMUnitTest ll_log_tests[] = {
            cmocka_unit_test_setup_teardown(test_nothing, NULL, NULL),
            cmocka_unit_test_setup_teardown(test_nothing, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_one, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_reset, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_many, generic_setup_1h, NULL)
    };
    rc = cmocka_run_group_tests(ll_log_tests, generic_pre_group_mem, generic_post_group);
    if (stop_on_error && rc) return rc;
     */

    const struct CMUnitTest bc_log_tests[] = {
            cmocka_unit_test_setup_teardown(bc_test_nothing, NULL, NULL),
            cmocka_unit_test_setup_teardown(bc_test_nothing, NULL, NULL),
            cmocka_unit_test_setup_teardown(bc_test_write_read, NULL, NULL),
            cmocka_unit_test_setup_teardown(bc_test_write_read_multiple, NULL, NULL),
    };
    rc = cmocka_run_group_tests(bc_log_tests, generic_pre_group_bc, generic_post_group_bc);
    return rc;
}