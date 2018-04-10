#include <string.h>
#include "test_runtime.h"
#include "ll_log.h"

#define MAX_HANDLES 2

typedef struct ll_log_test_env {
    tcbl_log log;
    tcbl_log_h h[MAX_HANDLES];
    void (*cleanup)(struct ll_log_test_env *);
} *ll_log_test_env;

static void test_nothing(void **state)
{
    ll_log_test_env env = *state;
    // Let the text fixture open and close the log. Do nothing here.
    env->cleanup(env);
}

static void test_append_one(void **state)
{
    ll_log_test_env env = *state;
    tcbl_log_h lh = env->h[0];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    ll_log_entry_write e = tcbl_malloc(NULL, ll_log_entry_write_sz);
    assert_non_null(e);
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->block_id = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);

    RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));

    tcbl_log_entry entry_out;
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

    tcbl_free(h, e, ll_log_entry_write_sz);

    env->cleanup(env);
}

static void test_append_reset(void **state)
{
    ll_log_test_env env = *state;
    tcbl_log_h lh = env->h[0];

    size_t ll_log_entry_write_sz = sizeof(struct ll_log_entry_write) + TCBL_TEST_PAGE_SIZE;
    ll_log_entry_write e = tcbl_malloc(NULL, ll_log_entry_write_sz);
    assert_non_null(e);
    e->entry_type = LL_LOG_ENTRY_WRITE;
    e->block_id = 123;

    memset(e->data, 1, TCBL_TEST_PAGE_SIZE);

    RC_OK(tcbl_log_append(lh, (tcbl_log_entry) e));

    tcbl_log_entry entry_out;
    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_memory_equal(e, entry_out, ll_log_entry_write_sz);
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    RC_OK(tcbl_log_reset(lh));

    RC_OK(tcbl_log_seek(lh, 0));
    RC_OK(tcbl_log_next(lh, &entry_out));
    assert_null(entry_out);

    tcbl_free(h, e, ll_log_entry_write_sz);

    env->cleanup(env);
}

//static void g_noop(ll_log_test_env env)
//{
//    // do nothing
//}

static void g_cleanup_mem(ll_log_test_env env)
{
    tcbl_log_free(env->log);
}


static int generic_setup(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;
    tcbl_log_init_mem(env->log, TCBL_TEST_PAGE_SIZE);
    return 0;
}

static int generic_setup_1h(void **state)
{
    generic_setup(state);

    ll_log_test_env env = (ll_log_test_env) *state;
    env->h[0] = tcbl_malloc(NULL, env->log->log_h_size);
    RC_OK(tcbl_log_open(env->log, env->h[0]));
    return 0;
}

static int generic_teardown(void **state)
{
    ll_log_test_env env = (ll_log_test_env) *state;
    for (int i = 0; i < MAX_HANDLES; i++) {
        if (env->h[i]) {
            tcbl_log_close(env->h[i]);
            tcbl_free(NULL, env->h[i], env->log->log_h_size);
        }
        env->h[i] = NULL;
    }
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

    env->log = (tcbl_log) log;
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
            cmocka_unit_test_setup_teardown(test_nothing, generic_setup, generic_teardown),
            cmocka_unit_test_setup_teardown(test_nothing, generic_setup_1h, generic_teardown),
            cmocka_unit_test_setup_teardown(test_append_one, generic_setup_1h, generic_teardown),
            cmocka_unit_test_setup_teardown(test_append_reset, generic_setup_1h, generic_teardown),
    };
    rc = cmocka_run_group_tests(ll_log_tests, generic_pre_group_mem, generic_post_group_mem);
    return rc;
}