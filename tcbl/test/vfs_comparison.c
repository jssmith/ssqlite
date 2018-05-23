#include <string.h>
#include <sys/param.h>
#include "vfs_comparison.h"


int cmp_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out)
{
    comparison_vfs_fh fh = tcbl_malloc(NULL, sizeof(struct comparison_vfs_fh));
    if (fh == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = vfs;
    comparison_vfs cmp_vfs = (comparison_vfs) vfs;
    int rc1, rc2;
    rc1 = vfs_open(cmp_vfs->vfs_a, file_name, &fh->vfs_fh_a);
    rc2 = vfs_open(cmp_vfs->vfs_b, file_name, &fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    *file_handle_out = (vfs_fh) fh;
    return MAX(rc1, rc2);
}

int cmp_delete(vfs vfs, const char *file_name)
{
    comparison_vfs cmp_vfs = (comparison_vfs) vfs;
    int rc1, rc2;
    rc1 = vfs_delete(cmp_vfs->vfs_a, file_name);
    rc2 = vfs_delete(cmp_vfs->vfs_b, file_name);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_exists(vfs vfs, const char *file_name, int *out)
{
    comparison_vfs cmp_vfs = (comparison_vfs) vfs;
    int rc1, rc2;
    int out1, out2;
    rc1 = vfs_exists(cmp_vfs->vfs_a, file_name, &out1);
    rc2 = vfs_exists(cmp_vfs->vfs_b, file_name, &out2);
    if (&out1 != &out2) {
        printf("CMP FAILURE: %d %d\n", out1, out2);
    }
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    *out = out1;
    return MAX(rc1, rc2);

}

int cmp_close(vfs_fh file_handle)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_close(cmp_vfs_fh->vfs_fh_a);
    rc2 = vfs_close(cmp_vfs_fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}


int cmp_read(vfs_fh file_handle, void* data, size_t offset, size_t len, size_t *out_len)
{
    printf("CMP READ: %ld %ld\n", offset, len);
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    size_t out_len1, out_len2;
    char data1[len];
    char data2[len];
    rc1 = vfs_read_2(cmp_vfs_fh->vfs_fh_a, data1, offset, len, &out_len1);
    rc2 = vfs_read_2(cmp_vfs_fh->vfs_fh_b, data2, offset, len, &out_len2);
    if (out_len1 != out_len2) {
        printf("CMP FAILURE: %ld %ld\n", out_len1, out_len2);
    }
    if (memcmp(data1, data2, len) != 0) {
        printf("CMP FAILURE: on data\n");
    }
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    if (out_len != NULL) {
        *out_len = out_len1;
    }
    memcpy(data, data1, len);
    return MAX(rc1, rc2);
}

int cmp_write(vfs_fh file_handle, const void* data, size_t offset, size_t len)
{
    printf("CMP WRITE: %ld %ld\n", offset, len);
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_write(cmp_vfs_fh->vfs_fh_a, data, offset, len);
    rc2 = vfs_write(cmp_vfs_fh->vfs_fh_b, data, offset, len);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_lock(vfs_fh file_handle, int lock_operation)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_lock(cmp_vfs_fh->vfs_fh_a, lock_operation);
    rc2 = vfs_lock(cmp_vfs_fh->vfs_fh_b, lock_operation);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_file_size(vfs_fh file_handle, size_t *out)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    size_t out1, out2;
    rc1 = vfs_file_size(cmp_vfs_fh->vfs_fh_a, &out1);
    rc2 = vfs_file_size(cmp_vfs_fh->vfs_fh_b, &out2);
    if (out1 != out2) {
        printf("CMP FAILURE: %ld %ld\n", out1, out2);
    }
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    *out = out1;
    return MAX(rc1, rc2);
}

int cmp_truncate(vfs_fh file_handle, size_t len)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_truncate(cmp_vfs_fh->vfs_fh_a, len);
    rc2 = vfs_truncate(cmp_vfs_fh->vfs_fh_b, len);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_freevfs(vfs vfs)
{
    comparison_vfs cmp_vfs = (comparison_vfs) vfs;
    int rc1, rc2;
    rc1 = vfs_free(cmp_vfs->vfs_a);
    rc2 = vfs_free(cmp_vfs->vfs_b);
    return MAX(rc1, rc2);
}

int cmp_txn_begin(vfs_fh file_handle)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_txn_begin(cmp_vfs_fh->vfs_fh_a);
    rc2 = vfs_txn_begin(cmp_vfs_fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_txn_commit(vfs_fh file_handle)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_txn_commit(cmp_vfs_fh->vfs_fh_a);
    rc2 = vfs_txn_commit(cmp_vfs_fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_txn_abort(vfs_fh file_handle)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_txn_abort(cmp_vfs_fh->vfs_fh_a);
    rc2 = vfs_txn_abort(cmp_vfs_fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int cmp_checkpoint(vfs_fh file_handle)
{
    comparison_vfs_fh cmp_vfs_fh = (comparison_vfs_fh) file_handle;
    int rc1, rc2;
    rc1 = vfs_checkpoint(cmp_vfs_fh->vfs_fh_a);
    rc2 = vfs_checkpoint(cmp_vfs_fh->vfs_fh_b);
    if (rc1 != rc2) {
        printf("CMP FAILURE: %d %d\n", rc1, rc2);
    }
    return MAX(rc1, rc2);
}

int comparison_vfs_allocate(vfs *cmp_vfs, vfs vfs_a, vfs vfs_b)
{
    static struct vfs_info cmp_vfs_info = {
            sizeof(struct comparison_vfs),
            sizeof(struct comparison_vfs_fh),
            cmp_open,
            cmp_delete,
            cmp_exists,
            cmp_close,
            cmp_read,
            cmp_write,
            cmp_lock,
            cmp_file_size,
            cmp_truncate,
            cmp_freevfs
    };
    static struct tvfs_info cmp_tvfs_info = {
            cmp_txn_begin,
            cmp_txn_commit,
            cmp_txn_abort,
            cmp_checkpoint
    };
    comparison_vfs vfs = tcbl_malloc(NULL, sizeof(struct comparison_vfs));
    if (!vfs) {
        return TCBL_ALLOC_FAILURE;
    }
    vfs->vfs_info = &cmp_vfs_info;
    vfs->tvfs_info = &cmp_tvfs_info;
    vfs->vfs_a = vfs_a;
    vfs->vfs_b = vfs_b;

    *cmp_vfs = (struct vfs*) vfs;
    return TCBL_OK;
}