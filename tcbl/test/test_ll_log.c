#include <string.h>
#include "test_runtime.h"
#include "ll_log.h"

#define MAX_HANDLES 2

typedef struct ll_log_test_env {
    tcbl_log log;
    tcbl_log_h h[MAX_HANDLES];
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

//    for (int i = sizeof(struct ll_log_entry_write); i < ll_log_entry_write_sz; i++) {
//        assert_int_equal(buff[i], 4);
//    }

    ll_log_entry_write e = (ll_log_entry_write) buff;
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->block_id = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);


    RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));

    tcbl_log_entry entry_out = NULL;
    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

//    print_data(buff, ll_log_entry_write_sz);

    RC_OK(tcbl_log_meld(lh));

    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
//    printf("have output at location %p\n", entry_out);
//    print_data(entry_out, ll_log_entry_write_sz);
    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
//    printf("next...\n");
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    env->cleanup(env);
}

static void init_entry_write(ll_log_entry_write e, unsigned i)
{
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->block_id = 0x123 + i;
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
        RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));
    }

    RC_OK(tcbl_log_seek(lh, 0));
    tcbl_log_entry entry_out = NULL;
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
        RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));
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
    e->block_id = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);

    RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));

    tcbl_log_entry entry_out = NULL;
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

static void g_configure_mem(ll_log_test_env env)
{
    tcbl_log_init_mem(env->log, TCBL_TEST_PAGE_SIZE);
    for (int i = 0; i < env->config_handles; i++) {
        env->h[i] = tcbl_malloc(NULL, env->log->log_h_size);
        RC_OK(tcbl_log_open(env->log, env->h[i]));
    }
}

static void g_cleanup_mem(ll_log_test_env env)
{
    for (int i = 0; i < env->config_handles; i++) {
        RC_OK(tcbl_log_close(env->h[i]));
        tcbl_free(NULL, env->h[i], env->log->log_h_size);
        env->h[i] = NULL;
    }
    tcbl_log_free(env->log);
}

static int generic_setup_1h(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;
    env->config_handles = 1;
    return 0;
}

static int generic_pre_group_mem(void **state)
{
    ll_log_test_env env = tcbl_malloc(NULL, sizeof(struct ll_log_test_env));
    *state = env;
    assert_non_null(env);
    for (int i = 0; i < MAX_HANDLES; i++) {
        env->h[i] = NULL;
    }

    tcbl_log_mem log = tcbl_malloc(NULL, sizeof(struct tcbl_log_mem));
    assert_non_null(env);

    env->log = (tcbl_log) log;
    env->config_handles = 0;
    env->configure = g_configure_mem;
    env->cleanup = g_cleanup_mem;
    return 0;
}

static int generic_post_group_mem(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;

    tcbl_free(NULL, env->log, sizeof(struct tcbl_log_mem));
    tcbl_free(NULL, env, sizeof(struct ll_log_test_env));
    return 0;
}


int main(void) {
    int rc = 0;
    const struct CMUnitTest ll_log_tests[] = {
            cmocka_unit_test_setup_teardown(test_nothing, NULL, NULL),
            cmocka_unit_test_setup_teardown(test_nothing, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_one, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_reset, generic_setup_1h, NULL),
            cmocka_unit_test_setup_teardown(test_append_many, generic_setup_1h, NULL),
    };
    rc = cmocka_run_group_tests(ll_log_tests, generic_pre_group_mem, generic_post_group_mem);
    return rc;
}