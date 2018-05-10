#include <sys/param.h>
#include <string.h>

#include "tcbl_vfs.h"
#include "runtime.h"
#include "sglib.h"

typedef struct tcbl_change_log_header {
    uint64_t page_size;
    uint64_t checkpoint_active_id;
    uint64_t checkpoint_started_ts;
    uint64_t checkpoint_begin_lsn;
    uint64_t checkpoint_end_lsn;
    int active_log_file;

} *tcbl_change_log_header;

typedef struct tcbl_change_log_file_header {
    uint64_t page_size;
    uint64_t checkpoint_active_id;
    uint64_t checkpoint_started_ts;
    uint64_t checkpoint_begin_lsn;
    uint64_t checkpoint_end_lsn;
    int active_log_file;

} *tcbl_change_log_file_header;

// Internal declarations
static int tcbl_txn_begin(vfs_fh file_handle);
static int tcbl_txn_commit(vfs_fh file_handle);
static int tcbl_txn_abort(vfs_fh file_handle);
static int tcbl_checkpoint(vfs_fh file_handle);

static int tcbl_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    int rc;
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;

    tcbl_fh fh = tcbl_malloc(NULL, sizeof(struct tcbl_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = vfs;
    fh->underlying_fh = NULL;
    fh->txn_active = NULL;

    rc = vfs_open(tcbl_vfs->underlying_vfs, file_name, &fh->underlying_fh);
    if (rc) goto exit;

    if (tcbl_vfs->cache != NULL) {
        rc = vfs_cache_open(tcbl_vfs->cache, &fh->cache_h, fh->underlying_fh);
        if (rc) goto exit;
    } else {
        fh->cache_h = NULL;
    }

    rc = bc_log_create(&fh->txn_log, tcbl_vfs->underlying_vfs, fh->underlying_fh, fh->cache_h, file_name, tcbl_vfs->page_size);
    if (rc) goto exit;

    *file_handle_out = (vfs_fh) fh;

    exit:
    if (rc && fh->underlying_fh) {
        vfs_close(fh->underlying_fh);
    }
    if (rc && fh->cache_h) {
        vfs_cache_close(fh->cache_h);
    }
    return rc;
}

static int tcbl_delete(vfs vfs, const char *file_name)
{
    int rc;
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    rc = vfs_delete(tcbl_vfs->underlying_vfs, file_name);
    if (rc) {
        return rc;
    }

    return bc_log_delete(tcbl_vfs->underlying_vfs, file_name);
}

static int tcbl_exists(vfs vfs, const char *file_name, int *out)
{
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    return vfs_exists(tcbl_vfs->underlying_vfs, file_name, out);
}

static int tcbl_close(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    int rc = vfs_close(fh->underlying_fh);
    int rc_ = TCBL_OK;
    if (fh->cache_h) {
        rc_ = vfs_cache_close(fh->cache_h);
    }
    if (!rc && rc_) rc = rc_;
    if (fh->txn_active) {
        rc_ = bc_log_txn_abort(&fh->txn_log_h);
    }
    if (!rc && rc_) rc = rc_;
    tcbl_free(NULL, fh, sizeof(struct tcbl_fh));
    return rc;
}

static int tcbl_read(vfs_fh file_handle, void* data, size_t offset, size_t len, size_t *out_len)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;

    size_t alignment_shift = offset % page_size;
    size_t block_offset = offset - alignment_shift;
    size_t block_len = ((offset + len - 1) / page_size + 1) * page_size - block_offset;

    // If no transaction is active start one
    bool auto_txn = !fh->txn_active;
    if (auto_txn) {
        rc = tcbl_txn_begin((vfs_fh) fh);
        if (rc) {
            return rc;
        }
    }

    size_t read_offset = block_offset;
    char* dst = data;
    size_t block_begin_skip = alignment_shift;
    size_t block_read_size = MIN(len, page_size - alignment_shift);
    bool bounds_error = false;
    char buff[page_size];
    rc = TCBL_OK;
    size_t total_read_len = 0;
    while (read_offset < block_offset + block_len) {
        bool found_data;
        void *p;
        size_t found_newlen;
        rc = bc_log_read(&fh->txn_log_h, read_offset, &found_data, &p, &found_newlen);
        if (rc) goto txn_end;
        if (found_data) {
            memcpy(dst, &((char *) p)[block_begin_skip], block_read_size);
            total_read_len += MIN(block_read_size, found_newlen - block_offset - block_begin_skip);
            if (found_newlen < block_offset + block_begin_skip + block_read_size) {
                bounds_error = true;
            } else {
                bounds_error = false;
            }
        } else {
            size_t read_len;
            if (fh->cache_h != NULL) {
                rc = vfs_cache_get(fh->cache_h, buff, read_offset, page_size, &read_len);
            } else {
                rc = vfs_read_2(fh->underlying_fh, buff, read_offset, page_size, &read_len);
            }
            if (rc == TCBL_BOUNDS_CHECK) {
                // TODO understand where this condition arises and document it
                size_t underlying_size;
                if (fh->cache_h != NULL) {
                    rc = vfs_cache_len_get(fh->cache_h, &underlying_size);
                } else {
                    vfs_file_size(fh->underlying_fh, &underlying_size);
                }
                if (underlying_size < read_offset + block_begin_skip + block_read_size) {
                    bounds_error = true;
                } else {
                    bounds_error = false;
                }
            } else if (rc == TCBL_OK) {
                bounds_error = false;
            }
            if (!(rc == TCBL_OK || rc == TCBL_BOUNDS_CHECK)) {
                goto txn_end;
            }
            total_read_len += read_len;
            memcpy(dst, &buff[block_begin_skip], block_read_size);
        }

        dst += block_read_size;
        read_offset += page_size;
        block_begin_skip = 0;
        block_read_size = MIN(page_size, offset + len - read_offset);
    }

    txn_end:
    if (auto_txn) {
        // skip rc assignment on this
        tcbl_txn_abort((vfs_fh) fh);
    }
    // TODO - this fragments large reads into many smaller ones. Should not do that.
    if (!((rc == TCBL_OK) || (rc == TCBL_BOUNDS_CHECK))) {
        return rc;
    } else {
        if (out_len != NULL) {
            *out_len = total_read_len;
        }
        return bounds_error ? TCBL_BOUNDS_CHECK : TCBL_OK;
    }
}

