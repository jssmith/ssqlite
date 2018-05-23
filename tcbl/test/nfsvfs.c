#include <tcbl_runtime.h>
#include "vfs.h"
#include "nfsvfs.h"
#include <stdint.h>

int nfs_open(vfs vfs, const char *file_name_str, vfs_fh *file_handle_out)
{
    nfs_vfs nvfs = (nfs_vfs) vfs;
    nfs_vfs_fh fh = tcbl_malloc(NULL, sizeof(struct nfs_vfs_fh));
    if (fh == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = vfs;

    vector filename = allocate_buffer(NULL, strlen(file_name_str) + 1);
    push_bytes(filename, file_name_str, strlen(file_name_str));
    printf("have length of filename %d\n", strlen(file_name_str));
    vector path = allocate_buffer(NULL, 10);
    vector_push(path, filename);

    status st = file_open_write(nvfs->client, path, &fh->nfs_fh);
    if (st) {
        return TCBL_ALLOC_FAILURE;
    }
    *file_handle_out = (vfs_fh) fh;
    return TCBL_OK;
}

int nfs_delete(vfs vfs, const char *file_name)
{
    return TCBL_NOT_IMPLEMENTED;
}

int nfs_exists(vfs vfs, const char *file_name_str, int *out)
{
    nfs_vfs v = (nfs_vfs) vfs;

    vector filename = allocate_buffer(NULL, strlen(file_name_str) + 1);
    push_bytes(filename, file_name_str, strlen(file_name_str));
    printf("have length of filename %d\n", strlen(file_name_str));
    vector path = allocate_buffer(NULL, 10);
    vector_push(path, filename);

    status st = exists(v->client, path);
    *out = st ? 1 : 0;
    // TODO need to property translate errors rather than
    // just saying does not exist
    return TCBL_OK;
}

int nfs_close(vfs_fh file_handle)
{
    nfs_vfs_fh fh = (nfs_vfs_fh) file_handle;
    file_close(fh->nfs_fh);
    fh->nfs_fh = NULL;
    return TCBL_OK;
}


int nfs_read(vfs_fh file_handle, void* data, size_t offset, size_t len, size_t *out_len)
{
    nfs_vfs_fh fh = (nfs_vfs_fh) file_handle;
    status st = readfile(fh->nfs_fh, data, offset, len);
    if (st) return TCBL_IO_ERROR;
    if (out_len != NULL) {
        // TODO can we get the true length read from the client?
        *out_len = len;
    }
    return TCBL_OK;
}

int nfs_write(vfs_fh file_handle, const void* data, size_t offset, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

int nfs_lock(vfs_fh file_handle, int lock_operation)
{
    return TCBL_NOT_IMPLEMENTED;
}

int nfs_file_size(vfs_fh file_handle, size_t *out)
{
    nfs_vfs_fh fh = (nfs_vfs_fh) file_handle;
    uint64_t sz;
    status st = file_size(fh->nfs_fh, &sz);
    if (st) return TCBL_IO_ERROR;
    *out = sz;
    return TCBL_OK;
}

int nfs_truncate(vfs_fh file_handle, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

int nfs_freevfs(vfs vfs)
{
    // TODO close client
    return TCBL_OK;
}

int nfs_vfs_allocate(vfs *vfs, const char *servername)
{
    client c;
    status st = create_client((char *)servername, &c); // TODO remove char * cast
    if (st != STATUS_OK) {
        return TCBL_IO_ERROR;
    }

    static struct vfs_info nfs_vfs_info = {
            sizeof(struct nfs_vfs),
            sizeof(struct nfs_vfs_fh),
            nfs_open,
            nfs_delete,
            nfs_exists,
            nfs_close,
            nfs_read,
            nfs_write,
            nfs_lock,
            nfs_file_size,
            nfs_truncate,
            nfs_freevfs
    };
    nfs_vfs nfs_vfs = tcbl_malloc(NULL, sizeof(struct nfs_vfs));
    if (!vfs) {
        return TCBL_ALLOC_FAILURE;
    }
    nfs_vfs->vfs_info = &nfs_vfs_info;
    nfs_vfs->client = c;
    *vfs = (struct vfs*) nfs_vfs;
    return TCBL_OK;
}


