#include <string.h>
#include <sys/param.h>
#include "runtime.h"
#include "ll_log.h"

int tcbl_mem_init(tcbl_mem mem, size_t len)
{
    void *data = tcbl_malloc(NULL, len);
    if (data == NULL) {
        return TCBL_ALLOC_FAILURE;
    }
    mem->data = data;
    mem->len = len;
    return TCBL_OK;
}

int tcbl_mem_ensure_capacity(tcbl_mem mem, size_t len)
{
    if (mem->len < len) {
        size_t newlen = MAX(2 * mem->len, len);
        void *newdata = tcbl_malloc(NULL, newlen);
        if (newdata == NULL) {
            return TCBL_ALLOC_FAILURE;
        }
        memcpy(newdata, mem->data, mem->len);
        memset(&newdata[mem->len], 0, newlen - mem->len);
        tcbl_free(NULL, mem->data, mem->len);
        mem->data = newdata;
        mem->len = newlen;
    }
    return TCBL_OK;
}

int tcbl_mem_free(tcbl_mem mem)
{
    if (mem->data) {
        tcbl_free(NULL, mem->data, mem->len);
        mem->data = 0;
        mem->len = 0;
    }
    return TCBL_OK;
}


int tcbl_mem_log_init(tcbl_base_mem_log l) {
    l->n_entries = 0;
    int rc;
    rc = tcbl_mem_init(&l->cum_sizes, 100);
    if (rc) {
        return rc;
    }
    rc = tcbl_mem_init(&l->entries, 1000);
    if (rc) {
        tcbl_mem_free(&l->cum_sizes);
        return rc;
    }
    return TCBL_OK;
}

static size_t _tcbl_mem_data_size(tcbl_base_mem_log l)
{
    size_t sz;
    if (l->n_entries == 0) {
        sz = 0;
    } else {
        sz = ((size_t *)l->cum_sizes.data)[l->n_entries - 1];
    }
    return sz;
}

int tcbl_mem_log_append(tcbl_base_mem_log l, void* data, size_t len)
{
    size_t pre_size = _tcbl_mem_data_size(l);
    tcbl_mem_ensure_capacity(&l->cum_sizes, l->n_entries + sizeof(size_t));
    tcbl_mem_ensure_capacity(&l->entries, pre_size + len);
    ((size_t *)l->cum_sizes.data)[l->n_entries] = pre_size + len;
    // xx is this legal array indexing?
    memcpy(&((char *)l->entries.data)[pre_size], data, len);
    l->n_entries += 1;
    return TCBL_OK;
}

int tcbl_mem_log_length(tcbl_base_mem_log l, size_t *len_out)
{
    *len_out = l->n_entries;
    return TCBL_OK;
}

int tcbl_mem_log_concat(tcbl_base_mem_log l1, tcbl_base_mem_log l2)
{
    int rc;
    rc = tcbl_mem_ensure_capacity(&l1->cum_sizes, l1->n_entries + l2->n_entries);
    if (rc) return rc;

    size_t pre_size_1 = _tcbl_mem_data_size(l1);
    size_t pre_size_2 = _tcbl_mem_data_size(l2);
    rc = tcbl_mem_ensure_capacity(&l1->entries, pre_size_1 + pre_size_2);
    if (rc) return rc;

    size_t *pbegin = &((size_t *) &l1->cum_sizes.data)[l1->cum_sizes.len];
    size_t *psrc = (size_t *) &l2->cum_sizes.data;
    for (size_t *pdst = pbegin; pdst < &pbegin[l2->n_entries]; pdst++, psrc++) {
        *pdst = *psrc + pre_size_1;
    }
    memcpy(&((char *) l1->entries.data)[pre_size_1], l2->entries.data, pre_size_2);
    l1->n_entries += l2->n_entries;
    return TCBL_OK;
}

int tcbl_mem_log_get(tcbl_base_mem_log l, size_t index, void** out_data, size_t *out_len)
{
    if (index > l->n_entries) {
        return TCBL_BOUNDS_CHECK;
    }
    size_t data_offs;
    if (index == 0) {
        data_offs = 0;
    } else {
        data_offs = ((size_t *) l->cum_sizes.data)[index - 1];
    }
    *out_data = &l->entries.data[data_offs];
    if (out_len != NULL) {
        size_t data_len = ((size_t *) l->cum_sizes.data)[index - 1] - data_offs;
        *out_len = data_len;
    }
    return TCBL_OK;
}

int tcbl_mem_log_reset(tcbl_base_mem_log l)
{
    l->n_entries = 0;
    return TCBL_OK;
}

int tcbl_mem_log_free(tcbl_base_mem_log l)
{
    tcbl_mem_free(&l->cum_sizes);
    tcbl_mem_free(&l->entries);
    return TCBL_OK;
}

int tcbl_log_open_mem(tcbl_log log, tcbl_log_h lh)
{
    int rc;
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;
    h->log = log;

    tcbl_log_offs base_len;
    rc = tcbl_mem_log_length(&((tcbl_log_mem) log)->entries, &base_len);
    if (rc) return rc;
    h->base_len = base_len;

    rc = tcbl_log_seek((tcbl_log_h) h, base_len);
    if (rc) return rc;

    return tcbl_mem_log_init(&h->added_entries);
}

