#include <sys/param.h>
#include <string.h>

#include "tcbl_vfs.h"
#include "runtime.h"
#include "sglib.h"

typedef struct tcbl_change_log_header {
    uint64_t page_size;
    uint64_t begin_lsn;
} *tcbl_change_log_header;

typedef struct tcbl_change_log {
    size_t offset;
    size_t newlen;
    union {
        uint64_t lsn;
        struct tcbl_change_log* next;
    };
    char data[0];
} *tcbl_change_log;

// Internal declarations
static int tcbl_txn_begin(vfs_fh file_handle);
static int tcbl_txn_commit(vfs_fh file_handle);
static int tcbl_txn_abort(vfs_fh file_handle);
static int tcbl_checkpoint(vfs_fh file_handle);

static size_t tcbl_log_filename_len(const char * file_name)
{
    return strlen(file_name) + 4;
}

static void gen_log_file_name(const char *file_name, char *log_file_name)
{
    strcpy(log_file_name, file_name);
    strcpy(&log_file_name[strlen(file_name)], "-log");
}

static int tcbl_log_open(vfs underlying_vfs, const char *file_name, size_t page_size, vfs_fh *log_fh)
{
    int rc;
    size_t log_fn_len = tcbl_log_filename_len(file_name);
    char log_file_name[log_fn_len + 1];
    gen_log_file_name(file_name, log_file_name);

    rc = vfs_open(underlying_vfs, log_file_name, log_fh);
    if (rc) {
        return rc;
    }

    size_t log_size;
    rc = vfs_file_size(*log_fh, &log_size);
    if (rc) {
        return rc;
    }

    if (log_size == 0) {
        struct tcbl_change_log_header h = {
                page_size,
                0 // begin_lsn
        };
        return vfs_write(*log_fh, (char*) &h, 0, sizeof(struct tcbl_change_log_header));
    } else {
        if ((log_size - sizeof(struct tcbl_change_log_header)) % (page_size + sizeof(struct tcbl_change_log)) != 0) {
            return TCBL_INVALID_LOG;
        }
    }
    return TCBL_OK;
}

static int tcbl_read_log_tail_header(vfs_fh log_file_handle, size_t log_page_size, tcbl_change_log log_entry, size_t *log_file_size)
{
    // TODO this should really perhaps be looking for the committed tail - recovery
    int rc;
    rc = vfs_file_size(log_file_handle, log_file_size);
    if (rc) {
        return rc;
    }
    if (*log_file_size == 0) {
        return TCBL_INVALID_LOG;
    }
    if ((*log_file_size - sizeof(struct tcbl_change_log_header)) % log_page_size != 0) {
        return TCBL_INVALID_LOG;
    }
    if (*log_file_size > sizeof(struct tcbl_change_log_header)) {
        return vfs_read(log_file_handle, (char *) log_entry, (*log_file_size) - log_page_size,
                        sizeof(struct tcbl_change_log));
    } else {
        return TCBL_OK;
    }
}

static int tcbl_log_get_latest(vfs_fh log_fh, size_t page_size, uint64_t *last_lsn, size_t *log_size)
{
    int rc;
    size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

    size_t current_log_size;

    struct tcbl_change_log last_log_entry;
    rc = tcbl_read_log_tail_header(log_fh, log_page_size, &last_log_entry, &current_log_size);
    if (rc) {
        return rc;
    }
    if (current_log_size == sizeof(struct tcbl_change_log_header)) {
        struct tcbl_change_log_header h;
        rc = vfs_read(log_fh, (char*) &h, 0, sizeof(struct tcbl_change_log_header));
        if (rc) {
            return rc;
        }
        *last_lsn = h.begin_lsn;
    } else {
        *last_lsn = last_log_entry.lsn;
    }
    *log_size = current_log_size;
    return TCBL_OK;
}

