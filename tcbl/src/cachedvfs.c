#include <string.h>
#include <sys/param.h>
#include <inttypes.h>
#include <stdbool.h>
#include "runtime.h"
#include "cachedvfs.h"

//void check_free_list(char* l)
//{
//    char* p = l;
//    while (p != NULL) {
//        printf("fl: %p, %ld\n", (void *) p, p - l);
//        p = *((char **) p);
//    }
//}

static void vfs_cache_initialize(cvfs c)
{
    memset(c->cache_index, 0, c->num_index_entries * sizeof(struct cvfs_entry));
    c->cache_free_list = c->cache_data;
    for (int i = 0; i < c->num_pages; i++) {
        *((char **) (c->cache_data + i * c->page_size)) = i + 1 < c->num_pages ? c->cache_data + (i + 1) * c->page_size : NULL;
    }
    c->len = 0;
}

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

    size_t cache_data_sz = num_pages * page_size;
    c->cache_data = tcbl_malloc(NULL, cache_data_sz);
    if (c->cache_data == NULL) {
        rc = TCBL_ALLOC_FAILURE;
        goto exit;
    }

    vfs_cache_initialize(c);

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

int vfs_cache_free(cvfs cvfs)
{
    tcbl_free(NULL, cvfs->cache_index, cvfs->num_index_entries * sizeof(struct cvfs_entry));
    tcbl_free(NULL, cvfs->cache_data, cvfs->num_pages * cvfs->page_size);
    tcbl_free(NULL, cvfs, sizeof(struct cvfs));
    return TCBL_OK;
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
    e = &cvfs->cache_index[cache_index_pos];
    size_t orig_cache_index_pos = cache_index_pos;
//    printf("find at offset %ld\n", offs);
    while (e->data != NULL) {
        if (e->offset == offs) {
            *out_entry = e;
            if (out_did_create) {
                *out_did_create = false;
            }
            return TCBL_OK;
        } else {
            cache_index_pos = (cache_index_pos + 1) % cvfs->num_index_entries;
            if (cache_index_pos == orig_cache_index_pos) {
                // This should never happen because we allocate 2x the hash slots
                // as we have entries.
                return TCBL_INTERNAL_ERROR;
            }
            e = &cvfs->cache_index[cache_index_pos];
        }
    }
    if (create) {
        e->offset = offs;
        e->len = 0;
        if (cvfs->cache_free_list != NULL) {
            // take from free list
            e->data = cvfs->cache_free_list;
            cvfs->cache_free_list = *((char **) cvfs->cache_free_list);
//            if (cvfs->cache_free_list) {
//                printf("took from free list, free list now at %ld\n", cvfs->cache_free_list - cvfs->cache_data);
//            } else {
//                printf("took from free list, free list now empty\n");
//            }
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
        *out_entry = e;
        if (out_did_create) {
            *out_did_create = true;
        }
        return TCBL_OK;
    }
    return TCBL_NOT_FOUND;
}

int vfs_cache_open(cvfs cvfs, struct cvfs_h **cvfs_h,
                   int (*fill_fn)(void *, void *, size_t, size_t, size_t *),
                   void* fill_ctx)
{
    struct cvfs_h *h = tcbl_malloc(NULL, sizeof(struct cvfs_h));
    if (h == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    h->cvfs = cvfs;
    h->fill_fn = fill_fn;
    h->fill_ctx = fill_ctx;
    *cvfs_h = h;
    return TCBL_OK;
}

int vfs_cache_close(cvfs_h cvfs_h)
{
    tcbl_free(NULL, cvfs_h, sizeof(struct cvfs_h));
    return TCBL_OK;
}

int vfs_cache_get(cvfs_h cvfs_h, void* data, size_t offset, size_t len, size_t *out_len)
{
    int rc = TCBL_OK;
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

    size_t total_res_len = 0;
    while (read_offset < block_offset + block_len) {
        bool did_create;
        rc = vfs_cache_find(cvfs_h->cvfs, read_offset, true, &e, &did_create);
        if (rc) {
            return rc;
        }
        if (did_create) {
//            printf("filling data in range %ld %ld\n",
//                   e->data - c->cache_data,
//                   e->data - c->cache_data + page_size);
            rc = cvfs_h->fill_fn(cvfs_h->fill_ctx, e->data, read_offset, page_size, &e->entry_len);
            if (!(rc == TCBL_OK || rc == TCBL_BOUNDS_CHECK)) {
                return rc;
            }
        }
        memcpy(dst, &e->data[block_begin_skip], block_read_size);
        total_res_len += MIN(block_read_size, e->entry_len);
        if (block_read_size > e->entry_len) {
            rc = TCBL_BOUNDS_CHECK;
        }

        if (rc != TCBL_OK) {
            break;
        }
        dst += block_read_size;
        read_offset += page_size;
        block_begin_skip = 0;
        block_read_size = MIN(page_size, offset + len - read_offset);
    }
    if (out_len != NULL) {
        *out_len = total_res_len;
    }
    return rc;
}

int vfs_cache_update(cvfs_h cvfs_h, void* data, size_t offset, size_t len)
{
    int rc;
    cvfs_entry e;
    size_t page_size = cvfs_h->cvfs->page_size;

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

int vfs_cache_clear(cvfs_h cvfs_h)
{
    vfs_cache_initialize(cvfs_h->cvfs);
    return TCBL_OK;
}

int vfs_cache_len_get(cvfs_h cvfs_h, size_t *out_len)
{
    return TCBL_NOT_IMPLEMENTED;
//    *out_len = cvfs_h->cvfs->len;
//    return TCBL_OK;
}

int vfs_cache_len_update(cvfs_h cvfs_h, size_t new_len)
{
    cvfs_h->cvfs->len = new_len;
    return TCBL_OK;
}
