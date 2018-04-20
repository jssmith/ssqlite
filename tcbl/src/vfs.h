#ifndef TCBL_VFS_H
#define TCBL_VFS_H

typedef struct vfs *vfs;
typedef struct vfs_fh *vfs_fh;

#define VFS_LOCK_SH 1
#define VFS_LOCK_EX 2
#define VFS_LOCK_UN 8

typedef struct vfs_info {
    size_t vfs_obj_size;
    size_t vfs_fh_size;
    int (*x_open)(vfs, const char *file_name, vfs_fh* file_handle_out);
    int (*x_delete)(vfs, const char* file_name);
    int (*x_exists)(vfs, const char* file_name, int *out);
    int (*x_close)(vfs_fh);
    int (*x_read)(vfs_fh, void *data, size_t offset, size_t len);
    int (*x_write)(vfs_fh, const void *data, size_t offset, size_t len);
    int (*x_lock)(vfs_fh, int lock_operation);
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

int vfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out);
int vfs_delete(vfs vfs, const char *file_name);
int vfs_exists(vfs vfs, const char *file_name, int *out);
int vfs_close(vfs_fh file_handle);
int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len);
int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len);
int vfs_lock(vfs_fh file_handle, int lock_operation);
int vfs_file_size(vfs_fh file_handle, size_t *out);
int vfs_truncate(vfs_fh file_handle, size_t len);
int vfs_free(vfs vfs);

int vfs_txn_begin(vfs_fh vfs_fh);
int vfs_txn_commit(vfs_fh vfs_fh);
int vfs_txn_abort(vfs_fh vfs_fh);
int vfs_checkpoint(vfs_fh vfs_fh);

#endif