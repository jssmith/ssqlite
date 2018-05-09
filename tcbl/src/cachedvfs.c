#include <string.h>
#include <sys/param.h>
#include <stdbool.h>
#include "runtime.h"
#include "cachedvfs.h"
#include "sglib.h"

int vfs_cache_allocate(struct cvfs **cvfs, size_t page_size, size_t num_pages)
{
    int rc = TCBL_OK;
    struct cvfs *c = tcbl_malloc(NULL, sizeof(struct cvfs));
    if (c == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    c->num_pages = num_pages;
    c->page_size = page_size;
    c->cache_index = NULL;
    c->cache_data = NULL;
    c->num_index_entries = 2 * num_pages;
    c->lru_first = NULL;
    c->lru_last = NULL;

    size_t cache_index_sz = c->num_index_entries * sizeof(struct cvfs_entry);
    c->cache_index = tcbl_malloc(NULL, cache_index_sz);
    if (c->cache_index == NULL) {
        rc = TCBL_ALLOC_FAILURE;
        goto exit;
    }
    memset(c->cache_index, 0, c->num_index_entries);

    size_t cache_data_sz = num_pages * page_size;
    c->cache_data = tcbl_malloc(NULL, cache_data_sz);
    if (c->cache_data == NULL) {
        rc = TCBL_ALLOC_FAILURE;
        goto exit;
    }

    c->cache_free_list = c->cache_data;
    for (int i = 0; i < num_pages; i++) {
        *((char **) c->cache_data + i * page_size) = i < num_pages ? c->cache_data + (i + 1) * page_size : NULL;
    }
    c->len = 0;
    exit:
    if (rc) {
        if (c->cache_data) {
            tcbl_free(NULL, c->cache_data, cache_data_sz);
        }
        if (c->cache_index) {
            tcbl_free(NULL, c->cache_index, cache_index_sz);
        }
        tcbl_free(NULL, c, sizeof(struct cvfs));
    } else {
        *cvfs = c;
    }
    return rc;
}

uint64_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

static int vfs_cache_find(cvfs cvfs, size_t offs, bool create, cvfs_entry *out_entry, bool* out_did_create)
{
    cvfs_entry e;
    size_t cache_index_pos = hash(offs) % cvfs->num_index_entries;
    e = cvfs->cache_index[cache_index_pos];
    while (e->data != NULL) {
        cache_index_pos = (cache_index_pos + 1) % cvfs->num_index_entries;
        e = cvfs->cache_index[cache_index_pos];
        if (e->offset == offs) {
            *out_entry = e;
            if (out_did_create) {
                *out_did_create = false;
            }
            return TCBL_OK;
        }
        // TODO infinite loop if overfilled
    }
    if (create) {
        e->offset = offs;
        e->len = 0;
        if (cvfs->cache_free_list != NULL) {
            // take from free list
            e->data = cvfs->cache_free_list;
            cvfs->cache_free_list = *((char **) cvfs->cache_free_list);
        } else {
            // evict from LRU
            cvfs_entry ev = cvfs->lru_last;
            e->data = ev->data;
            ev->data = NULL;
            cvfs->lru_last = ev->lru_prev;
            cvfs->lru_last->lru_next = NULL;
        }
        // Add to LRU
        e->lru_next = cvfs->lru_first;
        e->lru_prev = NULL;
        if (cvfs->lru_first != NULL) {
            cvfs->lru_first->lru_prev = e;
        } else {
            cvfs->lru_last = e;
        }
        cvfs->lru_first = e;
        if (out_did_create) {
            *out_did_create = true;
        }
        return TCBL_OK;
    }
    return TCBL_NOT_FOUND;
}

int vfs_cache_open(cvfs cvfs, struct cvfs_h **cvfs_h, vfs_fh fill_fh)
{
    struct cvfs_h *h = tcbl_malloc(NULL, sizeof(struct cvfs_h));
    if (h == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    h->cvfs = cvfs;
    h->fill_fh = fill_fh;
    return TCBL_OK;
}

int vfs_cache_close(cvfs_h cvfs_h)
{
    tcbl_free(NULL, cvfs_h, sizeof(struct cvfs_h));
    return TCBL_OK;
}

int vfs_cache_get(cvfs_h cvfs_h, void* data, size_t offset, size_t len)
{
    int rc;
    cvfs_entry e;
    cvfs c = cvfs_h->cvfs;
    size_t page_size = c->page_size;

    size_t alignment_shift = offset % page_size;
    size_t block_offset = offset - alignment_shift;
    size_t block_len = ((offset + len - 1) / page_size + 1) * page_size - block_offset;

    char* dst = data;

    size_t read_offset = block_offset;
    size_t block_begin_skip = alignment_shift;
    size_t block_read_size = MIN(len, page_size - alignment_shift);

    while (read_offset < block_offset + block_len) {
        bool did_create;
        rc = vfs_cache_find(cvfs_h->cvfs, read_offset, true, &e, &did_create);
        if (rc) {
            return rc;
        }
        if (did_create) {
            rc = vfs_read(cvfs_h->fill_fh, e->data, read_offset, block_len);
            if (rc) {
                return rc;
            }
        }
        memcpy(dst, &e->data[block_begin_skip], block_read_size);

        dst += block_read_size;
        read_offset += page_size;
        block_begin_skip = 0;
        block_read_size = MIN(page_size, offset + len - read_offset);
    }
    return TCBL_OK;
}

int vfs_cache_update(cvfs_h cvfs_h, void* data, size_t offset, size_t len)
{
    int rc;
    cvfs_entry e;
    cvfs c = cvfs_h->cvfs;
    size_t page_size = c->page_size;

    size_t alignment_shift = offset % page_size;
    size_t block_offset = offset - alignment_shift;
    size_t block_len = ((offset + len - 1) / page_size + 1) * page_size - block_offset;

    char* src = data;

    size_t read_offset = block_offset;
    size_t block_begin_skip = alignment_shift;
    size_t block_write_size = MIN(len, page_size - alignment_shift);

    while (read_offset < block_offset + block_len) {
        rc = vfs_cache_find(cvfs_h->cvfs, read_offset, false, &e, NULL);
        if (rc == TCBL_OK) {
            memcpy(&e->data[block_begin_skip], src, block_write_size);
        } else if (rc == TCBL_NOT_FOUND) {
            goto next;
        } else {
            return rc;
        }
        next:
        src += block_write_size;
        read_offset += page_size;
        block_begin_skip = 0;
        block_write_size = MIN(page_size, offset + len - read_offset);
    }
    return TCBL_OK;
}

int vfs_cache_len_get(cvfs_h cvfs_h, size_t *out_len)
{
    *out_len = cvfs_h->cvfs->len;
    return TCBL_OK;
}

int vfs_cache_len_update(cvfs_h cvfs_h, size_t new_len)
{
    cvfs_h->cvfs->len = new_len;
    return TCBL_OK;
}