int tcbl_log_length_mem(tcbl_log log, tcbl_log_offs *out_offs)
{
//    tcbl_log_mem l = (tcbl_log_mem) log;x
    return TCBL_NOT_IMPLEMENTED;
}

int tcbl_log_reset_mem(tcbl_log_h lh)
{
    int rc;
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;

    rc = tcbl_mem_log_reset(&h->added_entries);
    if (rc) return rc;

    size_t base_len;
    rc = tcbl_mem_log_length(&((tcbl_log_mem) h->log)->entries, &base_len);
    if (rc) return rc;
    h->base_len = base_len;

    return TCBL_OK;
}

int tcbl_log_meld_mem(tcbl_log_h lh)
{
    int rc;
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;

    tcbl_base_mem_log parent_log = &((tcbl_log_mem) h->log)->entries;

    rc = tcbl_mem_log_concat(parent_log, &h->added_entries);
    if (rc) return rc;
    return tcbl_log_reset_mem(lh);
}

size_t log_entry_size(tcbl_log log, tcbl_log_entry entry)
{
    switch (entry->entry_type) {
        case LL_LOG_ENTRY_READ:
            return sizeof(struct ll_log_entry_read);
        case LL_LOG_ENTRY_WRITE:
            return sizeof(struct ll_log_entry_write) + log->block_size;
        case LL_LOG_ENTRY_BEGIN:
            return sizeof(struct ll_log_entry_begin);
        case LL_LOG_ENTRY_TRY_COMMIT:
            return sizeof(struct ll_log_entry_try_commit);
        default:
            return 0;
    }
}

int tcbl_log_append_mem(tcbl_log_h lh, tcbl_log_entry entry)
{
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;
    size_t sz = log_entry_size(lh->log, entry);
    // xxx update position to end
    return tcbl_mem_log_append(&h->added_entries, entry, sz);
}

int tcbl_log_seek_mem(tcbl_log_h lh, tcbl_log_offs seek_pos)
{
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;
    tcbl_log_mem parent_log = (tcbl_log_mem) h->log;

    if (seek_pos < h->base_len) {
        h->pos_log = &parent_log->entries;
        h->pos_log_offs = seek_pos;
    } else {
        h->pos_log = &h->added_entries;
        h->pos_log_offs = seek_pos - h->base_len;
    }
    // xx we allow seeking beyond the end of the log
    return TCBL_OK;
}

int tcbl_log_next_mem(tcbl_log_h lh, tcbl_log_entry *out_entry)
{
    int rc;
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;
    tcbl_log_mem parent_log = (tcbl_log_mem) h->log;

    if (h->pos_log == &parent_log->entries) {
        if (h->pos_log_offs == h->base_len) {
            h->pos_log = &h->added_entries;
            h->pos_log_offs = 0;
        }
    }
    if (h->pos_log == &h->added_entries) {
        if (h->pos_log_offs >= h->pos_log->n_entries) {
            *out_entry = NULL;
            return TCBL_OK;
        }
    }
    rc = tcbl_mem_log_get(h->pos_log, h->pos_log_offs, (void **) out_entry, NULL);
    if (rc) return rc;
    h->pos_log_offs += 1;
    return TCBL_OK;
}

int tcbl_log_close_mem(tcbl_log_h lh)
{
    tcbl_log_h_mem h = (tcbl_log_h_mem) lh;
    return tcbl_mem_log_free(&h->added_entries);
}

int tcbl_log_free_mem(tcbl_log log)
{
    tcbl_log_mem l = (tcbl_log_mem) log;
    return tcbl_mem_log_free(&l->entries);
}

int tcbl_log_init_mem(tcbl_log log, size_t block_size)
{
    static struct tcbl_log_ops ops = {
        tcbl_log_open_mem,
        tcbl_log_length_mem,
        tcbl_log_meld_mem,
        tcbl_log_reset_mem,
        tcbl_log_append_mem,
        tcbl_log_seek_mem,
        tcbl_log_next_mem,
        tcbl_log_close_mem,
        tcbl_log_free_mem
    };
    log->block_size = block_size;
    log->log_h_size = sizeof(struct tcbl_log_h_mem);
    memcpy(&log->ops, &ops, sizeof(struct tcbl_log_ops));
    return tcbl_mem_log_init(&((tcbl_log_mem) log)->entries);
}

int tcbl_log_open(tcbl_log log, tcbl_log_h lh)
{
    return log->ops.x_open(log, lh);
}

int tcbl_log_meld(tcbl_log_h lh)
{
    return lh->log->ops.x_meld(lh);
}

int tcbl_log_reset(tcbl_log_h lh)
{
    return lh->log->ops.x_reset(lh);
};

int tcbl_log_append(tcbl_log_h lh, tcbl_log_entry entry)
{
    return lh->log->ops.x_append(lh, entry);
}

int tcbl_log_seek(tcbl_log_h lh, tcbl_log_offs offs)
{
    return lh->log->ops.x_seek(lh, offs);
}

int tcbl_log_next(tcbl_log_h lh, tcbl_log_entry *out_entry)
{
    return lh->log->ops.x_next(lh, out_entry);
}

int tcbl_log_length(tcbl_log l, tcbl_log_offs *out_offs)
{
    return l->ops.x_length(l, out_offs);
}

int tcbl_log_close(tcbl_log_h lh)
{
    return lh->log->ops.x_close(lh);
}

int tcbl_log_free(tcbl_log log) {
    return log->ops.x_free(log);
}