static int tcbl_file_size(vfs_fh file_handle, size_t* out_size)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;

    bool auto_txn = !fh->txn_active;
    if (auto_txn) {
        rc = tcbl_txn_begin((vfs_fh) fh);
        if (rc) return rc;
    }

    bool found_size = false;

    rc = bc_log_length(&fh->txn_log_h, &found_size, out_size);
    if (rc) goto exit;

    if (!found_size) {
        if (fh->cache_h) {
            rc = vfs_cache_len_get(fh->cache_h, out_size);
        } else {
            rc = vfs_file_size(fh->underlying_fh, out_size);
        }
    }
    exit:
    if (auto_txn) {
        tcbl_txn_abort((vfs_fh) fh);
    }
    return rc;
}

static int tcbl_truncate(vfs_fh file_handle, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

static int tcbl_lock(vfs_fh file_handle, int lock_operation)
{
    return TCBL_NOT_IMPLEMENTED;
}

static int tcbl_write(vfs_fh file_handle, const void* data, size_t offset, size_t len)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    char buff[page_size];

    size_t alignment_shift = offset % page_size;
    size_t block_offset = offset - alignment_shift;
    size_t block_len = ((offset + len - 1) / page_size + 1) * page_size - block_offset;

    // If no transaction is active start one
    bool auto_txn = !fh->txn_active;
    if (auto_txn) {
        rc = tcbl_txn_begin((vfs_fh) fh);
        if (rc) {
            return rc;
        }
    }

    size_t starting_file_size;
    rc = tcbl_file_size(file_handle, &starting_file_size);
    if (rc) goto txn_end;

    size_t write_offset = block_offset;
    const char* src = data;
    size_t block_begin_skip = alignment_shift;
    size_t block_write_size = MIN(len, page_size - alignment_shift);

    rc = TCBL_OK;
    while (write_offset < block_offset + block_len) {
        size_t newlen = MAX(write_offset + block_begin_skip + block_write_size, starting_file_size);
        if (block_begin_skip > 0 || block_begin_skip + block_write_size < page_size) {
            // TODO maybe we are reading a bit more than necessary here as some will be overwritten
            // but just pull in as much of the block as is available for now
            if (write_offset < starting_file_size) {
                size_t read_len = MIN(page_size, starting_file_size - write_offset);
                rc = tcbl_read(file_handle, buff, write_offset, read_len, NULL);
                if (rc == TCBL_BOUNDS_CHECK) {
                    // trust that read has provided everything that it can and
                    // zeroed out the rest
                    rc = TCBL_OK;
                } else if (rc) {
                    break;
                }
                if (read_len < page_size) {
                    // Sanitize the remainder of the block - in case we extend later
                    memset(&buff[read_len], 0, page_size - read_len);
                }
            } else {
                memset(buff, 0, page_size);
            }
        }
        memcpy(&buff[block_begin_skip], src, block_write_size);
        bc_log_write(&fh->txn_log_h, write_offset, buff, newlen);

        src += block_write_size;
        write_offset += page_size;
        block_begin_skip = 0;
        block_write_size = MIN(page_size, offset + len - write_offset);
    }

    txn_end:
    if (auto_txn) {
        if (rc) {
            tcbl_txn_abort((vfs_fh) fh);
            return rc;
        } else {
            return tcbl_txn_commit((vfs_fh) fh);
        }
    } else {
        // TODO if error still need to go into bad state
    }
    return rc;
}

