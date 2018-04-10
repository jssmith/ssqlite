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

struct tlog {
    size_t page_size;
    struct tlog_ops ops;
    const char* file_name;
    vfs_fh root_fh;
};

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

int tlog_delete_v1(tlog log)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_close_v1(tlog log)
{
    return TCBL_OK;
}

int tlog_entry_ct_v1(tlog log, uint64_t *log_entry_ct_out)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_begin_v1(tlog log)
{

}

int tlog_commit_v1(tlog log)
{

}


int tlog_find_block_v1(tlog log, bool *found_block, change_log_entry change_record, uint64_t log_entry_ct, size_t offset)
{
    /*
    int rc;

    // Read from the underlying file system
    // scan the log for this block - lots of things are bad about this
    // implementation: 1/ it scans, 2/ it scans once for each read block
    // 3/ it makes a copy for each time the block is encountered in the history.
    size_t log_page_size = sizeof(struct tcbl_change_log_entry) + page_size;
    char log_page_buff[log_page_size];
    tcbl_change_log_entry log_page = (tcbl_change_log_entry) log_page_buff;
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
            if (log_page->newlen < offset + len) {
                bounds_error = true;
            } else {
                bounds_error = false;
            }
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
    }
    */
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_file_size_v1(tlog log, bool *found_size, size_t *size_out, uint64_t log_entry_ct)
{
    /*

    size_t page_size = ((tcbl_vfs) fh->vfs)->page_size;
    size_t log_page_size = sizeof(struct tcbl_change_log_entry) + page_size;

    struct tcbl_change_log_entry last_log_entry;
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

     */
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_append_v1(tlog log, void* change_records)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_checkpoint_v1(tlog log, vfs_fh underlying_fh)
{
    /*
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
        printf("failed read 1\n");
        return rc;
    }
    uint64_t last_lsn = h.begin_lsn;
    size_t log_offset = sizeof(struct tcbl_change_log_header);
    char buff[log_page_size];
    tcbl_change_log_entry log_entry = (tcbl_change_log_entry) buff;
    while (log_offset < log_file_size) {
        rc = vfs_read(fh->underlying_log_fh, (char *) log_entry, log_offset, log_page_size);
        if (rc) {
            printf("failed read 2\n");
            return rc;
        }
        rc = vfs_write(fh->underlying_fh, log_entry->data, log_entry->offset, MIN(page_size, log_entry->newlen - log_entry->offset));
        if (rc) {
            printf("failed write 1\n");
            return rc;
        }
        if (log_entry->lsn < last_lsn) {
            printf("failed lsn 1\n");
            return TCBL_INVALID_LOG;
        }
        last_lsn = log_entry->lsn;
        // TODO recovery - leaving invalid state during partial checkpoint
        log_offset += log_page_size;
    }
    h.begin_lsn = last_lsn;
    rc = vfs_write(fh->underlying_log_fh, (char *) &h, 0, sizeof(struct tcbl_change_log_header));
    if (rc) {
        printf("failed write 2\n");
        return rc;
    }
    rc = vfs_truncate(fh->underlying_log_fh, sizeof(struct tcbl_change_log_header));
    if (rc) {
        printf("failed truncate\n");
        return rc;
    }
     */
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_free_v1(tlog log)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tlog_open_v1(vfs vfs, const char *file_name, size_t page_size, tlog *tlog)
{
    struct tlog *log = tcbl_malloc(NULL, sizeof(struct tlog));
    if (!tlog) {
        return TCBL_ALLOC_FAILURE;
    }
    log->file_name = file_name; // TODO maybe we should make a copy
    log->ops = (struct tlog_ops) {
            tlog_begin_v1,
            tlog_commit_v1,
            tlog_delete_v1,
            tlog_close_v1,
            tlog_entry_ct_v1,
            tlog_find_block_v1,
            tlog_file_size_v1,
            tlog_append_v1,
            tlog_checkpoint_v1,
            tlog_free
    };
    log->page_size = page_size;
    *tlog = log;
    return TCBL_OK;
}

int tlog_txn_begin(tlog log)
{
    return log->ops.x_txn_begin(log);
}
int tlog_txn_commit(tlog log)
{
    return log->ops.x_txn_commit(log);
}

int tlog_delete(tlog log)
{
    return log->ops.x_delete(log);
}

int tlog_close(tlog log)
{
    return log->ops.x_close(log);
}

int tlog_entry_ct(tlog log, uint64_t *log_entry_ct_out)
{
    // the entry count comes from the header
    char *log_header;
    vfs_read(log->root_fh, log_header, 0, sizeof(struct tcbl_change_log_header));
//    ((tcbl_change_log_header) log_header)->
    return log->ops.x_entry_ct(log, log_entry_ct_out);
}

int tlog_find_block(tlog log, bool *found_block, change_log_entry change_record, uint64_t log_entry_ct, size_t offset)
{
    return log->ops.x_find_block(log, found_block, change_record, log_entry_ct, offset);
}

int tlog_file_size(tlog log, bool *found_size, size_t *size_out, uint64_t log_entry_ct)
{
    return log->ops.x_file_size(log, found_size, size_out, log_entry_ct);
}

int tlog_append(tlog log, change_log_entry change_records)
{
    return log->ops.x_append(log, change_records);
}

int tlog_checkpoint(tlog log, vfs_fh underlying_fh)
{
    return log->ops.x_checkpoint(log, underlying_fh);
}

int tlog_free(tlog log)
{
    return log->ops.x_free(log);
}


/*
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
        if ((log_size - sizeof(struct tcbl_change_log_header)) % (page_size + sizeof(struct change_log_entry)) != 0) {
            return TCBL_INVALID_LOG;
        }
    }
    return TCBL_OK;
}


static int tcbl_read_log_tail_header(vfs_fh log_file_handle, size_t log_page_size, change_log_entry log_entry, size_t *log_file_size)
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
                        sizeof(struct change_log_entry));
    } else {
        return TCBL_OK;
    }
}

static int tcbl_log_get_latest(vfs_fh log_fh, size_t page_size, uint64_t *last_lsn, size_t *log_size)
{
    int rc;
    size_t log_page_size = sizeof(struct change_log_entry) + page_size;

    size_t current_log_size;

    struct change_log_entry last_log_entry;
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

//int tlog_file_size(tlog, bool *found_size, size_t size_out, uint64_t lsn);
static int tcbl_read_log_tail_lsn_header(vfs_fh log_file_handle, size_t log_page_size, change_log_entry log_entry, uint64_t lsn, size_t *log_file_size, size_t *lsn_log_file_size)
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
        rc = vfs_read(log_file_handle, (char *) log_entry, pos, sizeof(struct change_log_entry));
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
 */

static int tcbl_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    int rc;
    tcbl_vfs tcbl_vfs = (struct tcbl_vfs *) vfs;

    tcbl_fh fh = tcbl_malloc(NULL, sizeof(struct tcbl_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->underlying_fh = NULL;
    fh->log = NULL;

    rc = vfs_open(tcbl_vfs->underlying_vfs, file_name, &fh->underlying_fh);
    if (rc) {
        goto exit;
    }

    rc = tlog_open_v1(vfs, file_name, tcbl_vfs->page_size, &fh->log);
    if (rc) {
        goto exit;
    }

    fh->vfs = vfs;
    fh->change_log = NULL;
    fh->txn_active = false;
    *file_handle_out = (vfs_fh) fh;
    exit:
    if (fh->underlying_fh) {
        vfs_close(fh->underlying_fh);
    }
    if (fh->log) {
        tlog_close(fh->log);
    }
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
    int rc2 = tlog_close(fh->log);
    tcbl_free(NULL, fh, sizeof(struct tcbl_fh));
    return rc1 == TCBL_OK ? rc2 : rc1;
}

static int tcbl_change_log_cmp(change_log_entry l1, change_log_entry l2) {
    return l1->offset < l2->offset ? -1 : l1->offset == l2->offset ? 0 : 1;
}

static int tcbl_read(vfs_fh file_handle, void* data, size_t offset, size_t len)
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
    char buff[sizeof(struct change_log_entry) + page_size];
    rc = TCBL_OK;
    while (read_offset < block_offset + block_len) {
        change_log_entry result;
        struct change_log_entry search_record = {
            read_offset
        };
        // check changes in the current transaction
        SGLIB_LIST_FIND_MEMBER(struct change_log_entry, fh->change_log, &search_record, tcbl_change_log_cmp, next, result);
        if (result) {
            memcpy(dst, &result->data[block_begin_skip], block_read_size);
            bounds_error = false;
            goto next_block;
        }
        // check the log for this block
        bool found_block;
        change_log_entry log_page = (change_log_entry) &buff;
        rc = tlog_find_block(fh->log, &found_block, log_page, fh->txn_begin_log_entry_ct, read_offset);
        if (rc) {
            goto txn_end;
        } else {
            if (found_block) {
                memcpy(dst, &log_page->data[block_begin_skip], block_read_size);
                if (log_page->newlen < offset + len) {
                    bounds_error = true;
                } else {
                    bounds_error = false;
                }
                goto next_block;
            }
        }

        rc = vfs_read(fh->underlying_fh, buff, read_offset, page_size);
        if (rc == TCBL_BOUNDS_CHECK) {
            size_t underlying_size;
            vfs_file_size(fh->underlying_fh, &underlying_size);
            if (underlying_size < read_offset + block_begin_skip + block_read_size) {
                bounds_error = true;
            } else {
                bounds_error = false;
            }
        }
        if (!(rc == TCBL_OK || rc == TCBL_BOUNDS_CHECK)) {
            goto txn_end;
        }
        memcpy(dst, &buff[block_begin_skip], block_read_size);

        next_block:
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
        return bounds_error ? TCBL_BOUNDS_CHECK : TCBL_OK;
    }
}

static int tcbl_file_size(vfs_fh file_handle, size_t* out)
{
    int rc;
    tcbl_fh fh = (tcbl_fh) file_handle;

    // Check current transaction changes
    if (fh->change_log) {
        *out = fh->change_log->newlen;
        return TCBL_OK;
    }

    // Check the change log
    bool found_size;
    size_t size;
    if (fh->txn_active) {
        rc = tlog_file_size(fh->log, &found_size, &size, fh->txn_begin_log_entry_ct);
    } else {
        rc = tlog_file_size(fh->log, &found_size, &size, 0);
    }
    if (rc) {
        return rc;
    }
    if (found_size) {
        *out = size;
        return TCBL_OK;
    }

    // Go to the data file if size not available in the log
    return vfs_file_size(fh->underlying_fh, out);
}

static int tcbl_truncate(vfs_fh file_handle, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

static int tcbl_write(vfs_fh file_handle, const void* data, size_t offset, size_t len)
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
            change_log_entry log_record = tcbl_malloc(NULL, sizeof(struct change_log_entry) + page_size);
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
                    if (rc == TCBL_BOUNDS_CHECK) {
                        // trust that read has provided everything that it can and
                        // zeroed out the rest
                        rc = TCBL_OK;
                    } else if (rc) {
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
            SGLIB_LIST_ADD(struct change_log_entry, fh->change_log, log_record, next);
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

    rc = tlog_entry_ct(fh->log, &fh->txn_begin_log_entry_ct);
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
        uint64_t current_log_entry_ct;

        rc = tlog_entry_ct(fh->log, &current_log_entry_ct);
        if (rc) {
            return rc;
        }

        if (current_log_entry_ct != fh->txn_begin_log_entry_ct) {
            // abort for now TODO add merge operator, e.g., append when blocks don't conflict
            tcbl_txn_abort(file_handle);
            return TCBL_CONFLICT_ABORT;
        }

        SGLIB_LIST_REVERSE(struct change_log_entry, fh->change_log, next);
        rc = tlog_append(fh->log, fh->change_log);
        if (rc) {
            return rc;
        }

        // Free the log
        while (fh->change_log) {
            change_log_entry l = fh->change_log;
            SGLIB_LIST_DELETE(struct change_log_entry, fh->change_log, l, next);
            tcbl_free(NULL, l, sizeof(struct change_log_entry) + page_size);
        }
    }
    fh->txn_active = false;
    return TCBL_OK;
}

static int tcbl_txn_abort(vfs_fh file_handle)
{
    tcbl_fh fh = (tcbl_fh) file_handle;

    if (!fh->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    while (fh->change_log) {
        change_log_entry l = fh->change_log;
        SGLIB_LIST_DELETE(struct change_log_entry, fh->change_log, l, next);
        tcbl_free(NULL, l, sizeof(struct change_log_entry) + page_size);
    }
    fh->txn_active = false;
    return TCBL_OK;
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
    return tlog_checkpoint(fh->log, fh->underlying_fh);
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