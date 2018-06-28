#include <string.h>
#include <sys/param.h>
#include "tcbl_runtime.h"
#include "ll_log.h"
#include "tcbl_vfs.h"
#include "sglib.h"

//#define DEBUG_PRINT
#ifdef DEBUG_PRINT
static void print_data(void* data, size_t len)
{
    char *d = data;
    size_t i;
    for (i = 0; i < len; i++) {
        if (i > 0) {
            if (i % 8 == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
        printf("%02x", d[i] & 0xff);
    }
    printf("\n");
}
#endif


int str_append(char *dst_buff, const char *s1, const char *s2, size_t buff_len)
{
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    if (l1 + l2 + 1 <= buff_len) {
        memcpy(dst_buff, s1, l1);
        memcpy(&dst_buff[l1], s2, l2 + 1);
        return TCBL_OK;
    } else {
        return TCBL_BOUNDS_CHECK;
    }
}

int bc_log_create(bc_log l, vfs vfs, vfs_fh data_fh, cvfs_h cache_h, const char *name, size_t page_size)
{
    l->page_size = page_size;
    size_t name_sz = strlen(name);
//    l->log_name = tcbl_malloc(NULL, name_sz + 5);
//    if (l->log_name == NULL) {
//        return TCBL_ALLOC_FAILURE;
//    }
    if (name_sz + 5 > 100) {
        return TCBL_BAD_ARGUMENT;
    }
    // TODO maybe we can be lazy about opening here
    memcpy(l->log_name, name, name_sz);
    memcpy(&l->log_name[name_sz], "-log", 5);
    int rc = vfs_open(vfs, l->log_name, &l->log_fh);
    if (rc) return rc;
    l->data_fh = data_fh;
    l->underlying_vfs = vfs;
    l->data_cache_h = cache_h;
    l->handles = NULL;

    char buff[sizeof(struct bc_log_header)];
    rc = vfs_read(l->log_fh, buff, 0, sizeof(struct bc_log_header));
    // TODO there is a race condition here when two processes simultaneously
    // create and initialize the log file.
    uint64_t checkpoint_seq = 1;
    if (rc == TCBL_OK) {
        checkpoint_seq = ((bc_log_header) buff)->checkpoint_seq;
    } else if (rc == TCBL_BOUNDS_CHECK) {
        printf("wrting new log header\n");
        // write a new header
        bc_log_header h = (bc_log_header) buff;
        h->checkpoint_seq = checkpoint_seq;
        if (data_fh != NULL) {
            rc = vfs_file_size(data_fh, &h->newlen);
            if (rc) return rc;
        }
        rc = vfs_write(l->log_fh, buff, 0, sizeof(struct bc_log_header));
    }
    if (rc) return rc;
    l->fh_checkpoint_seq = checkpoint_seq;
    l->cache_update_offset.checkpoint_seq = checkpoint_seq;
    vfs_file_size(l->log_fh, &l->cache_update_offset.offset);

    return TCBL_OK;
}

int bc_log_delete(vfs vfs, const char *name)
{
    size_t name_len = strlen(name);
    char log_file_name[name_len + 5];
    memcpy(log_file_name, name, name_len);
    memcpy(&log_file_name[name_len], "-log", 5);
    int rc = vfs_delete(vfs, log_file_name);
    if (rc == TCBL_FILE_NOT_FOUND) {
        return TCBL_OK;
    }
    return rc;
}

static int bc_log_apply(vfs_fh log_fh, vfs_fh data_fh, size_t start_offs, size_t end_offs, size_t page_size, size_t *out_newlen)
{
    size_t read_offs = start_offs;
    size_t log_entry_size = sizeof(struct bc_log_entry) + page_size;

    int rc;
    char buff[log_entry_size];
    bc_log_entry e = (bc_log_entry) buff;
    while (read_offs < end_offs) {
        rc = vfs_read(log_fh, buff, read_offs, log_entry_size);
        if (rc) return rc;
        *out_newlen = e->newlen;
        rc = vfs_write(data_fh, e->data, e->offset, MIN(page_size, e->newlen - e->offset));
        if (rc) return rc;
        read_offs += log_entry_size;
    }
    return TCBL_OK;
}

int bc_log_checkpoint(bc_log l)
{
    // TODO - maybe check whether have active txn
    int rc, cleanup_rc;
    size_t log_name_len = strlen(l->log_name);
//    printf("checkpoint on log %s\n", l->log_name);
    char checkpoint_coordinator_name[log_name_len + 4];
    rc = str_append(checkpoint_coordinator_name, l->log_name, "-cp", sizeof(checkpoint_coordinator_name));
//    printf("checkpoint coordinator is %s\n", checkpoint_coordinator_name);
    if (rc) return rc;

    size_t log_entry_size = sizeof(struct bc_log_entry) + l->page_size;
    char commit_entry_bytes[log_entry_size];

    size_t log_header_sz = sizeof(struct bc_log_header);
    char log_header_bytes[log_header_sz];

    char cp_fh_bytes[l->underlying_vfs->vfs_info->vfs_fh_size];
    vfs_fh cp_fh = (vfs_fh) &cp_fh_bytes;
    rc = vfs_open(l->underlying_vfs, checkpoint_coordinator_name, &cp_fh);
    if (rc) return rc;

    // TODO this should be a nonblocking lock operation that fails if commit is in progress
    rc = vfs_lock(cp_fh, VFS_LOCK_EX);
    if (rc) return rc;

    size_t log_index_sz = sizeof(struct bc_log_index);
    char log_index_bytes[log_index_sz];

    size_t log_index_start_sz;
    rc = vfs_file_size(cp_fh, &log_index_start_sz);
    if (rc) return rc;

    uint64_t start_checkpoint_seq = 1;
    if (log_index_start_sz == log_index_sz) {
        rc = vfs_read(cp_fh, log_index_bytes, 0, log_index_sz);
        if (rc) goto exit_a;
        start_checkpoint_seq = ((bc_log_index) log_index_bytes)->checkpoint_seq;
    } else if (log_index_start_sz != 0) {
        printf("invalid size log index");
        rc = TCBL_INTERNAL_ERROR;
        goto exit_a;
    } else {
        printf("zero file size log index, assuming default starting checkpoint seq\n");
    }

    size_t log_file_size;
    rc = vfs_file_size(l->log_fh, &log_file_size);
    if (rc) goto exit_a;

    rc = vfs_read(l->log_fh, log_header_bytes, 0, log_header_sz);
    if (rc) goto exit_a;
    uint64_t log_checkpoint_seq = ((bc_log_header) log_header_bytes)->checkpoint_seq;
    if (log_checkpoint_seq != start_checkpoint_seq) {
        printf("log checkpoint seq file: %ld <> counter: %ld\n",
            log_checkpoint_seq, start_checkpoint_seq);
        rc = TCBL_INTERNAL_ERROR;
        goto exit_a;
    }

    size_t newlen = ((bc_log_header) log_header_bytes)->newlen;
    rc = bc_log_apply(l->log_fh, l->data_fh, log_header_sz, log_file_size, l->page_size, &newlen);
    if (rc) goto exit_a;

    rc = vfs_lock(l->log_fh, VFS_LOCK_EX);
    if (rc) goto exit_a;

    size_t final_log_file_size;
    rc = vfs_file_size(l->log_fh, &final_log_file_size);
    if (rc) goto exit_b;

    if (final_log_file_size > log_file_size) {
        rc = bc_log_apply(l->log_fh, l->data_fh, log_file_size, final_log_file_size, l->page_size, &newlen);
        if (rc) goto exit_b;
    }

    bc_log_entry commit_entry = (bc_log_entry) commit_entry_bytes;
    memset(commit_entry, 0, log_entry_size);
    commit_entry->flag = LOG_FLAG_CHECKPOINT;
    rc = vfs_write(l->log_fh, commit_entry_bytes, final_log_file_size, log_entry_size);
//    printf("wrote checkpoint record at offset %ld for log %s\n", final_log_file_size, l->log_name);
    exit_b:
    cleanup_rc = vfs_lock(l->log_fh, VFS_LOCK_EX | VFS_LOCK_UN);
    rc = rc ? rc : cleanup_rc;
    if (rc) goto exit_a;

    // Update the index header
    uint64_t new_checkpoint_seq = start_checkpoint_seq + 1;
    ((bc_log_index) log_index_bytes)->checkpoint_seq = new_checkpoint_seq;
    rc = vfs_write(cp_fh, log_index_bytes, 0, log_index_sz);
//    printf("updated the index header %s %ld\n", checkpoint_coordinator_name, new_checkpoint_seq);
    if (rc) goto exit_a;

    // Create a new log file but leave the old one open because we
    // want to be able to apply the log to updating the cache.

    rc = vfs_delete(l->underlying_vfs, l->log_name);
    if (rc) goto exit_a;

    vfs_fh new_log_fh;
    rc = vfs_open(l->underlying_vfs, l->log_name, &new_log_fh);
    if (rc) goto exit_a;

    ((bc_log_header) log_header_bytes)->checkpoint_seq = new_checkpoint_seq;
    ((bc_log_header) log_header_bytes)->newlen = newlen;
    rc = vfs_write(new_log_fh, log_header_bytes, 0, log_header_sz);
    if (rc) goto exit_a;
//    printf("created new log file with checkpoint sequence %lu for %s\n", new_checkpoint_seq, l->log_name);

    rc = vfs_close(new_log_fh);

    exit_a:
    cleanup_rc = vfs_lock(cp_fh, VFS_LOCK_EX | VFS_LOCK_UN);
    rc = rc ? rc : cleanup_rc;
    cleanup_rc = vfs_close(cp_fh);
    rc = rc ? rc : cleanup_rc;
//    printf("bc log checkpoint returns %d\n", rc);
    return rc;
}

static int cmp_log_offset(struct log_offset a, struct log_offset b)
{
    return a.checkpoint_seq < b.checkpoint_seq ? -1 :
           a.checkpoint_seq == b.checkpoint_seq ? (
                a.offset < b.offset ? -1 : a.offset == b.offset ? 0 : 1
           ) : 1;
}

int bc_log_txn_begin(bc_log l, bc_log_h h)
{
    int rc;
    h->log = l;
    h->added_entries = NULL;
    size_t log_size;
    rc = vfs_file_size(h->log->log_fh, &log_size);
    if (rc) return rc;

    // DEBUGGING - check the header
    char header_buff[sizeof(struct bc_log_header)];
    vfs_read(h->log->log_fh, header_buff, 0, sizeof(struct bc_log_header));
//    printf("begin txn with log header version %lu\n",
//           ((struct bc_log_header *) header_buff)->checkpoint_seq);
    // END - DEBUGGING - check the header
    size_t log_entry_size = sizeof(struct bc_log_entry) + h->log->page_size;

    // Update the cache by applying logs as far as possible, limited by other open
    // log handles requiring snapshot access.
    struct log_offset min_open_offset = { UINT64_MAX, SIZE_MAX };
    if (l->handles != NULL) {
        struct bc_log_h_l *p = l->handles;
        // find the smallest open offset
        while (p != NULL) {
            if (cmp_log_offset(min_open_offset, p->bc_log_h->txn_offset) < 0) {
                min_open_offset = p->bc_log_h->txn_offset;
            }
            p = p->next;
        }
    } else {
        min_open_offset = (struct log_offset) { l->fh_checkpoint_seq, sizeof(struct bc_log_header) };
    }
//    struct log_offset max_log_offset = { l->fh_checkpoint_seq, MAX(log_size, sizeof(struct bc_log_header))};
//    struct log_offset max_log_offset = { l->fh_checkpoint_seq, log_size >= sizeof(struct bc_log_header) + log_entry_size ? log_size - log_entry_size : sizeof(struct bc_log_header) };
    struct log_offset max_log_offset = { l->fh_checkpoint_seq, log_size >= sizeof(struct bc_log_header) + log_entry_size ? log_size : sizeof(struct bc_log_header) };
    struct log_offset read_offset = cmp_log_offset(min_open_offset, max_log_offset) < 0 ?
                                    min_open_offset : max_log_offset;
    while (cmp_log_offset(read_offset, max_log_offset) < 0) {
        char buff[log_entry_size];
        bc_log_entry e = (bc_log_entry) buff;
        rc = vfs_read(h->log->log_fh, buff, read_offset.offset, log_entry_size);
        if (rc) return rc;
        if (e->flag == LOG_FLAG_CHECKPOINT) {
            rc = vfs_close(h->log->log_fh);
            if (rc) return rc;
            rc = vfs_open(h->log->underlying_vfs, h->log->log_name, &h->log->log_fh);
            if (rc) return rc;
            rc = vfs_file_size(h->log->log_fh, &log_size);
            if (rc) return rc;

            size_t new_checkpoint_seq;
            char header_buff[sizeof(struct bc_log_header)];
            vfs_read(h->log->log_fh, header_buff, 0, sizeof(struct bc_log_header));
            new_checkpoint_seq = ((bc_log_header) header_buff)->checkpoint_seq;
            if (new_checkpoint_seq != l->fh_checkpoint_seq + 1) {
                // TODO dump cache and skip ahead
                return TCBL_SNAPSHOT_EXPIRED;
            }
            l->fh_checkpoint_seq = new_checkpoint_seq;
            max_log_offset = (struct log_offset) { new_checkpoint_seq, MAX(log_size, sizeof(struct bc_log_header)) };
            read_offset = (struct log_offset) { new_checkpoint_seq, sizeof(struct bc_log_header) };
        } else {
            if (l->data_cache_h) {
                vfs_cache_update(l->data_cache_h, e->data, e->offset, MIN(h->log->page_size, e->newlen - e->offset));
            }
            // TODO cache update length
            read_offset.offset += log_entry_size;
        }
    }

    h->read_entry = tcbl_malloc(NULL, sizeof(struct bc_log_entry) + l->page_size);
    if (h->read_entry == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    struct bc_log_h_l *new_list_entry = tcbl_malloc(NULL, sizeof(struct bc_log_h_l));
    if (new_list_entry == NULL) {
        tcbl_free(NULL, h->read_entry, sizeof(struct bc_log_entry) + l->page_size);
        h->read_entry = NULL;
        return TCBL_ALLOC_FAILURE;
    }
    new_list_entry->bc_log_h = h;
    SGLIB_LIST_ADD(struct bc_log_h_l, l->handles, new_list_entry, next);
    // TODO need to add code to free this entry
    h->txn_offset = (struct log_offset) { l->fh_checkpoint_seq, log_size };
//    printf("start transaction at offset %lu:%ld\n", h->txn_offset.checkpoint_seq, h->txn_offset.offset);
    h->txn_active = true;
    return TCBL_OK;
}

static int initialize_log(bc_log log)
{
    int rc;
    uint64_t initial_checkpoint_seq = 1;
    char buff_header[sizeof(struct bc_log_header)];
    bc_log_header log_header = (bc_log_header) buff_header;
    log_header->checkpoint_seq = initial_checkpoint_seq;
    log_header->newlen = 0; // TODO sometimes should probably read this in
    rc = vfs_write(log->log_fh, buff_header, 0, sizeof(struct bc_log_header));
    if (rc) return rc;

    // write a checkpoint index files as well
    char checkpoint_fh_bytes[log->underlying_vfs->vfs_info->vfs_fh_size];
    vfs_fh checkpoint_index_fh = (vfs_fh) checkpoint_fh_bytes;

    char checkpoint_fn[strlen(log->log_name) + 4];
    str_append(checkpoint_fn, log->log_name, "-cp", sizeof(checkpoint_fn));
    rc = vfs_open(log->underlying_vfs, checkpoint_fn, &checkpoint_index_fh);
    if (rc) return rc;

    char checkpoint_index_buff[sizeof(struct bc_log_index)];
    bc_log_index checkpoint_index = (bc_log_index)  checkpoint_index_buff;
    checkpoint_index->checkpoint_seq = initial_checkpoint_seq;
    rc = vfs_write(checkpoint_index_fh, checkpoint_index_buff, 0, sizeof(struct bc_log_index));

    int rc_close = vfs_close(checkpoint_index_fh);
    return rc ? rc : rc_close;
}

int log_h_cmp(struct bc_log_h_l *a, struct bc_log_h_l *b)
{
    return (a->bc_log_h < b->bc_log_h) ? -1 : (a->bc_log_h == b->bc_log_h) ? 0 : 1;
}

void bc_log_txn_cleanup(bc_log_h h)
{
//    printf("free entry at %p\n", h->read_entry);
    if (h->txn_active) {
        tcbl_free(NULL, h->read_entry, sizeof(struct bc_log_entry) + h->log->page_size);
        h->read_entry = NULL;
        struct bc_log_h_l t = { h, NULL };
        struct bc_log_h_l *x;
        SGLIB_LIST_FIND_MEMBER(struct bc_log_h_l, h->log->handles, &t, log_h_cmp, next, x);
        SGLIB_LIST_DELETE(struct bc_log_h_l, h->log->handles, x, next);
        tcbl_free(NULL, x, sizeof(struct bc_log_h_l));
        h->txn_active = false;
    }
}

int bc_log_txn_commit(bc_log_h h)
{
    int rc = TCBL_OK;
    int unlockrc;
    if (!h->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }
    if (h->added_entries == NULL) {
        h->txn_active = false;
        return TCBL_OK;
    } else {
        size_t log_entry_size = sizeof(struct bc_log_entry) + h->log->page_size;
        char buff[log_entry_size];
        bc_log_entry we = (bc_log_entry) buff;
        size_t write_offs = h->txn_offset.offset;
        bc_log_entry e;

        rc = vfs_lock(h->log->log_fh, VFS_LOCK_EX);
        if (rc) return rc;

        // Ensure that file size has not changed
        size_t log_size;
        rc = vfs_file_size(h->log->log_fh, &log_size);
        if (rc) goto exit;

        if (log_size != h->txn_offset.offset) {
            bc_log_txn_abort(h);
            rc = TCBL_CONFLICT_ABORT;
            goto exit;
        }

        if (h->txn_offset.offset == 0) {
            rc = initialize_log(h->log);
            if (rc) goto exit;
            write_offs = sizeof(struct bc_log_header);
        }

        SGLIB_LIST_REVERSE(struct bc_log_entry, h->added_entries, next);
        while (h->added_entries) {
            e = h->added_entries;
            SGLIB_LIST_DELETE(struct bc_log_entry, h->added_entries, e, next);
            // TODO go faster by merging writes
            memcpy(we, e, log_entry_size);
            we->lsn = 1;
            if (!e->next) {
                we->flag = LOG_FLAG_COMMIT;
            }
            rc = vfs_write(h->log->log_fh, buff, write_offs, log_entry_size);
            if (rc) break; // TODO should probably continue to free memory
            write_offs += log_entry_size;
            tcbl_free(NULL, e, log_entry_size);
        }
        exit:
        unlockrc = vfs_lock(h->log->log_fh, VFS_LOCK_EX | VFS_LOCK_UN);
        bc_log_txn_cleanup(h);
        return rc ? rc : unlockrc;
    }
}

int bc_log_txn_abort(bc_log_h h) {
    int rc = TCBL_OK;
    size_t log_entry_size = sizeof(struct bc_log_entry) + h->log->page_size;
    bc_log_entry e;

    if (!h->txn_active) {
        return TCBL_NO_TXN_ACTIVE;
    }

    if (h->added_entries) {
        while (h->added_entries) {
            e = h->added_entries;
            SGLIB_LIST_DELETE(struct bc_log_entry, h->added_entries, e, next);
            tcbl_free(NULL, e, log_entry_size);
        }
//        rc = vfs_lock(h->log->log_fh, VFS_LOCK_EX | VFS_LOCK_UN);
    }
    bc_log_txn_cleanup(h);
    return rc;
}

int bc_log_write(bc_log_h h, size_t offs, void* data, size_t newlen)
{
    size_t page_size = h->log->page_size;
#ifdef DEBUG_PRINT
    printf("bc log write %ld at offs %ld\n", newlen, offs);
    print_data(data, page_size);
#endif
    if (offs % page_size != 0) {
        return TCBL_BAD_ARGUMENT;
    }
    // TODO bring this back...
//    if (h->added_entries == NULL) {
//        vfs_lock(h->log->log_fh, VFS_LOCK_EX);
//    }
    int rc = TCBL_OK;
    bc_log_entry e = tcbl_malloc(NULL, sizeof(struct bc_log_entry) + page_size);
    if (e == NULL) {
        rc = TCBL_ALLOC_FAILURE;
        goto exit;
    }
    e->offset = offs;
    e->newlen = newlen;
    e->flag = 0;
    e->next = NULL;
    memcpy(e->data, data, page_size);
#ifdef DEBUG_PRINT
    printf("installed data at %p entry at position %p for offs %ld\n", e->data, e, offs);
    print_data(e->data, page_size);
#endif
    SGLIB_LIST_ADD(struct bc_log_entry, h->added_entries, e, next);
    exit:
    if (rc) {
        vfs_lock(h->log->log_fh, VFS_LOCK_EX | VFS_LOCK_UN);
    }
    return rc;
}

int log_entry_comparator(bc_log_entry e1, bc_log_entry e2)
{
    return (e1->offset < e2->offset) ? -1 : (e1->offset == e2->offset) ? 0 : 1;
}

int bc_log_read(bc_log_h h, size_t offs, bool *found_data, void** out_data, size_t *out_newlen)
{
//#ifdef TCBL_PERF_STATS
//    tcbl_stats_counter_inc(&((tcbl_fh) h->log->data_fh)->stats, TCBL_BC_LOG_READ);
//#endif
//
    size_t page_size = h->log->page_size;
    if (offs % page_size != 0) {
        return TCBL_BAD_ARGUMENT;
    }
    size_t log_entry_size = sizeof(struct bc_log_entry) + h->log->page_size;


    bc_log_entry found_entry;
    struct bc_log_entry search_entry = {
        offs
    };
    // search the added entries
    SGLIB_LIST_FIND_MEMBER(struct bc_log_entry, h->added_entries, &search_entry, log_entry_comparator, next, found_entry);

    if (found_entry) {
#ifdef DEBUG_PRINT
        printf("returning data from position %p entry at position %p for offs %ld\n", found_entry->data, found_entry, offs);
        print_data(found_entry->data, page_size);
#endif
        *out_data = found_entry->data;
        *out_newlen = found_entry->newlen;
        *found_data = true;
        return TCBL_OK;
    }

    // read from the log file
    int rc;
    size_t read_offs = sizeof(struct bc_log_header);
    char buff[log_entry_size];
    bc_log_entry e = (bc_log_entry) buff;
    bool found = false;
    while (read_offs < h->txn_offset.offset) {
        rc = vfs_read(h->log->log_fh, buff, read_offs, log_entry_size);
        if (rc) return rc;
        if (e->offset == offs && e->flag != LOG_FLAG_CHECKPOINT) {
            found = true;
            memcpy(h->read_entry, buff, log_entry_size);
        }
        read_offs += log_entry_size;
    }
    if (found) {
        *found_data = true;
        *out_data = &(h->read_entry->data);
        *out_newlen = h->read_entry->newlen;
    } else {
        *found_data = false;
        *out_data = NULL;
    }
    return TCBL_OK;
}

int bc_log_length(bc_log_h h, bool *found_size, size_t *out_size)
{
    int rc;
    *found_size = false;
    if (h->added_entries) {
        *found_size = true;
        *out_size = h->added_entries->newlen;
    } else if (h->txn_offset.offset > sizeof(struct bc_log_header)) {
        size_t log_entry_size = sizeof(struct bc_log_entry) + h->log->page_size;
        size_t read_offs = h->txn_offset.offset - log_entry_size;
        while (!*found_size && read_offs >= sizeof(struct bc_log_header)) {
            char buff[log_entry_size];
            bc_log_entry e = (bc_log_entry) buff;
            rc = vfs_read(h->log->log_fh, buff, read_offs, log_entry_size);
            if (rc) return rc;
            if (e->flag == LOG_FLAG_COMMIT) {
                *found_size = true;
                *out_size = e->newlen;
            } else {
                if (read_offs >= log_entry_size) {
                    read_offs -= log_entry_size;
                } else {
                    read_offs = 0;
                }
            }
        }
    } else if (h->txn_offset.offset == sizeof(struct bc_log_header)) {
        char buff[sizeof(struct bc_log_header)];
        rc = vfs_read(h->log->log_fh, buff, 0, sizeof(struct bc_log_header));
        if (rc) return rc;
        *found_size = true;
        *out_size = ((bc_log_header) buff)->newlen;
    }
    return TCBL_OK;
}
