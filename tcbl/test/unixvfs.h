#ifndef TCBL_VFS_UNIX_H
#define TCBL_VFS_UNIX_H

#include "tcbl_vfs.h"

int unix_vfs_allocate(vfs *vfs, const char *);
int unix_vfs_free(vfs vfs);

#endif //TCBL_VFS_UNIX_H
