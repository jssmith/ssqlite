#include <string.h>
#include <stdio.h>
#include <sys/param.h>

#include "memvfs.h"
#include "sglib.h"
#include "runtime.h"

typedef struct memvfs_file {
    char *name;
    size_t name_alloc_len;
    char *data;
    size_t len;
    size_t alloc_len;
    struct memvfs_file *next_file;
} *memvfs_file;

typedef struct memvfs_fh {
    struct memvfs *vfs;
    struct memvfs_file *memvfs_file;
} *memvfs_fh;

typedef struct memvfs {
    vfs_info vfs_info;
    struct memvfs_file *files;
} *memvfs;


static void print_block(const char* data, size_t len)
{
    for (int i = 0; i < len; i++) {
        if (i % 8 == 7) {
            printf("%02x\n", data[i] & 0xff);
        } else {
            printf("%02x ", data[i] & 0xff);
        }
    }
}

int file_name_comparator(memvfs_file f1, memvfs_file f2)
{
    return strcmp(f1->name, f2->name);
}

int memvfs_find_file(memvfs memvfs, const char *name, memvfs_file *file_out)
{
    struct memvfs_file* result;
    struct memvfs_file search_file = {
            (char*) name
    };
    SGLIB_LIST_FIND_MEMBER(struct memvfs_file, memvfs->files, &search_file, file_name_comparator, next_file, result);
    if (result) {
        *file_out = result;
        return TCBL_OK;
    }
    memvfs_file f = tcbl_malloc(NULL, sizeof(struct memvfs_file));
    if (!f) {
        return TCBL_ALLOC_FAILURE;
    }
    size_t name_len = strlen(name) + 1;
    f->name = tcbl_malloc(NULL, name_len);
    if (!f->name) {
        return TCBL_ALLOC_FAILURE;
    }
    strcpy(f->name, name);
    f->name_alloc_len = name_len;
    f->data = 0;
    f->len = 0;
    f->alloc_len = 0;
    f->next_file = NULL; // TODO is this necessary?
    *file_out = f;

    SGLIB_LIST_ADD(struct memvfs_file, memvfs->files, f, next_file);
    return TCBL_OK;
}

int memvfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out)
{
    memvfs_fh fh = tcbl_malloc(NULL, sizeof(struct memvfs_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = (memvfs) vfs;
    *file_handle_out = (vfs_fh) fh;

    return memvfs_find_file((memvfs) vfs, file_name, &fh->memvfs_file);
}

int memvfs_close(vfs_fh file_handle)
{
    tcbl_free(NULL, file_handle, file_handle->vfs->vfs_info->vfs_fh_size);
    return TCBL_OK;
}

int memvfs_read(vfs_fh file_handle, char *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    if (f->len < offset + len) {
        return TCBL_BOUNDS_CHECK;
    }
    memcpy(buff, &f->data[offset], len);
#ifdef TCBL_MEMVFS_VERBOSE
    printf("memvfs read %s %lx %lx\n", f->name, offset, len);
    print_block(buff, len);
#endif
    return TCBL_OK;
}

static int memvfs_ensure_alloc_len(memvfs_file f, size_t len)
{
    if (f->alloc_len < len) {
        size_t new_len = MAX(len, f->alloc_len * 2);
        char* new_data = tcbl_malloc(NULL, new_len);
        if (!new_data) {
            return TCBL_ALLOC_FAILURE;
        }
        if (f->data) {
            memcpy(new_data, f->data, f->len);
            tcbl_free(NULL, f->data, f->alloc_len);
        }
        f->data = new_data;
        f->alloc_len = new_len;
    }
    return TCBL_OK;
}

int memvfs_write(vfs_fh file_handle, const char *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    memvfs_ensure_alloc_len(f, offset + len);

    memcpy(&f->data[offset], buff, len);
    if (offset > f->len) {
        memset(&f->data[f->len], 0, offset - f->len);
    }
    if (offset + len > f->len) {
        f->len = offset + len;
    }
#ifdef TCBL_MEMVFS_VERBOSE
    printf("memvfs write %s %lx %lx\n", f->name, offset, len);
    print_block(buff, len);
#endif
    return TCBL_OK;
}

int memvfs_file_size(vfs_fh vfs_fh, size_t* out)
{
    struct memvfs_fh *fh = (struct memvfs_fh *) vfs_fh;
    *out = fh->memvfs_file->len;
    return TCBL_OK;
}

int memvfs_truncate(vfs_fh vfs_fh, size_t len)
{
    int rc;
    memvfs_fh fh = (memvfs_fh) vfs_fh;
    memvfs_file f = fh->memvfs_file;
    if (len > f->len) {
        rc = memvfs_ensure_alloc_len(f, len);
        if (rc) {
            return rc;
        }
        memset(&f->data[f->len], 0, len - f->len);
    }
    f->len = len;
    return TCBL_OK;
}

int memvfs_free(vfs vfs)
{
    memvfs memvfs = (struct memvfs *) vfs;
    while (memvfs->files) {
        memvfs_file f = memvfs->files;
        SGLIB_LIST_DELETE(struct memvfs_file, memvfs->files, f, next_file);
        tcbl_free(NULL, f->name, f->name_alloc_len);
        if (f->data) {
            tcbl_free(NULL, f->data, f->alloc_len);
        }
        tcbl_free(NULL, f, sizeof(struct memvfs_file));
    }
    return TCBL_OK;
}

int memvfs_allocate(vfs *vfs)
{
    static struct vfs_info memvfs_info = {
            sizeof(struct memvfs),
            sizeof(struct memvfs_fh),
            &memvfs_open,
            &memvfs_close,
            &memvfs_read,
            &memvfs_write,
            &memvfs_file_size,
            &memvfs_truncate,
            &memvfs_free
    };
    memvfs memvfs = tcbl_malloc(NULL, sizeof(struct memvfs));
    if (!memvfs) {
        return TCBL_ALLOC_FAILURE;
    }
    memvfs->vfs_info = &memvfs_info;
    memvfs->files = NULL;
    *vfs = (struct vfs *) memvfs;
    return TCBL_OK;
}