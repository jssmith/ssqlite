#ifndef TCBL_LL_LOG_H
#define TCBL_LL_LOG_H

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include "vfs.h"
#include "cachedvfs.h"

#define LL_LOG_ENTRY_READ           1
#define LL_LOG_ENTRY_WRITE          2
#define LL_LOG_ENTRY_BEGIN          3
#define LL_LOG_ENTRY_TRY_COMMIT     4

/*
typedef struct tcbl_mem {
    size_t len;
    void* data;
} *tcbl_mem;

int tcbl_mem_init(tcbl_mem, size_t len);
int tcbl_mem_ensure_capacity(tcbl_mem, size_t len);
int tcbl_mem_free(tcbl_mem);

typedef struct tcbl_base_mem_log {
    size_t n_entries;
    struct tcbl_mem cum_sizes;
    struct tcbl_mem entries;
} *tcbl_base_mem_log;

// xxx rename all of these to mem -> base_mem
int tcbl_mem_log_init(tcbl_base_mem_log);
int tcbl_mem_log_append(tcbl_base_mem_log, void* data, size_t len);
int tcbl_mem_log_length(tcbl_base_mem_log, size_t *len_out);
int tcbl_mem_log_concat(tcbl_base_mem_log, tcbl_base_mem_log);
int tcbl_mem_log_get(tcbl_base_mem_log, size_t index, void** out_data, size_t *out_len);
int tcbl_mem_log_reset(tcbl_base_mem_log);
int tcbl_mem_log_free(tcbl_base_mem_log);


typedef struct ll_log_entry {
    int entry_type;
} *ll_log_entry;

typedef struct ll_log_entry_read {
    int entry_type;
    size_t offset;
} *ll_log_entry_read;

typedef struct ll_log_entry_write {
    int entry_type;
    size_t offset;
    size_t newlen;
    char* data[0];
} *ll_log_entry_write;

typedef struct ll_log_entry_begin {
    int entry_type;
} *ll_log_entry_begin;

typedef struct ll_log_entry_try_commit {
    int entry_type;
} *ll_log_entry_commit;

//typedef struct ll_log_meld_ops {
//    int (*x_meld_begin)(void **meld_data);
//    int (*x_add_entries)(void **meld_data, ll_log_entry *, bool *accept); // in this version just return accept or not
//    int (*x_meld_end)(void *meld_data);
//} *ll_log_meld_ops;

typedef size_t tcbl_log_offs;
typedef struct tcbl_log *tcbl_log;

typedef struct tcbl_log_h {
    tcbl_log log;
} *tcbl_log_h;

struct tcbl_log_ops {
    int (*x_open)(tcbl_log, tcbl_log_h);
    int (*x_length)(tcbl_log, tcbl_log_offs *);
    int (*x_meld)(tcbl_log_h);
    int (*x_meld_offs)(tcbl_log_h, size_t offs);
    int (*x_reset)(tcbl_log_h);
    int (*x_append)(tcbl_log_h, ll_log_entry);
    int (*x_seek)(tcbl_log_h, tcbl_log_offs offs);
    int (*x_next)(tcbl_log_h, ll_log_entry *);
    int (*x_close)(tcbl_log_h);
    int (*x_free)(tcbl_log);
};

struct tcbl_log {
    size_t block_size;
    size_t log_h_size;
    struct tcbl_log_ops ops;
};

int tcbl_log_open(tcbl_log, tcbl_log_h);
//int tcbl_log_branch_offs(tcbl_log_h, tcbl_log_offs *);
int tcbl_log_length(tcbl_log l, tcbl_log_offs *out_offs);
int tcbl_log_meld(tcbl_log_h);
int tcbl_log_meld_offs(tcbl_log_h, size_t offs);
int tcbl_log_reset(tcbl_log_h);
int tcbl_log_append(tcbl_log_h, ll_log_entry);
int tcbl_log_seek(tcbl_log_h, tcbl_log_offs seek_pos);
int tcbl_log_next(tcbl_log_h, ll_log_entry *);
// xx do we need close - why not close on meld or reset? to keep read ptr?
int tcbl_log_close(tcbl_log_h);
int tcbl_log_free(tcbl_log);

// in memory log implementation

typedef struct tcbl_log_entry_mem {
    struct tcbl_log_entry_mem *next;
    struct ll_log_entry e[0];
} *tcbl_log_entry_mem;

typedef struct tcbl_log_mem {
    struct tcbl_log log;
    struct tcbl_base_mem_log entries;
} *tcbl_log_mem;

typedef struct tcbl_log_h_mem {
    tcbl_log log;
    tcbl_log_offs base_len;
    tcbl_base_mem_log pos_log;
    size_t pos_log_offs;
    struct tcbl_base_mem_log added_entries;
} *tcbl_log_h_mem;

int tcbl_log_init_mem(tcbl_log, size_t block_size);

size_t ll_log_entry_size(tcbl_log log, ll_log_entry entry);
*/

/*
 * The block change log provides access by block id. For now we have just one simple
 * implementation based on files.
 */

typedef struct bc_log {
    size_t page_size;
    char* log_name;
    vfs underlying_vfs;
    vfs_fh data_fh;
    vfs_fh log_fh;
    cvfs_h data_cache_h;
} *bc_log;

#define LOG_FLAG_COMMIT         1
#define LOG_FLAG_CHECKPOINT     2

typedef struct bc_log_index {
    uint64_t checkpoint_seq;
} *bc_log_index;

typedef struct bc_log_header {
    uint64_t checkpoint_seq;
    size_t newlen;
} *bc_log_header;

typedef struct bc_log_entry {
    size_t offset;
    size_t newlen;
    char flag;
    union {
        uint64_t lsn;
        struct bc_log_entry* next;
    };
    char data[0];
} *bc_log_entry;

typedef struct bc_log_h {
    bc_log log;
    bool txn_active;
    size_t txn_offset;
    bc_log_entry added_entries;
    bc_log_entry read_entry;
} *bc_log_h;

int bc_log_create(bc_log, vfs vfs, vfs_fh data_fh, cvfs_h cache_h, const char *name, size_t page_size);
int bc_log_delete(vfs vfs, const char *name);
int bc_log_checkpoint(bc_log);
int bc_log_txn_begin(bc_log, bc_log_h);
int bc_log_txn_commit(bc_log_h);
int bc_log_txn_abort(bc_log_h);
int bc_log_write(bc_log_h, size_t offs, void* data, size_t newlen);
int bc_log_read(bc_log_h, size_t offs, bool *found_data, void** out_data, size_t *out_newlen);
int bc_log_length(bc_log_h, bool *found_size, size_t *out_size);

#endif // TCBL_LL_LOG_H