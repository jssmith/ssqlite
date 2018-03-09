#include <assert.h>
#include "tcbl_vfs.h"
#include "runtime.h"


static int tcbl_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    vfs_fh underlying_fh;
    int rc;
    rc = vfs_open(tcbl_vfs->underlying_vfs, file_name, &underlying_fh);
    if (rc) {
        return rc;
    }
    tcbl_fh fh = tcbl_malloc(NULL, sizeof(struct tcbl_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->underlying_fh = underlying_fh;
    fh->vfs = vfs;
    *file_handle_out = (vfs_fh) fh;
    return TCBL_OK;
}

static int tcbl_close(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    int rc = vfs_close(fh->underlying_fh);
    tcbl_free(NULL, fh, sizeof(struct tcbl_fh));
    return rc;
}

static int tcbl_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    return vfs_read(fh->underlying_fh, data, offset, len);
}

static int tcbl_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    return vfs_write(fh->underlying_fh, data, offset, len);
}

static int tcbl_file_size(vfs_fh file_handle, size_t* out)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    return vfs_file_size(fh->underlying_fh, out);
}

static int tcbl_begin_txn(vfs_fh vfs_fh)
{
//    tcbl_txn txn = tcbl_malloc(NULL, sizeof(struct tcbl_txn));
//    if (!txn) {
//        return TCBL_ALLOC_FAILURE;
//    }
//    txn->vfs = tvfs;
//    *vfs_txn = (struct vfs_txn *) txn;
    return TCBL_OK;
}

static int tcbl_commit_txn(vfs_fh vfs_fh)
{
//    tcbl_txn txn = (tcbl_txn) vfs_txn;
//    tcbl_free(NULL, txn, sizeof(struct tcbl_txn));
    return TCBL_OK;
}

static int tcbl_abort_txn(vfs_fh vfs_fh)
{
//    tcbl_txn txn = (tcbl_txn) vfs_txn;
//    tcbl_free(NULL, txn, sizeof(struct tcbl_txn));
    return TCBL_OK;
}


static int tcbl_freevfs(vfs vfs)
{
    return TCBL_OK;
}

int vfs_free(vfs vfs)
{
    if (vfs->vfs_info->x_free) {
        vfs->vfs_info->x_free(vfs);
    }
    tcbl_free(NULL, vfs, vfs->vfs_size);
    return TCBL_OK;
}

int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs)
{
    static struct vfs_info tcbl_vfs_info = {
            sizeof(struct tcbl_vfs),
            sizeof(struct tcbl_fh),
            &tcbl_open,
            &tcbl_close,
            &tcbl_read,
            &tcbl_write,
            &tcbl_file_size,
            &tcbl_freevfs
    };
    static struct tvfs_info tcbl_tvfs_info = {
            sizeof(struct tcbl_txn),
            &tcbl_begin_txn,
            &tcbl_commit_txn,
            &tcbl_abort_txn
    };
    tcbl_vfs tcbl_vfs = tcbl_malloc(NULL, sizeof(struct tcbl_vfs));
    if (!tcbl_vfs) {
        return TCBL_ALLOC_FAILURE;
    }
    tcbl_vfs->vfs_info = &tcbl_vfs_info;
    tcbl_vfs->tvfs_info = &tcbl_tvfs_info;
    tcbl_vfs->underlying_vfs = underlying_vfs;
    *tvfs = (struct tvfs *) tcbl_vfs;
    return TCBL_OK;
}


int vfs_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    return vfs->vfs_info->x_open(vfs, file_name, file_handle_out);
}

int vfs_close(vfs_fh file_handle)
{
    return file_handle->vfs->vfs_info->x_close(file_handle);
}

int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_read(file_handle, data, offset, len);
}

int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_write(file_handle, data, offset, len);
}

int vfs_file_size(vfs_fh file_handle, size_t *out)
{
    return file_handle->vfs->vfs_info->x_file_size(file_handle, out);
}


int vfs_txn_begin(vfs_fh vfs_fh)
{
    return ((tvfs)(vfs_fh->vfs))->tvfs_info->x_begin_txn(vfs_fh);
}

int vfs_txn_commit(vfs_fh vfs_fh)
{
    return ((tvfs)(vfs_fh->vfs))->tvfs_info->x_commit_txn(vfs_fh);
}

int vfs_txn_abort(vfs_fh vfs_fh)
{
    return ((tvfs)(vfs_fh->vfs))->tvfs_info->x_abort_txn(vfs_fh);
}
