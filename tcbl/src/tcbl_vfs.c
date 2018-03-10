#include <assert.h>
#include <sys/param.h>
#include <string.h>

#include "tcbl_vfs.h"
#include "runtime.h"
#include "sglib.h"

typedef struct tcbl_change_log {
    size_t offset;
    size_t newlen;
    union {
        u_int64_t lsn;
        struct tcbl_change_log* next;
    };
    char data[0];
} *tcbl_change_log;

// Internal declarations
static int tcbl_begin_txn(vfs_fh file_handle);
static int tcbl_commit_txn(vfs_fh file_handle);
static int tcbl_abort_txn(vfs_fh file_handle);

static int tcbl_read_log_tail_header(vfs_fh log_file_handle, size_t log_page_size, tcbl_change_log log_entry, size_t *log_file_size)
{
    int rc;
    rc = vfs_file_size(log_file_handle, log_file_size);
    if (rc) {
        return rc;
    }
    if (*log_file_size == 0) {
        return TCBL_OK;
    }
    if (*log_file_size % log_page_size != 0) {
        return TCBL_INVALID_LOG;
    }
    return vfs_read(log_file_handle, (char *) log_entry, (*log_file_size) - log_page_size, sizeof(struct tcbl_change_log));
}

static int tcbl_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    vfs_fh underlying_fh, underlying_log_fh;
    int rc;

    rc = vfs_open(tcbl_vfs->underlying_vfs, file_name, &underlying_fh);
    if (rc) {
        return rc;
    }

    char *log_fn_ext = "-log";
    size_t file_name_len = strlen(file_name);
    size_t log_fn_ext_len = strlen(log_fn_ext);
    char log_file_name[file_name_len + log_fn_ext_len + 1];
    memcpy(log_file_name, file_name, file_name_len);
    memcpy(&log_file_name[strlen(file_name)], log_fn_ext, log_fn_ext_len + 1);

    rc = vfs_open(tcbl_vfs->underlying_vfs, log_file_name, &underlying_log_fh);
    if (rc) {
        vfs_close(underlying_fh);
        return rc;
    }

    tcbl_fh fh = tcbl_malloc(NULL, sizeof(struct tcbl_fh));
    if (!fh) {
        vfs_close(underlying_fh);
        vfs_close(underlying_log_fh);
        return TCBL_ALLOC_FAILURE;
    }
    fh->underlying_fh = underlying_fh;
    fh->underlying_log_fh = underlying_log_fh;
    fh->vfs = vfs;
    fh->change_log = NULL;
    fh->txn_active = false;
    *file_handle_out = (vfs_fh) fh;
    return TCBL_OK;
}

static int tcbl_close(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    int rc1 = vfs_close(fh->underlying_fh);
    int rc2 = vfs_close(fh->underlying_log_fh);
    tcbl_free(NULL, fh, sizeof(struct tcbl_fh));
    return rc1 == TCBL_OK ? rc2 : rc1;
}

static int tcbl_change_log_cmp(tcbl_change_log l1, tcbl_change_log l2) {
    return l1->offset < l2->offset ? -1 : l1->offset == l2->offset ? 0 : 1;
}

static int tcbl_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;

    // Require alignment for now - TODO add unaligned later
    if (offset % page_size != 0 || len % page_size !=0) {
        return TCBL_BAD_ARGUMENT;
    }

    // If no transaction is active start one
    bool auto_txn = !fh->txn_active;
    if (auto_txn) {
        rc = tcbl_begin_txn((vfs_fh) fh);
        if (rc) {
            return rc;
        }
    }


    size_t read_offset = offset;
    char* dst = data;
    rc = TCBL_OK;
    while (read_offset < offset + len) {
        tcbl_change_log result;
        struct tcbl_change_log search_record = {
            read_offset
        };
        SGLIB_LIST_FIND_MEMBER(struct tcbl_change_log, fh->change_log, &search_record, tcbl_change_log_cmp, next, result);
        if (result) {
            memcpy(dst, result->data, page_size);
        } else {
            // scan the log for this block - lots of things are bad about this
            // implementation: 1/ it scans, 2/ it scans once for each read block
            // 3/ it makes a copy for each time the block is encountered in the history.
            size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;
            char log_page_buff[log_page_size];
            tcbl_change_log log_page = (tcbl_change_log) log_page_buff;
            size_t log_read_pos = 0;
            bool found_block = false;
            while (log_read_pos < fh->txn_begin_log_size) {
                // TODO check lsn too
                rc = vfs_read(fh->underlying_log_fh, log_page_buff, log_read_pos, log_page_size);
                if (rc) {
                    break;
                }
                if (log_page->offset == read_offset) {
                    memcpy(dst, log_page->data, page_size);
                    found_block = true;
                }
                log_read_pos += log_page_size;
            }
            // read the block from the underlying file system
            if (!rc && !found_block) {
                rc = vfs_read(fh->underlying_fh, data, offset, len);
            }
        }
        dst += page_size;
        read_offset += page_size;
    }

    if (auto_txn) {
        // skip rc assignment on this
        tcbl_abort_txn((vfs_fh) fh);
    }
    // TODO - this fragments large reads into many smaller ones. Should not do that.
    return rc;
}

