#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <sys/param.h>

#include "tcbl_vfs.h"
#include "runtime.h"
#include "sglib.h"

typedef struct memvfs_file {
    char *name;
    size_t name_alloc_len;
    char *data;
    size_t len;
    size_t alloc_len;
    struct memvfs_file *next_file;
} *memvfs_file;

typedef struct memvfs_fh {
    struct memvfs *vfs;
    struct memvfs_file *memvfs_file;
} *memvfs_fh;

typedef struct memvfs {
    vfs_info vfs_info;
    struct memvfs_file *files;
} *memvfs;

int file_name_comparator(memvfs_file f1, memvfs_file f2)
{
    return strcmp(f1->name, f2->name);
}

int memvfs_new_file(memvfs memvfs, const char *name, memvfs_file *file_out)
{
    struct memvfs_file* result;
    struct memvfs_file search_file = {
            (char*) name
    };
    SGLIB_LIST_FIND_MEMBER(struct memvfs_file, memvfs->files, &search_file, file_name_comparator, next_file, result);
    if (result) {
        *file_out = result;
        return TCBL_OK;
    }
    memvfs_file f = tcbl_malloc(NULL, sizeof(struct memvfs_file));
    if (!f) {
        return TCBL_ALLOC_FAILURE;
    }
    size_t name_len = strlen(name) + 1;
    f->name = tcbl_malloc(NULL, name_len);
    if (!f->name) {
        return TCBL_ALLOC_FAILURE;
    }
    strcpy(f->name, name);
    f->name_alloc_len = name_len;
    f->data = 0;
    f->len = 0;
    f->alloc_len = 0;
    *file_out = f;
    SGLIB_LIST_ADD(struct memvfs_file, memvfs->files, f, next_file);
    return TCBL_OK;
}

int memvfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out)
{
    memvfs_fh fh = tcbl_malloc(NULL, sizeof(struct memvfs_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = (memvfs) vfs;
    *file_handle_out = (vfs_fh) fh;

    // TODO create file if does not exist
    int rc;
    rc = memvfs_new_file((memvfs) vfs, file_name, &fh->memvfs_file);
    return rc;
}

int memvfs_close(vfs vfs, vfs_fh file_handle)
{
    tcbl_free(NULL, file_handle, vfs->fh_size);
    return TCBL_OK;
}

int memvfs_read(vfs vfs, vfs_fh file_handle, char *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    if (f->len < offset + len) {
        return TCBL_BOUNDS_CHECK;
    }
    memcpy(buff, &f->data[offset], len);
    return TCBL_OK;
}

int memvfs_write(struct vfs *vfs, vfs_fh file_handle, const char *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    // ensure length
    if (f->alloc_len < offset + len) {
        size_t new_len = MAX(offset + len, f->alloc_len * 2);
        char* new_data = tcbl_malloc(NULL, new_len);
        if (!new_data) {
            return TCBL_ALLOC_FAILURE;
        }
        if (f->data) {
            memcpy(new_data, f->data, f->len);
            tcbl_free(NULL, f->data, f-alloc_len);
        }
        f->data = new_data;
        f->alloc_len = new_len;
    }

    memcpy(&f->data[offset], buff, len);
    if (offset > f->len) {
        memset(&f->data[f->len], 0, offset - f->len);
    }
    if (offset + len > f->len) {
        f->len = offset + len;
    }
    return TCBL_OK;
}

int memvfs_free(vfs vfs)
{
    memvfs memvfs = (struct memvfs *) vfs;
    while (memvfs->files) {
        memvfs_file f = memvfs->files;
        SGLIB_LIST_DELETE(struct memvfs_file, memvfs->files, f, next_file);
        tcbl_free(NULL, f->name, f->name_alloc_len);
        if (f->data) {
            tcbl_free(NULL, f->data, f->alloc_len);
        }
        tcbl_free(NULL, f, sizeof(struct memvfs_file));
    }
    return TCBL_OK;
}


static int memvfs_allocate(vfs *vfs)
{
    static struct vfs_info memvfs_info = {
            sizeof(struct memvfs),
            sizeof(struct memvfs_fh),
            &memvfs_open,
            &memvfs_close,
            &memvfs_read,
            &memvfs_write,
            &memvfs_free
    };
    memvfs memvfs = tcbl_malloc(NULL, sizeof(struct memvfs));
    if (!memvfs) {
        return TCBL_ALLOC_FAILURE;
    }
    memvfs->vfs_info = &memvfs_info;
    memvfs->files = NULL;
    *vfs = (struct vfs *) memvfs;
    return TCBL_OK;
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

#define RC_OK(__x) assert_int_equal(__x, TCBL_OK);

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
    int rc;
    vfs memvfs;
    vfs_fh fh;

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(memvfs);

    rc = vfs_open(memvfs, "/test-file", &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    rc = vfs_write(fh, data_in, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_read(fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    assert_memory_equal(data_in, data_out, data_size);

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_memvfs_write_read_2()
{
    int rc;
    vfs memvfs;
    vfs_fh fh;

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(memvfs);

    rc = vfs_open(memvfs, "/test-file", &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);

    size_t sz = 100;
    char buff[sz];
    for (int i = 0; i < sz; i++) {
        buff[0] = (char) i;
        rc = vfs_write(fh, buff, i, 1);
        assert_int_equal(rc, TCBL_OK);
    }
    rc = vfs_read(fh, buff, 0, sz);
    assert_int_equal(rc, TCBL_OK);
    for (int i = 0; i < sz; i++) {
        assert_int_equal((char) i, buff[i]);
    }

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_memvfs_reopen()
{
    int rc;
    vfs memvfs;
    vfs_fh fh;
    char *test_file_name = "/test_file";

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(memvfs);

    rc = vfs_open(memvfs, test_file_name, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    rc = vfs_write(fh, data_in, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_open(memvfs, test_file_name, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);

    rc = vfs_read(fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    assert_memory_equal(data_in, data_out, data_size);

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_memvfs_two_files()
{
    int rc;
    vfs memvfs;
    vfs_fh fh;
    char *test_file_name_1 = "/test_file";
    char *test_file_name_2 = "/another_file";

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(memvfs);

    rc = vfs_open(memvfs, test_file_name_1, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);
    assert_ptr_equal(memvfs, fh->vfs);

    size_t data_size = 100;
    char data_in_1[data_size];
    char data_in_2[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in_1[i] = (char) i;
    }

    rc = vfs_write(fh, data_in_1, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    for (int i = 0; i < data_size; i++) {
        data_in_2[i] = (char) (2 * i);
    }

    rc = vfs_open(memvfs, test_file_name_2, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);

    rc = vfs_write(fh, data_in_2, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_open(memvfs, test_file_name_1, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    rc = vfs_read(fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);
    assert_memory_equal(data_in_1, data_out, data_size);
    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_open(memvfs, test_file_name_2, &fh);
    assert_int_equal(rc, TCBL_OK);
    assert_non_null(fh);
    memset(data_out, 0, sizeof(data_out));
    rc = vfs_read(fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);
    assert_memory_equal(data_in_2, data_out, data_size);
    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_tcbl_open_close()
{
    vfs memvfs;
    tvfs tcbl;
    tcbl_fh fh;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);

    RC_OK(tcbl_allocate(&tcbl, memvfs));

    RC_OK(vfs_open((vfs) tcbl, "test-file", (vfs_fh *) &fh));
    assert_ptr_equal(tcbl, fh->vfs);

    RC_OK(vfs_close((vfs_fh) fh));
    RC_OK(vfs_free((vfs) tcbl));
    RC_OK(vfs_free(memvfs));
}

static void test_tcbl_write_read()
{
    int rc;
    vfs memvfs;
    tvfs tcbl;
    tcbl_fh fh;

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_allocate(&tcbl, memvfs);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_open((vfs) tcbl, "test-file", (vfs_fh *) &fh);
    assert_int_equal(rc, TCBL_OK);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    rc = vfs_write((vfs_fh) fh, data_in, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_read((vfs_fh) fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    assert_memory_equal(data_in, data_out, data_size);

    rc = vfs_close((vfs_fh) fh);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free((vfs) tcbl);
    assert_int_equal(rc, TCBL_OK);

    rc = vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_tcbl_txn_nothing_commit(void **state)
{
    tvfs tcbl = *state;
    vfs_txn txn;

    RC_OK(vfs_txn_begin(tcbl, &txn));
    assert_ptr_equal(tcbl, txn->vfs);

    RC_OK(vfs_txn_commit(txn));
}

static void test_tcbl_txn_nothing_abort(void **state)
{

    tvfs tcbl = *state;
    vfs_txn txn;

    RC_OK(vfs_txn_begin(tcbl, &txn));
    assert_ptr_equal(tcbl, txn->vfs);

    RC_OK(vfs_txn_abort(txn));
}

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

static void test_tcbl_txn_write_read_commit(void **state)
{
    tvfs tcbl = *state;
    vfs_txn txn;

    char *test_filename = "/test-file";

    size_t data_len = 1000;
    char data_in[data_len];
    char data_out[data_len];
    prep_data(data_in, data_len, 98345);

    vfs_fh fh;
    RC_OK(vfs_open((vfs) tcbl, test_filename, &fh));

//    RC_OK(vfs_txn_begin(tcbl, &txn));

//
//    RC_OK(vfs_txn_write(txn, fh, data_in, 0, data_len));
//
//    memset(data_out, 0, sizeof(data_out));
//    RC_OK(vfs_txn_read(txn, fh, data_out, 0, data_len));
//    assert_memory_equal(data_in, data_out, data_len);
//
//    RC_OK(vfs_txn_commit(txn));
//
//    memset(data_out, 0, sizeof(data_out));
//    RC_OK(vfs_read(fh, data_out, 0, data_len));
//    assert_memory_equal(data_in, data_out, data_len);

    RC_OK(vfs_close(fh));
}

static void test_tcbl_txn_write_read_abort(void **state)
{

}

static int tcbl_setup(void **state)
{
    vfs memvfs;
    tvfs tcbl;

    RC_OK(memvfs_allocate(&memvfs));
    assert_non_null(memvfs);
    RC_OK(tcbl_allocate(&tcbl, memvfs));

    *state = tcbl;
    return 0;
}

static int tcbl_teardown(void **state)
{
    tcbl_vfs tcbl = *state;
    vfs memvfs = tcbl->underlying_vfs;

    RC_OK(vfs_free((vfs) tcbl));
    RC_OK(vfs_free(memvfs));

    return 0;
}

//static void test_leak_memory()
//{
//    int *tmp = tcbl_malloc(NULL, sizeof(int));
//    *tmp = 0;
//}


int main(void)
{
    int rc;
    const struct CMUnitTest memvfs_tests[] = {
//        cmocka_unit_test(test_leak_memory),
//        cmocka_unit_test(test_memvfs_create),
//        cmocka_unit_test(test_memvfs_open),
//        cmocka_unit_test(test_memvfs_write_read),
//        cmocka_unit_test(test_memvfs_write_read_2),
//        cmocka_unit_test(test_memvfs_reopen),
//        cmocka_unit_test(test_memvfs_two_files),
    };
    rc = cmocka_run_group_tests_name("memvfs", memvfs_tests, NULL, NULL);
    if (rc) {
        return rc;
    }
    const struct CMUnitTest tcbl_tests[] = {
//        cmocka_unit_test(test_tcbl_open_close),
//        cmocka_unit_test(test_tcbl_write_read),
//        cmocka_unit_test_setup_teardown(test_tcbl_txn_nothing_commit, tcbl_setup, tcbl_teardown),
//        cmocka_unit_test_setup_teardown(test_tcbl_txn_nothing_abort, tcbl_setup, tcbl_teardown),
        cmocka_unit_test_setup_teardown(test_tcbl_txn_write_read_commit, tcbl_setup, tcbl_teardown),
//        cmocka_unit_test_setup_teardown(test_tcbl_txn_write_read_abort, tcbl_setup, tcbl_teardown),
    };
    return cmocka_run_group_tests_name("tcbl", tcbl_tests, NULL, NULL);
}