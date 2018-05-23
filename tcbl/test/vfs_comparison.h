#ifndef TCBL_VFS_COMPARISON_H
#define TCBL_VFS_COMPARISON_H

#include "tcbl_runtime.h"
#include "vfs.h"

typedef struct comparison_vfs_fh {
    vfs vfs;
    vfs_fh vfs_fh_a;
    vfs_fh vfs_fh_b;
} *comparison_vfs_fh;

typedef struct comparison_vfs {
    vfs_info vfs_info;
    tvfs_info tvfs_info;
    vfs vfs_a;
    vfs vfs_b;
} *comparison_vfs;

int comparison_vfs_allocate(vfs *comparson_vfs, vfs a, vfs b);

#endif //TCBL_VFS_COMPARISON_H
