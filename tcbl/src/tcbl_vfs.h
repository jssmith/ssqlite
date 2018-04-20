#ifndef TCBL_TCBL_VFS_H
#define TCBL_TCBL_VFS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "ll_log.h"
#include "vfs.h"

typedef struct tvfs *tvfs;

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
    struct bc_log txn_log;
    struct bc_log_h txn_log_h;
    bool txn_active;
} *tcbl_fh;

int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs, size_t page_size);

#endif // TCBL_TCBL_VFS_H