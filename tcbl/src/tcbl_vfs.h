#ifndef _TCBL_H
#define _TCBL_H

#include <stddef.h>

#define TCBL_OK 0
#define TCBL_NOT_IMPLEMENTED 1
#define TCBL_ALLOC_FAILURE 2
#define TCBL_BAD_ARGUMENT 3
#define TCBL_BOUNDS_CHECK 4


typedef struct vfs *vfs;
typedef struct tvfs *tvfs;

typedef struct vfs_fh {
    vfs vfs;
} *vfs_fh;

typedef struct vfs_txn {
    tvfs vfs;
} *vfs_txn;

typedef struct vfs_info {
    size_t vfs_obj_size;
    size_t vfs_fh_size;
    int (*x_open)(vfs, const char* file_name, vfs_fh* file_handle_out);
    int (*x_close)(vfs, vfs_fh);
    int (*x_read)(vfs, vfs_fh, char* data, size_t offset, size_t len);
    int (*x_write)(vfs, vfs_fh, const char* data, size_t offset, size_t len);
    int (*x_free)(vfs);
} *vfs_info;

typedef struct tvfs_info {
    size_t vfs_txn_size;
    int (*x_begin_txn)(tvfs, vfs_txn *);
    int (*x_commit_txn)(tvfs, vfs_txn);
    int (*x_abort_txn)(tvfs, vfs_txn);
    int (*x_txn_read)(tvfs, vfs_txn, vfs_fh, char* data, size_t offset, size_t len);
    int (*x_txn_write)(tvfs, vfs_txn, vfs_fh, const char* data, size_t offset, size_t len);
} *tvfs_info;

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
    vfs underlying_vfs;
} *tcbl_vfs;

typedef struct tcbl_fh {
    vfs vfs;
    vfs_fh underlying_fh;
} *tcbl_fh;

typedef struct tcbl_txn {
    tvfs vfs;
} *tcbl_txn;


int tcbl_allocate(tvfs* tvfs, vfs underlying_vfs);

int vfs_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out);
int vfs_close(vfs_fh file_handle);
int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len);
int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len);
int vfs_sync(tcbl_fh file_handle);
int vfs_free(vfs vfs);


int vfs_txn_begin(tvfs tvfs, vfs_txn *txn);
int vfs_txn_commit(vfs_txn txn);
int vfs_txn_abort(vfs_txn txn);
int vfs_txn_read(vfs_txn txn, vfs_fh file_handle, char* data, size_t offset, size_t len);
int vfs_txn_write(vfs_txn txn, vfs_fh file_handle, const char* data, size_t offset, size_t len);

//int tcbl_sync(tcbl_fh file_handle);

#endif