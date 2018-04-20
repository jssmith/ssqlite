#ifndef TCBL_LL_LOG_H
#define TCBL_LL_LOG_H

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#define LL_LOG_ENTRY_READ           1
#define LL_LOG_ENTRY_WRITE          2
#define LL_LOG_ENTRY_BEGIN          3
#define LL_LOG_ENTRY_TRY_COMMIT     4

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
int tcbl_log_meld(tcbl_log_h /*, size_t *offs_out */);
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

///////////////////////////////////
// serializable log
// an underlying log may contain serializability violations, but this view
// only shows serializable transactions
/*
typedef struct vector {
    void* data;
    size_t capacity;
    size_t sz;
    size_t element_sz;
} *vector;

int vector_init(vector v, size_t elem_sz, size_t initial_capacity);
int vector_reset(vector v, size_t min_capacity, size_t max_capacity);
int vector_append(vector v, void* data, size_t offs_out);
int vector_get(vector v, size_t offs, void **data_out);
*/

typedef struct readlist {
    int i;
    struct ilist *next;
} ilist;

typedef struct tcbl_log_serializable {
    struct tcbl_log log;
    tcbl_log underlying_log;
} *tcbl_log_serializable;

typedef struct tcbl_log_h_serializable {
    tcbl_log log;
    tcbl_log_offs base_len;

    struct tcbl_log_h underlying_log_h[0];
} *tcbl_log_h_serializable;

int tcbl_log_init_serializable(tcbl_log, size_t block_size, tcbl_log underlying_log);

/*
 * The block change log adds access by block id
 */

typedef struct bc_log {
    tcbl_log log;
    size_t page_size;
} *bc_log;

typedef struct bc_log_entry {
    size_t offset;
    size_t newlen;
    union {
        uint64_t lsn;
        struct change_log_entry* next;
    };
    char data[0];
} *bc_log_entry;


typedef struct bc_log_h {
    bc_log log;
    tcbl_log_h log_h;
} *bc_log_h;

int bc_log_create(bc_log);
int bc_log_txn_begin(bc_log, bc_log_h);
int bc_log_txn_commit(bc_log_h);
int bc_log_txn_abort(bc_log_h);
int bc_log_write(bc_log_h, size_t offs, void* data, size_t newlen);
// TODO should newlen be here?
// TODO should we just have a write length rather than assume block size?
int bc_log_read(bc_log_h, size_t offs, bool *found_data, void** out_data);
int bc_log_length(bc_log_h, bool *found_size, size_t *out_size);


size_t ll_log_entry_size(tcbl_log log, ll_log_entry entry);

#endif // TCBL_LL_LOG_H