static int tcbl_file_size(vfs_fh file_handle, size_t* out)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    if (fh->change_log) {
        *out = fh->change_log->newlen;
        return TCBL_OK;
    }

    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

    struct tcbl_change_log last_log_entry;
    size_t current_log_size;
    rc = tcbl_read_log_tail_header(fh->underlying_log_fh, log_page_size, &last_log_entry, &current_log_size);
    if (rc) {
        return rc;
    }
    if (current_log_size > 0) {
        *out = last_log_entry.newlen;
        return TCBL_OK;
    }
    return vfs_file_size(fh->underlying_fh, out);
}

static int tcbl_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;

    // Require alignment for now - TODO add unaligned later
    if (offset % page_size != 0 || len % page_size !=0) {
        return TCBL_BAD_ARGUMENT;
    }

    // If no transaction is active start one
    bool auto_txn = !fh->txn_active;
    if (auto_txn) {
        rc = tcbl_begin_txn((vfs_fh) fh);
        if (rc) {
            return rc;
        }
    }

    size_t starting_file_size;
    rc = tcbl_file_size(file_handle, &starting_file_size);

    if (!rc) {
        size_t page_offset = offset;
        const char *src_ptr = data;
        rc = TCBL_OK;
        while (src_ptr < data + len) {
            tcbl_change_log log_record = tcbl_malloc(NULL, sizeof(struct tcbl_change_log) + page_size);
            if (!log_record) {
                // TODO cleanup - should be taken care of by abort but make sure
                rc = TCBL_ALLOC_FAILURE;
                break;
            }
            log_record->offset = page_offset;
            log_record->newlen = MAX(page_offset + page_size, starting_file_size);
            memcpy(log_record->data, src_ptr, page_size);
            SGLIB_LIST_ADD(struct tcbl_change_log, fh->change_log, log_record, next);
            page_offset += page_size;
            src_ptr += page_size;
        }
    }

    if (auto_txn) {
        if (rc) {
            tcbl_abort_txn((vfs_fh) fh);
            return rc;
        } else {
            return tcbl_commit_txn((vfs_fh) fh);
        }
    } else {
        // TODO if error still need to go into bad state
    }
    return TCBL_OK;
}


static int tcbl_begin_txn(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    if (fh->txn_active) {
        return TCBL_TXN_ACTIVE;
    }

    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

    // TODO - we get the LSN for reads here.
    size_t current_log_size;

    struct tcbl_change_log last_log_entry;
    rc = tcbl_read_log_tail_header(fh->underlying_log_fh, log_page_size, &last_log_entry, &current_log_size);
    if (rc) {
        return rc;
    }
    fh->txn_shapshot_lsn = current_log_size > 0 ? last_log_entry.lsn : 0;
    fh->txn_begin_log_size = current_log_size;
    fh->txn_active = true;
    return TCBL_OK;
}

static int tcbl_commit_txn(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;

    if (!fh->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

    struct tcbl_change_log last_log_entry;
    size_t current_log_size;

    rc = tcbl_read_log_tail_header(fh->underlying_log_fh, log_page_size, &last_log_entry, &current_log_size);
    // TODO only need to read the header here

    if (current_log_size != fh->txn_begin_log_size ||
            (current_log_size > 0 && fh->txn_shapshot_lsn != last_log_entry.lsn)) {
        // abort for now TODO add ability to merge
        tcbl_abort_txn(file_handle);
        return TCBL_CONFLICT_ABORT;
    }

    uint64_t txn_commit_lsn = fh->txn_shapshot_lsn + 1;
    SGLIB_LIST_REVERSE(struct tcbl_change_log, fh->change_log, next);
    while (!rc && fh->change_log) {
        // note, pages are written in backwards order
        // TODO race condition possible where log is partially written
        tcbl_change_log l = fh->change_log;
        SGLIB_LIST_DELETE(struct tcbl_change_log, fh->change_log, l, next);
        l->lsn = txn_commit_lsn;
        rc = vfs_write(fh->underlying_log_fh, (char *) l, current_log_size, log_page_size);
        current_log_size += log_page_size;
        tcbl_free(NULL, l, sizeof(struct tcbl_change_log) + page_size);
    }
    fh->txn_active = false;
    // TODO this leaves us in a broken state if it fails
    return rc;
}

static int tcbl_abort_txn(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;

    if (!fh->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    while (fh->change_log) {
        tcbl_change_log l = fh->change_log;
        SGLIB_LIST_DELETE(struct tcbl_change_log, fh->change_log, l, next);
        tcbl_free(NULL, l, sizeof(struct tcbl_change_log) + page_size);
    }
    fh->txn_active = false;
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

int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs, size_t page_size)
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
    tcbl_vfs->page_size = page_size;
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
