#include "tcbl_vfs.h"
#include "runtime.h"


int tcbl_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
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

int tcbl_close(vfs vfs, vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    int rc = vfs_close(fh->underlying_fh);
    tcbl_free(NULL, fh, sizeof(struct tcbl_fh));
    return rc;
}

int tcbl_read(vfs vfs, vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    return vfs_read(fh->underlying_fh, data, offset, len);
}

int tcbl_write(vfs vfs, vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    return vfs_write(fh->underlying_fh, data, offset, len);
}


int tcbl_begin_txn(tvfs tvfs, vfs_txn *vfs_txn)
{
    tcbl_txn txn = tcbl_malloc(NULL, sizeof(struct tcbl_txn));
    if (!txn) {
        return TCBL_ALLOC_FAILURE;
    }
    txn->vfs = tvfs;
    *vfs_txn = (struct vfs_txn *) txn;
    return TCBL_OK;
}
int tcbl_commit_txn(tvfs tvfs, vfs_txn vfs_txn)
{
    tcbl_txn txn = (tcbl_txn) vfs_txn;
    tcbl_free(NULL, txn, sizeof(tcbl_txn));
    return TCBL_OK;
}

int tcbl_abort_txn(tvfs tvfs, vfs_txn vfs_txn)
{
    tcbl_txn txn = (tcbl_txn) vfs_txn;
    tcbl_free(NULL, txn, sizeof(tcbl_txn));
    return TCBL_OK;
}


int tcbl_freevfs(vfs vfs)
{
    return TCBL_NOT_IMPLEMENTED;
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
    return file_handle->vfs->vfs_info->x_close(file_handle->vfs, file_handle);
}

int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_read(file_handle->vfs, file_handle, data, offset, len);
}

int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_write(file_handle->vfs, file_handle, data, offset, len);
}

int vfs_begin_txn(tvfs vfs, vfs_txn *txn)
{
    if (vfs->tvfs_info->x_begin_txn) {
        return (vfs->tvfs_info->x_begin_txn(vfs, txn));
    } else {
        return TCBL_NOT_IMPLEMENTED;
    }
}

int vfs_commit_txn(vfs_txn txn)
{
    if (txn->vfs->tvfs_info->x_commit_txn) {
        return txn->vfs->tvfs_info->x_commit_txn(txn->vfs, txn);
    } else {
        return TCBL_NOT_IMPLEMENTED;
    }
}

int vfs_abort_txn(vfs_txn txn)
{
    if (txn->vfs->tvfs_info->x_abort_txn) {
        return txn->vfs->tvfs_info->x_abort_txn(txn->vfs, txn);
    } else {
        return TCBL_NOT_IMPLEMENTED;
    }
}
