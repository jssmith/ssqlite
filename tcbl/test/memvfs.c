#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <pthread.h>
#include <z3.h>

#include "memvfs.h"
#include "sglib.h"
#include "runtime.h"

typedef struct memvfs_file {
    char *name;
    size_t name_alloc_len;
    char *data;
    size_t len;
    size_t alloc_len;
    uint64_t ref_ct;
    struct memvfs_file *next_file;
} *memvfs_file;

typedef struct memvfs_fh {
    struct memvfs *vfs;
    struct memvfs_file *memvfs_file;
    bool is_valid;
    struct memvfs_fh *next;
} *memvfs_fh;

typedef struct memvfs {
    vfs_info vfs_info;
    pthread_mutex_t lock;
    struct memvfs_file *files;
    struct memvfs_fh *file_handles;
} *memvfs;

static void memvfs_free_file(memvfs_file f);

#ifdef TCBL_MEMVFS_VERBOSE
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
#endif

static inline int lock(memvfs memvfs)
{
    // TODO error checking
    int rc = pthread_mutex_lock(&memvfs->lock);
    pthread_t self = pthread_self();
    printf("lock %p %lu %d\n", &memvfs->lock, self, rc);
}

static inline int unlock(memvfs memvfs)
{
    // TODO error checking
    pthread_t self = pthread_self();
    int rc = pthread_mutex_unlock(&memvfs->lock);
    printf("unlock %p %lu %d\n", &memvfs->lock, self, rc);
}

static int file_name_comparator(memvfs_file f1, memvfs_file f2)
{
    return strcmp(f1->name, f2->name);
}

static void memvfs_find_existing_file(memvfs memvfs, const char *name, memvfs_file *file_out)
{
    struct memvfs_file search_file = {
            (char*) name
    };
    SGLIB_LIST_FIND_MEMBER(struct memvfs_file, memvfs->files, &search_file, file_name_comparator, next_file, *file_out);
}

