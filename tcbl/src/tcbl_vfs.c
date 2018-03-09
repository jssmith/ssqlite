#include "tcbl_vfs.h"
#include "runtime.h"

int tcbl_allocate(tcbl_vfs* tcbl, vfs underlying_fs)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_open(tcbl_vfs tcbl, const char* file_name, tcbl_fh* file_handle_out)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_close(tcbl_fh file_handle)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_read(tcbl_fh file_handle, char* data, size_t offset, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_write(tcbl_fh file_handle, const char* data, size_t offset, size_t len)
{
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_vfs_free(vfs vfs)
{
    if (vfs->vfs_ops->x_free) {
        vfs->vfs_ops->x_free(vfs);
    }
    tcbl_free(NULL, vfs, vfs->vfs_size);
    return TCBL_OK;
}

int vfs_open(vfs vfs, const char* file_name, vfs_fh* file_handle_out)
{
    return vfs->vfs_ops->x_open(vfs, file_name, file_handle_out);
}

int vfs_close(vfs_fh file_handle)
{
    return file_handle->vfs->vfs_ops->x_close(file_handle->vfs, file_handle);
}

int vfs_read(vfs_fh file_handle, char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_ops->x_read(file_handle->vfs, file_handle, data, offset, len);
}

int vfs_write(vfs_fh file_handle, const char* data, size_t offset, size_t len)
{
    return file_handle->vfs->vfs_ops->x_write(file_handle->vfs, file_handle, data, offset, len);
}