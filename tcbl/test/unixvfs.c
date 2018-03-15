#define _XOPEN_SOURCE 500

#include <unistd.h>
#include <stdio.h>
#include <runtime.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <sglib.h>

#include "unixvfs.h"

typedef struct unixvfs_fh {
    struct vfs_fh vfs_fh;
    int fd;
    bool is_valid;
    struct unixvfs_fh *next;
} *unixvfs_fh;

typedef struct unixvfs {
    vfs_info vfs_info;
    struct unixvfs_fh *file_handles;
    char *root_path;
} *unixvfs;



static int unix_vfs_open(vfs vfs, const char *file_name, vfs_fh* file_handle_out)
{
    unixvfs uvfs = (unixvfs) vfs;
    size_t root_path_len = strlen(uvfs->root_path);
    size_t file_name_len = strlen(file_name);
    char full_path_name[file_name_len + root_path_len + 1];
    memcpy(full_path_name, uvfs->root_path, root_path_len);
    memcpy(&full_path_name[root_path_len], file_name, file_name_len + 1);

    int fd = open(full_path_name, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        printf("problem opening file %s\n", strerror(errno));
        // TODO assign correct error codes
        return TCBL_FILE_NOT_FOUND;
    }

    unixvfs_fh fh = tcbl_malloc(NULL, sizeof(struct unixvfs_fh));
    if (!fh) {
        close(fd);
        return TCBL_ALLOC_FAILURE;
    }

    fh->vfs_fh.vfs = vfs;
    fh->fd = fd;
    fh->is_valid = true;
    fh->next = NULL;
    SGLIB_LIST_ADD(struct unixvfs_fh, uvfs->file_handles, fh, next);
    *file_handle_out = (vfs_fh) fh;
    return TCBL_OK;
}

static int unix_vfs_delete(vfs vfs, const char* file_name)
{
    unixvfs uvfs = (unixvfs) vfs;
    size_t root_path_len = strlen(uvfs->root_path);
    size_t file_name_len = strlen(file_name);
    char full_path_name[file_name_len + root_path_len + 1];
    memcpy(full_path_name, uvfs->root_path, root_path_len);
    memcpy(&full_path_name[root_path_len], file_name, file_name_len + 1);

    int rc = remove(full_path_name);
    if (rc) {
        if (errno == ENOENT) {
            return TCBL_FILE_NOT_FOUND;
        }
        return TCBL_IO_ERROR;
    }
    return TCBL_OK;
}

static int unix_vfs_exists(vfs vfs, const char* file_name, int *out)
{
    unixvfs uvfs = (unixvfs) vfs;
    size_t root_path_len = strlen(uvfs->root_path);
    size_t file_name_len = strlen(file_name);
    char full_path_name[file_name_len + root_path_len + 1];
    memcpy(full_path_name, uvfs->root_path, root_path_len);
    memcpy(&full_path_name[root_path_len], file_name, file_name_len + 1);

    struct stat s;
    *out = stat(full_path_name, &s) == 0 ? 1 : 0; // TODO handle errors
    return TCBL_OK;
}

static int unix_vfs_close(vfs_fh vfs_fh)
{
    unixvfs_fh fh = (unixvfs_fh) vfs_fh;
    if (fh->fd) {
        int rc = close(fh->fd);
        if (rc) {
            return TCBL_IO_ERROR;
        }
    }
    return TCBL_OK;
}

static int unix_vfs_read(vfs_fh vfs_fh, char *data, size_t offset, size_t len)
{
    unixvfs_fh fh = (unixvfs_fh) vfs_fh;

    void* pos = data;
    ssize_t read_offset = offset;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t len_read = pread(fh->fd, pos, remaining, read_offset);
//        printf("read len %ld\n", len_read);
        if (len_read == 0) {
            // end of file
            return TCBL_BOUNDS_CHECK;
        }
        if (len_read == -1) {
            return TCBL_IO_ERROR;
        }
        pos += len_read;
        read_offset += len_read;
        remaining -= len_read;
    }
    return TCBL_OK;
}

static int unix_vfs_write(vfs_fh vfs_fh, const char *data, size_t offset, size_t len)
{
    unixvfs_fh fh = (unixvfs_fh) vfs_fh;
    const void* pos = data;
    ssize_t write_offset = offset;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t len_written = pwrite(fh->fd, pos, remaining, write_offset);
        if (len_written == -1) {
            return TCBL_IO_ERROR;
        }
        pos += len_written;
        write_offset += len_written;
        remaining -= len_written;
    }
    return TCBL_OK;
}

static int unix_vfs_file_size(vfs_fh vfs_fh, size_t* size_out)
{
    unixvfs_fh fh = (unixvfs_fh) vfs_fh;
    struct stat s;
    int rc = fstat(fh->fd, &s);
    if (rc) {
        return TCBL_IO_ERROR;
    }
    *size_out = (size_t) s.st_size;
    return TCBL_OK;
}

static int unix_vfs_truncate(vfs_fh vfs_fh, size_t len)
{
    unixvfs_fh fh = (unixvfs_fh) vfs_fh;
    int rc = ftruncate(fh->fd, len);
    if (rc) {
        return TCBL_IO_ERROR;
    }
    return TCBL_OK;
}

int unix_vfs_free(vfs vfs)
{
    unixvfs uvfs = (unixvfs) vfs;
    unixvfs_fh fh = uvfs->file_handles;
    while (fh) {
        fh->is_valid = false;
        if (fh->fd) {
            // TODO check errors?
            close(fh->fd);
        }
        fh = fh->next;
    }
    return TCBL_OK;
}

int unix_vfs_allocate(vfs *vfs, const char* root_path)
{
    static struct vfs_info unix_vfs_info = {
            sizeof(struct unixvfs),
            sizeof(struct unixvfs_fh),
            unix_vfs_open,
            unix_vfs_delete,
            unix_vfs_exists,
            unix_vfs_close,
            unix_vfs_read,
            unix_vfs_write,
            unix_vfs_file_size,
            unix_vfs_truncate,
            unix_vfs_free
    };

    unixvfs unixvfs = tcbl_malloc(NULL, sizeof(struct unixvfs));
    if (!unixvfs) {
        return TCBL_ALLOC_FAILURE;
    }

    size_t root_path_len = strlen(root_path);
    bool add_trailing_slash = root_path[root_path_len - 1] != '/';
    size_t root_path_alloc_len = add_trailing_slash ? root_path_len + 2 : root_path_len + 1;
    unixvfs->root_path = tcbl_malloc(NULL, root_path_alloc_len);
    if (!unixvfs->root_path) {
        tcbl_free(NULL, unixvfs, sizeof(struct unixvfs));
        return  TCBL_ALLOC_FAILURE;
    }
    strcpy(unixvfs->root_path, root_path);
    if (add_trailing_slash) {
        strcpy(&unixvfs->root_path[root_path_len], "/");
    }

    unixvfs->vfs_info = &unix_vfs_info;
    unixvfs->file_handles = NULL;
    *vfs = (struct vfs *) unixvfs;
    return TCBL_OK;
}