static int memvfs_find_file(memvfs memvfs, const char *name, memvfs_file *file_out)
{
    memvfs_find_existing_file(memvfs, name, file_out);
    if (*file_out) {
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
    f->ref_ct = 1;
    *file_out = f;

    SGLIB_LIST_ADD(struct memvfs_file, memvfs->files, f, next_file);
    return TCBL_OK;
}

static int memvfs_open(vfs vfs, const char *file_name, vfs_fh *file_handle_out)
{
    memvfs_fh fh = tcbl_malloc(NULL, sizeof(struct memvfs_fh));
    if (!fh) {
        return TCBL_ALLOC_FAILURE;
    }
    fh->vfs = (memvfs) vfs;
    fh->is_valid = true;
    *file_handle_out = (vfs_fh) fh;

    lock((memvfs) vfs);
    int rc = memvfs_find_file((memvfs) vfs, file_name, &fh->memvfs_file);
    if (rc) {
        tcbl_free(NULL, fh, sizeof(struct memvfs_fh));
    } else {
        SGLIB_LIST_ADD(struct memvfs_fh, ((memvfs) vfs)->file_handles, fh, next);
        fh->memvfs_file->ref_ct += 1;
    }
    unlock((memvfs) vfs);
    return rc;
}

static int memvfs_delete(vfs vfs, const char *file_name)
{
    int rc = TCBL_OK;
    memvfs_file f;
    lock((memvfs) vfs);
    memvfs_find_existing_file((memvfs) vfs, file_name, &f);
    if (!f) {
        rc = TCBL_FILE_NOT_FOUND;
        goto exit;
    } else {
        SGLIB_LIST_DELETE(struct memvfs_file, ((memvfs) vfs)->files, f, next_file);
        f->ref_ct -= 1;
        if (f->ref_ct == 0) {
            memvfs_free_file(f);
        }
        goto exit;
    }
    exit:
    unlock((memvfs) vfs);
    return rc;
}

static int memvfs_exists(vfs vfs, const char* file_name, int *out)
{
    lock((memvfs) vfs);
    memvfs_file f;
    memvfs_find_existing_file((memvfs) vfs, file_name, &f);
    *out = (f != NULL);
    unlock((memvfs) vfs);
    return TCBL_OK;
}

static int memvfs_close(vfs_fh file_handle)
{
    memvfs_file f = ((memvfs_fh) file_handle)->memvfs_file;
    lock((memvfs) file_handle->vfs);
    if (f) {
        f->ref_ct -= 1;
        if (f->ref_ct == 0) {
            memvfs_free_file(f);
        }
    }
    SGLIB_LIST_DELETE(struct memvfs_fh, ((memvfs)((memvfs_fh) file_handle)->vfs)->file_handles, (memvfs_fh) file_handle, next);
    tcbl_free(NULL, file_handle, file_handle->vfs->vfs_info->vfs_fh_size);
    unlock((memvfs) file_handle->vfs);
    return TCBL_OK;
}

static int memvfs_read(vfs_fh file_handle, void *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    lock((memvfs) file_handle->vfs);
    if (f->len < offset + len) {
        if (offset < f->len) {
            size_t read_len = f->len - offset;
            memcpy(buff, &f->data[offset], read_len);
            memset(&buff[read_len], 0, len - read_len);
        } else {
            memset(buff, 0, len);
        }
        return TCBL_BOUNDS_CHECK;
    }
    memcpy(buff, &f->data[offset], len);
#ifdef TCBL_MEMVFS_VERBOSE
    printf("memvfs read %s %lx %lx\n", f->name, offset, len);
    print_block(buff, len);
#endif
    unlock((memvfs) file_handle->vfs);
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

static int memvfs_write(vfs_fh file_handle, const void *buff, size_t offset, size_t len)
{
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;

    lock((memvfs) file_handle->vfs);
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
    unlock((memvfs) file_handle->vfs);
    return TCBL_OK;
}

static int memvfs_file_size(vfs_fh file_handle, size_t* out)
{
    struct memvfs_fh *fh = (struct memvfs_fh *) file_handle;
    lock((memvfs) file_handle->vfs);
    *out = fh->memvfs_file->len;
    unlock((memvfs) file_handle->vfs);
    return TCBL_OK;
}

static int memvfs_truncate(vfs_fh file_handle, size_t len)
{
    int rc = TCBL_OK;
    memvfs_fh fh = (memvfs_fh) file_handle;
    memvfs_file f = fh->memvfs_file;
    lock((memvfs) file_handle->vfs);
    if (len > f->len) {
        rc = memvfs_ensure_alloc_len(f, len);
        if (rc) {
            goto exit;
        }
        memset(&f->data[f->len], 0, len - f->len);
    }
    f->len = len;
    exit:
    unlock((memvfs) file_handle->vfs);
    return rc;
}

static void memvfs_free_file_data(memvfs_file f)
{
    if (f->data) {
        tcbl_free(NULL, f->data, f->alloc_len);
        f->data = NULL;
    }
}

static void memvfs_free_file(memvfs_file f)
{
    tcbl_free(NULL, f->name, f->name_alloc_len);
    memvfs_free_file_data(f);
    tcbl_free(NULL, f, sizeof(struct memvfs_file));
}

int memvfs_free(vfs vfs)
{
    memvfs memvfs = (struct memvfs *) vfs;
    memvfs_fh fh = memvfs->file_handles;
    lock(memvfs);
    while (fh) {
        fh->is_valid = false;
        fh = fh->next;
    }
    while (memvfs->files) {
        memvfs_file f = memvfs->files;
        SGLIB_LIST_DELETE(struct memvfs_file, memvfs->files, f, next_file);
        f->ref_ct -= 1;
        if (f->ref_ct == 0) {
            memvfs_free_file(f);
        } else {
            memvfs_free_file_data(f);
        }
    }
    // TODO HOW DO WE FREE memvfs itself?
    unlock(memvfs);
    return TCBL_OK;
}

int memvfs_allocate(vfs *vfs)
{
    static struct vfs_info memvfs_info = {
            sizeof(struct memvfs),
            sizeof(struct memvfs_fh),
            memvfs_open,
            memvfs_delete,
            memvfs_exists,
            memvfs_close,
            memvfs_read,
            memvfs_write,
            memvfs_file_size,
            memvfs_truncate,
            memvfs_free
    };
    memvfs memvfs = tcbl_malloc(NULL, sizeof(struct memvfs));
    if (!memvfs) {
        return TCBL_ALLOC_FAILURE;
    }
    memvfs->vfs_info = &memvfs_info;
    memvfs->files = NULL;
    memvfs->file_handles = NULL;
    if (pthread_mutex_init(&memvfs->lock, NULL)) {
        tcbl_free(NULL, memvfs, sizeof(struct memvfs));
        return TCBL_ALLOC_FAILURE; // TODO more appropriate error code?
    }
    *vfs = (struct vfs *) memvfs;
    return TCBL_OK;
}