static int tcbl_read_log_tail_lsn_header(vfs_fh log_file_handle, size_t log_page_size, tcbl_change_log log_entry, uint64_t lsn, size_t *log_file_size, size_t *lsn_log_file_size)
{
    int rc;
    rc = vfs_file_size(log_file_handle, log_file_size);
    if (rc) {
        return rc;
    }
    if (*log_file_size < sizeof(struct tcbl_change_log_header)) {
        return TCBL_INVALID_LOG;
    }
    if (*log_file_size == sizeof(struct tcbl_change_log_header)) {
        struct tcbl_change_log_header h;
        rc = vfs_read(log_file_handle, (char *) & h, 0, sizeof(struct tcbl_change_log_header));
        if (rc) {
            return rc;
        }
        if (h.begin_lsn > lsn) {
            return TCBL_SNAPSHOT_EXPIRED;
        }
        *lsn_log_file_size = *log_file_size;
        return TCBL_OK;
    }
    if ((*log_file_size - sizeof(struct tcbl_change_log_header)) % log_page_size != 0) {
        return TCBL_INVALID_LOG;
    }
    size_t pos = (*log_file_size);
    do {
        pos -= log_page_size;
        rc = vfs_read(log_file_handle, (char *) log_entry, pos, sizeof(struct tcbl_change_log));
        if (rc) {
            return rc;
        }
        if (log_entry->lsn == lsn) {
            *lsn_log_file_size = pos + log_page_size;
            return TCBL_OK;
        } else if (log_entry->lsn < lsn) {
            // early exit - assumes lsn in order
            return TCBL_LOG_NOT_FOUND;
        }

    } while (pos >= log_page_size + sizeof(struct tcbl_change_log_header));
    *lsn_log_file_size = 0;
    return TCBL_OK;
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

    rc = tcbl_log_open(tcbl_vfs->underlying_vfs, file_name, tcbl_vfs->page_size, &underlying_log_fh);
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

static int tcbl_delete(vfs vfs, const char *file_name)
{
    int rc;
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    rc = vfs_delete(tcbl_vfs->underlying_vfs, file_name);
    if (rc) {
        return rc;
    }

    size_t log_fn_len = tcbl_log_filename_len(file_name);
    char log_file_name[log_fn_len + 1];
    gen_log_file_name(file_name, log_file_name);

    return vfs_delete(tcbl_vfs->underlying_vfs, log_file_name);
}

static int tcbl_exists(vfs vfs, const char *file_name, int *out)
{
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;
    return vfs_exists(tcbl_vfs->underlying_vfs, file_name, out);
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
    rc = TCBL_OK;
    while (read_offset < block_offset + block_len) {
        tcbl_change_log result;
        struct tcbl_change_log search_record = {
            read_offset
        };
        SGLIB_LIST_FIND_MEMBER(struct tcbl_change_log, fh->change_log, &search_record, tcbl_change_log_cmp, next, result);
        if (result) {
            memcpy(dst, &result->data[block_begin_skip], block_read_size);
        } else {
            // scan the log for this block - lots of things are bad about this
            // implementation: 1/ it scans, 2/ it scans once for each read block
            // 3/ it makes a copy for each time the block is encountered in the history.
            size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;
            char log_page_buff[log_page_size];
            tcbl_change_log log_page = (tcbl_change_log) log_page_buff;
            size_t log_read_pos = sizeof(struct tcbl_change_log_header);
            bool found_block = false;
            while (log_read_pos < fh->txn_begin_log_size) {
                // TODO check lsn too
                rc = vfs_read(fh->underlying_log_fh, log_page_buff, log_read_pos, log_page_size);
                if (rc) {
                    goto txn_end;
                }
                if (log_page->offset == read_offset) {
                    memcpy(dst, &log_page->data[block_begin_skip], block_read_size);
                    found_block = true;
                }
                log_read_pos += log_page_size;
            }
            // check the header for lsn
            struct tcbl_change_log_header h;
            rc = vfs_read(fh->underlying_log_fh, (char *) &h, 0, sizeof(struct tcbl_change_log_header));
            if (rc) {
                goto txn_end;
            }
            if (h.begin_lsn > fh->txn_shapshot_lsn) {
                rc = TCBL_SNAPSHOT_EXPIRED;
                goto txn_end;
            }
            // read the block from the underlying file system
            if (!rc && !found_block) {
                // TODO address extra copy when adding caching
                char buff[page_size];
                rc = vfs_read(fh->underlying_fh, buff, read_offset, page_size);
                if (!(rc == TCBL_OK || rc == TCBL_BOUNDS_CHECK)) {
                    goto txn_end;
                }
                memcpy(dst, &buff[block_begin_skip], block_read_size);
                rc = TCBL_OK;
            }
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
    size_t effective_log_size;
    if (fh->txn_active) {
        size_t current_lsn_log_size;
        rc = tcbl_read_log_tail_lsn_header(fh->underlying_log_fh, log_page_size, &last_log_entry, fh->txn_shapshot_lsn, &effective_log_size, &current_lsn_log_size);
        effective_log_size = current_lsn_log_size;
    } else {
        rc = tcbl_read_log_tail_header(fh->underlying_log_fh, log_page_size, &last_log_entry, &effective_log_size);
    }
    if (rc) {
        return rc;
    }
    if (effective_log_size > sizeof(struct tcbl_change_log_header)) {
        *out = last_log_entry.newlen;
        return TCBL_OK;
    }
    return vfs_file_size(fh->underlying_fh, out);
}

static int tcbl_truncate(vfs_fh file_handle, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

static int tcbl_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
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

    size_t starting_file_size;
    rc = tcbl_file_size(file_handle, &starting_file_size);


    if (!rc) {
        size_t write_offset = block_offset;
        const char* src = data;
        size_t block_begin_skip = alignment_shift;
        size_t block_write_size = MIN(len, page_size - alignment_shift);

        rc = TCBL_OK;
        while (write_offset < block_offset + block_len) {
            tcbl_change_log log_record = tcbl_malloc(NULL, sizeof(struct tcbl_change_log) + page_size);
            if (!log_record) {
                // TODO cleanup - should be taken care of by abort but make sure
                rc = TCBL_ALLOC_FAILURE;
                break;
            }
            log_record->offset = write_offset;
            log_record->newlen = MAX(write_offset + block_begin_skip + block_write_size, starting_file_size);
            if (block_begin_skip > 0 || block_begin_skip + block_write_size < page_size) {
                // TODO maybe we are reading a bit more than necessary here as some will be overwritten
                // but just pull in as much of the block as is available for now
                if (write_offset < starting_file_size) {
                    size_t read_len = MIN(page_size, starting_file_size - write_offset);
                    rc = tcbl_read(file_handle, log_record->data, write_offset, read_len);
                    if (rc) {
                        break;
                    }
                    if (read_len < page_size) {
                        // Sanitize the remainder of the block - in case we extend later
                        memset(&log_record->data[read_len], 0, page_size - read_len);
                    }
                } else {
                    memset(log_record->data, 0, page_size);
                }
            }
            memcpy(&log_record->data[block_begin_skip], src, block_write_size);
            SGLIB_LIST_ADD(struct tcbl_change_log, fh->change_log, log_record, next);
            src += block_write_size;
            write_offset += page_size;
            block_begin_skip = 0;
            block_write_size = MIN(page_size, offset + len - write_offset);
        }
    }

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

    rc = tcbl_log_get_latest(fh->underlying_log_fh, ((tcbl_vfs)fh->vfs)->page_size, &fh->txn_shapshot_lsn, &fh->txn_begin_log_size);
    if (rc) {
        return rc;
    }
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

    if (!fh->change_log) {
        rc = TCBL_OK;
    } else {
        size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
        size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

        uint64_t current_log_lsn;
        size_t current_log_size;

        rc = tcbl_log_get_latest(fh->underlying_log_fh, page_size, &current_log_lsn, &current_log_size);

        if (current_log_lsn != fh->txn_shapshot_lsn) {
            // abort for now TODO add ability to merge
            tcbl_txn_abort(file_handle);
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
    }
    fh->txn_active = false;
    // TODO this leaves us in a broken state if it fails
    return rc;
}

static int tcbl_txn_abort(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;

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

static int tcbl_checkpoint(vfs_fh file_handle)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;
    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    size_t log_page_size = sizeof(struct tcbl_change_log) + page_size;

    if (fh->txn_active) {
        return TCBL_TXN_ACTIVE;
    }

    size_t log_file_size;
    rc = vfs_file_size(fh->underlying_log_fh, &log_file_size);
    if (rc) {
        return rc;
    }
    if (log_file_size == sizeof(struct tcbl_change_log_header)) {
        return TCBL_OK;
    }
    if ((log_file_size - sizeof(struct tcbl_change_log_header)) % log_page_size != 0) {
        return TCBL_INVALID_LOG;
    }
    struct tcbl_change_log_header h;
    rc = vfs_read(fh->underlying_log_fh, (char *) &h, 0, sizeof(struct tcbl_change_log_header));
    if (rc) {
        return rc;
    }
    uint64_t last_lsn = h.begin_lsn;
    size_t log_offset = sizeof(struct tcbl_change_log_header);
    char buff[log_page_size];
    tcbl_change_log log_entry = (tcbl_change_log) buff;
    while (log_offset < log_file_size) {
        rc = vfs_read(fh->underlying_log_fh, (char *) log_entry, log_offset, log_page_size);
        if (rc) {
            return rc;
        }
        rc = vfs_write(fh->underlying_fh, log_entry->data, log_entry->offset, page_size);
        if (rc) {
            return rc;
        }
        if (log_entry->lsn < last_lsn) {
            return TCBL_INVALID_LOG;
        }
        last_lsn = log_entry->lsn;
        // TODO recovery - leaving invalid state during partial checkpoint
        log_offset += log_page_size;
    }
    h.begin_lsn = last_lsn;
    rc = vfs_write(fh->underlying_log_fh, (char *) &h, 0, sizeof(struct tcbl_change_log_header));
    if (rc) {
        return rc;
    }
    return vfs_truncate(fh->underlying_log_fh, sizeof(struct tcbl_change_log_header));
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
    tcbl_free(NULL, vfs, vfs->vfs_size);
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