#ifndef TCBL_CACHEDVFS_H
#define TCBL_CACHEDVFS_H

#include <stddef.h>

typedef struct cvfs *cvfs;
typedef struct cvfs_fh *cvfs_h;



int vfs_cache_allocate(cvfs *, int (*cb)(), size_t page_size, size_t num_pages);

int vfs_cache_open(cvfs, cvfs_h *);

int vfs_cache_get(cvfs_h, size_t offs, size_t len)

int vfs_cache_get(cvfs* c, cvfs_fh h, );
int vfs_cache_update(cvfs *c, cvfs_fh h, size_t offs, size_t len);


#endif //TCBL_CACHEDVFS_H
