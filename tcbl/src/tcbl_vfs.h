#ifndef _TCBL_H
#define _TCBL_H

#include <stddef.h>

#define TCBL_OK 0
#define TCBL_NOT_IMPLEMENTED 1
#define TCBL_ALLOC_FAILURE 2
#define TCBL_BAD_ARGUMENT 3
#define TCBL_BOUNDS_CHECK 4

typedef struct tcbl_fh {
    int ct;
} *tcbl_fh;

typedef struct tcbl_vfs {
    int ct;
    int (*open)(const char*, tcbl_fh*);
} *tcbl_vfs;

//struct vfs;
typedef struct vfs *vfs;

typedef struct vfs_fh {
    vfs vfs;
} *vfs_fh;


typedef struct vfs_info {
    size_t vfs_obj_size;
    size_t vfs_fh_size;
    int (*x_open)(struct vfs* vfs, const char* file_name, vfs_fh* file_handle_out);
    int (*x_close)(struct vfs*, vfs_fh);
    int (*x_read)(struct vfs*, vfs_fh, char* data, size_t offset, size_t len);
    int (*x_write)(struct vfs*, vfs_fh, const char* data, size_t offset, size_t len);
    int (*x_free)(struct vfs*);
} *vfs_info;

struct vfs {
    vfs_info vfs_ops;
};

typedef struct tcbl_txn {

} *tcbl_txn;

int tcbl_allocate(tcbl_vfs* tcbl, vfs underlying_fs);
int tcbl_open(tcbl_vfs tcbl, const char* file_name, tcbl_fh* file_handle_out);
int tcbl_close(tcbl_fh file_handle);
int tcbl_read(tcbl_fh file_handle, char* data, size_t offset, size_t len);
int tcbl_write(tcbl_fh file_handle, const char* data, size_t offset, size_t len);
int tcbl_sync(tcbl_fh file_handle);

int tcbl_begin_txn(tcbl_vfs tcbl, tcbl_txn tcbl_txn);
int tcbl_commit_txn(tcbl_txn);

int tcbl_vfs_free(vfs vfs);



int vfs_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out);
int vfs_close(vfs_fh file_handle);
int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len);
int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len);
//int tcbl_sync(tcbl_fh file_handle);

#endif