static int tcbl_txn_begin(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    if (fh->txn_active) {
        return TCBL_TXN_ACTIVE;
    }

    rc = bc_log_txn_begin(&fh->txn_log, &fh->txn_log_h);
    if (rc) return rc;

    fh->txn_active = true;
    return TCBL_OK;
}

static int tcbl_txn_commit(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;

    if (!fh->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    rc = bc_log_txn_commit(&fh->txn_log_h);
    fh->txn_active = false;
    return rc;
}

static int tcbl_txn_abort(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;

    if (!fh->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    rc = bc_log_txn_abort(&fh->txn_log_h);
    fh->txn_active = false;
    return rc;
}

static int tcbl_checkpoint(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;

    if (fh->txn_active) {
        // There may be no fundamental reason for not doing a checkpoint while
        // having a transaction active but it doesn't seem useful and has potential
        // for bugs.
        return TCBL_TXN_ACTIVE;
    }
    return bc_log_checkpoint(&fh->txn_log);
}

static int tcbl_freevfs(vfs vfs)
{
    return TCBL_OK; // No state yet
}

int vfs_free(vfs vfs)
{
    if (vfs->vfs_info->x_free) {
        vfs->vfs_info->x_free(vfs);
    }
    tcbl_free(NULL, vfs, vfs->vfs_info->vfs_obj_size);
    return TCBL_OK;
}

int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs, size_t page_size)
{
    static struct vfs_info tcbl_vfs_info = {
            sizeof(struct tcbl_vfs),
            sizeof(struct tcbl_fh),
            tcbl_open,
            tcbl_delete,
            tcbl_exists,
            tcbl_close,
            tcbl_read,
            tcbl_write,
            tcbl_lock,
            tcbl_file_size,
            tcbl_truncate,
            tcbl_freevfs
    };
    static struct tvfs_info tcbl_tvfs_info = {
            tcbl_txn_begin,
            tcbl_txn_commit,
            tcbl_txn_abort,
            tcbl_checkpoint
    };
    tcbl_vfs tcbl_vfs = tcbl_malloc(NULL, sizeof(struct tcbl_vfs));
    if (!tcbl_vfs) {
        return TCBL_ALLOC_FAILURE;
    }
    tcbl_vfs->vfs_info = &tcbl_vfs_info;
    tcbl_vfs->tvfs_info = &tcbl_tvfs_info;
    tcbl_vfs->underlying_vfs = underlying_vfs;
    tcbl_vfs->page_size = page_size;
    tcbl_vfs->cache = NULL;
    *tvfs = (struct tvfs *) tcbl_vfs;
    return TCBL_OK;
}


int vfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out)
{
    return vfs->vfs_info->x_open(vfs, file_name, file_handle_out);
}

int vfs_delete(vfs vfs, const char* file_name) {
    return vfs->vfs_info->x_delete(vfs, file_name);
}

int vfs_exists(vfs vfs, const char *file_name, int *out)
{
    return vfs->vfs_info->x_exists(vfs, file_name, out);
}

int vfs_close(vfs_fh file_handle)
{
    return file_handle->vfs->vfs_info->x_close(file_handle);
}

int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_read(file_handle, data, offset, len, NULL);
}

int vfs_read_2(vfs_fh file_handle, char* data, size_t offset, size_t len, size_t *out_len)
{
    return file_handle->vfs->vfs_info->x_read(file_handle, data, offset, len, out_len);
}

int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_info->x_write(file_handle, data, offset, len);
}

int vfs_lock(vfs_fh file_handle, int lock_operation)
{
    return file_handle->vfs->vfs_info->x_lock(file_handle, lock_operation);
}

int vfs_file_size(vfs_fh file_handle, size_t *out)
{
    return file_handle->vfs->vfs_info->x_file_size(file_handle, out);
}

int vfs_truncate(vfs_fh file_handle, size_t len)
{
    return file_handle->vfs->vfs_info->x_truncate(file_handle, len);
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

int vfs_checkpoint(vfs_fh vfs_fh)
{
    return ((tvfs)(vfs_fh->vfs))->tvfs_info->x_checkpoint(vfs_fh);
}