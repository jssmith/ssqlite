#ifndef TCBL_MEMVFS_H
#define TCBL_MEMVFS_H

#include "tcbl_vfs.h"

int memvfs_allocate(vfs *vfs);
int memvfs_free(vfs vfs);

#endif