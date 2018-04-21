#include "test_tcbl.h"

int cfh_read(vfs_fh fh, void *data, size_t offset, size_t len)
{
    change_fh cfh = (change_fh) fh;
    return vfs_read(cfh->orig_fh, data, offset, len);
}

int cfh_write(vfs_fh fh, const void *data, size_t offset, size_t len)
{
    int rc;
    change_fh cfh = (change_fh) fh;
    rc = cfh->env->before_change(cfh->orig_fh);
    if (rc) {
        return rc;
    }
    rc = vfs_write(cfh->orig_fh, data, offset, len);
    if (rc) {
        return rc;
    }
    return cfh->env->after_change(cfh->orig_fh);
}

int cfh_lock(vfs_fh fh, int lock_operation)
{
    change_fh cfh = (change_fh) fh;
    return vfs_lock(cfh->orig_fh, lock_operation);
}

int cfh_file_size(vfs_fh fh, size_t* size_out)
{
    change_fh cfh = (change_fh) fh;
    return vfs_file_size(cfh->orig_fh, size_out);
}

int cfh_truncate(vfs_fh fh, size_t len)
{
    int rc;
    change_fh cfh = (change_fh) fh;
    rc = cfh->env->before_change(cfh->orig_fh);
    if (rc) {
        return rc;
    }
    rc = vfs_truncate(cfh->orig_fh, len);
    if (rc) {
        return rc;
    }
    return cfh->env->after_change(cfh->orig_fh);
}

void create_change_fh(test_env env, struct change_fh *fhout)
{
    static struct vfs_info cfh_vfs_info = {
            sizeof(struct change_fh),
            0, // should never instantiate a vfs
            NULL,
            NULL,
            NULL,
            NULL,
            cfh_read,
            cfh_write,
            cfh_lock,
            cfh_file_size,
            cfh_truncate,
            NULL
    };
    static struct tvfs_info cfh_tvfs_info = {
            NULL,
            NULL,
            NULL,
            NULL
    };

    fhout->vfs_fh.vfs = (vfs) &(fhout->change_vfs);
    fhout->orig_fh = env->fh[0];
    fhout->env = env;
    fhout->change_vfs.vfs_info = &cfh_vfs_info;
    fhout->change_vfs.tvfs_info = &cfh_tvfs_info;
}
