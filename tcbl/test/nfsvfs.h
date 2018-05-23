#ifndef TCBL_NFSVFS_H
#define TCBL_NFSVFS_H

#include "tcbl_runtime.h"
#include "nfs4.h"

typedef struct nfs_vfs_fh {
    vfs vfs;
    file nfs_fh;
} *nfs_vfs_fh;

typedef struct nfs_vfs {
    vfs_info vfs_info;
    client client;
} *nfs_vfs;


int nfs_vfs_allocate(vfs *vfs, const char *servername);

#endif //TCBL_NFSVFS_H
