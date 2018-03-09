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
    vfs_info vfs_ops;
    struct memvfs_file *files;
} *memvfs;

int file_name_comparator(memvfs_file f1, memvfs_file f2)
{
    return strcmp(f1->name, f2->name);
}

int memvfs_new_file(memvfs memvfs, const char *name, memvfs_file *file_out)
{
//    type, list, elem, comparator, next, member
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
    f->name = tcbl_malloc(NULL, strlen(name) + 1);
    if (!f->name) {
        return TCBL_ALLOC_FAILURE;
    }
    strcpy(f->name, name);
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
        size_t new_len = MAX(offset+len, f->alloc_len * 2);
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

int memvfs_free(struct vfs *vfs)
{
    memvfs memvfs = (struct memvfs *) vfs;
    while (memvfs->files) {
        memvfs_file f = memvfs->files;
        SGLIB_LIST_DELETE(struct memvfs_file, memvfs->files, f, next_file);
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
    memvfs->vfs_ops = &memvfs_info;
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

    rc = tcbl_vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_memvfs_open()
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

    rc = vfs_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);

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

    rc = tcbl_vfs_free(memvfs);
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

    rc = tcbl_vfs_free(memvfs);
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

    rc = tcbl_vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

static void test_open_close()
{
    int rc;
    tcbl_vfs t;
    tcbl_fh fh;

    rc = tcbl_allocate(&t, NULL);
    assert_int_equal(rc, TCBL_OK);
    rc = tcbl_open(t, "test-file", &fh);
    assert_int_equal(rc, TCBL_OK);
    rc = tcbl_close(fh);
    assert_int_equal(rc, TCBL_OK);
}

static void test_write_read()
{
    int rc;
    vfs memvfs;
    tcbl_vfs t;
    tcbl_fh fh;

    rc = memvfs_allocate(&memvfs);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_allocate(&t, memvfs);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_open(t, "test-file", &fh);
    assert_int_equal(rc, TCBL_OK);

    size_t data_size = 100;
    char data_in[data_size];
    char data_out[data_size];
    for (int i = 0; i < data_size; i++) {
        data_in[i] = (char) i;
    }
    memset(data_out, 0, sizeof(data_out));

    rc = tcbl_write(fh, data_in, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_read(fh, data_out, 0, data_size);
    assert_int_equal(rc, TCBL_OK);

    assert_memory_equal(data_in, data_out, data_size);

    rc = tcbl_close(fh);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_vfs_free((vfs)t);
    assert_int_equal(rc, TCBL_OK);

    rc = tcbl_vfs_free(memvfs);
    assert_int_equal(rc, TCBL_OK);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_memvfs_create),
        cmocka_unit_test(test_memvfs_open),
        cmocka_unit_test(test_memvfs_write_read),
        cmocka_unit_test(test_memvfs_write_read_2),
        cmocka_unit_test(test_memvfs_reopen),
//        cmocka_unit_test(test_open_close),
//        cmocka_unit_test(test_write_read),
    };
//    return run_tests(tests);
    return cmocka_run_group_tests(tests, NULL, NULL);
}