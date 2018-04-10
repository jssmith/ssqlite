#ifndef TCBL_TCBL_H
#define TCBL_TCBL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct vfs *vfs;
typedef struct tvfs *tvfs;
typedef struct vfs_fh *vfs_fh;

typedef struct vfs_info {
    size_t vfs_obj_size;
    size_t vfs_fh_size;
    int (*x_open)(vfs, const char *file_name, vfs_fh* file_handle_out);
    int (*x_delete)(vfs, const char* file_name);
    int (*x_exists)(vfs, const char* file_name, int *out);
    int (*x_close)(vfs_fh);
    int (*x_read)(vfs_fh, void *data, size_t offset, size_t len);
    int (*x_write)(vfs_fh, const void *data, size_t offset, size_t len);
    int (*x_file_size)(vfs_fh, size_t* size_out);
    int (*x_truncate)(vfs_fh, size_t len);
    int (*x_free)(vfs);
} *vfs_info;

typedef struct tvfs_info {
    // TODO standardize on txn_begin or begin_txn
    int (*x_begin_txn)(vfs_fh);
    int (*x_commit_txn)(vfs_fh);
    int (*x_abort_txn)(vfs_fh);
    int (*x_checkpoint)(vfs_fh);
} *tvfs_info;

struct vfs_fh {
    vfs vfs;
};

struct vfs {
    vfs_info vfs_info;
};

struct tvfs {
    vfs_info vfs_info;
    tvfs_info tvfs_info;
};

typedef struct tcbl_vfs {
    vfs_info vfs_info;
    tvfs_info tvfs_info;
    size_t page_size;
    vfs underlying_vfs;
} *tcbl_vfs;

typedef struct tlog *tlog;

typedef struct change_log_entry {
    size_t offset;
    size_t newlen;
    union {
        uint64_t lsn;
        struct change_log_entry* next;
    };
    char data[0];
} *change_log_entry;

typedef struct tcbl_fh {
    vfs vfs;
    vfs_fh underlying_fh;
    tlog log;
    bool txn_active;
    uint64_t txn_begin_log_entry_ct;
    change_log_entry change_log;
} *tcbl_fh;

struct tlog_ops {
    int (*x_txn_begin)(tlog);
    int (*x_txn_commit)(tlog);
    int (*x_delete)(tlog);
    int (*x_close)(tlog);
    int (*x_entry_ct)(tlog, uint64_t *);
    int (*x_find_block)(tlog, bool *, change_log_entry, uint64_t, size_t);
    int (*x_file_size)(tlog, bool *, size_t *, uint64_t);
    int (*x_append)(tlog, void*);
    int (*x_checkpoint)(tlog, vfs_fh);
    int (*x_free)(tlog log);
};

int tlog_open_v1(vfs, const char *file_name, size_t page_size, tlog *tlog);
int tlog_txn_begin(tlog log); // xxx might want a read-only flag here
int tlog_txn_commit(tlog log);
int tlog_txn_abort(tlog log);

int tlog_delete(tlog log);
int tlog_close(tlog log);
int tlog_entry_ct(tlog, uint64_t *log_entry_ct_out);
int tlog_find_block(tlog, bool *found_block, change_log_entry change_record, uint64_t log_entry_ct, size_t offset);
int tlog_file_size(tlog, bool *found_size, size_t *size_out, uint64_t log_entry_ct);
int tlog_append(tlog, change_log_entry change_records);
int tlog_checkpoint(tlog, vfs_fh underlying_fh);
int tlog_free(tlog log);

int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs, size_t page_size);

int vfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out);
int vfs_delete(vfs vfs, const char *file_name);
int vfs_exists(vfs vfs, const char *file_name, int *out);
int vfs_close(vfs_fh file_handle);
int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len);
int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len);
int vfs_file_size(vfs_fh file_handle, size_t *out);
int vfs_truncate(vfs_fh file_handle, size_t len);
//int vfs_sync(tcbl_fh file_handle);
int vfs_free(vfs vfs);

int vfs_txn_begin(vfs_fh vfs_fh);
int vfs_txn_commit(vfs_fh vfs_fh);
int vfs_txn_abort(vfs_fh vfs_fh);
int vfs_checkpoint(vfs_fh vfs_fh);

#endif // TCBL_TCBL_H