#include <nfs4_internal.h>

// right now we are assuming that the amount of data being returned
// is the requested length, except possibly for the last fragment.
// not ideal, but just trying to avoid closing over length

status finish_read(void *dest, buffer b)
{
    u32 eof = read_beu32(b);
    u32 len = read_beu32(b);
    memcpy(dest, buffer_ref(b, 0), len);
}

u64 push_read(rpc r, bytes offset, buffer b, stateid sid)
{
    push_op(r, OP_READ, finish_read, 0);
    push_stateid(r, sid);
    push_be64(r->b, offset);
    u64 transfer = MIN(buffer_length(b), r->b->capacity - r->b->start);
    push_be32(r->b, transfer);
    return transfer;
    // always assume we should fill the outgoing buffer
}

u64 push_write(rpc r, bytes offset, buffer b, stateid sid)
{
    push_op(r, OP_WRITE, 0, 0);
    push_stateid(r, sid);
    push_be64(r->b, offset);
    push_be32(r->b, FILE_SYNC4); // could pass
    u64 remaining = buffer_length(r->b);
    // always assume we should fill the outgoing buffer
    u64 transfer = MIN(buffer_length(b), r->b->capacity - r->b->start);    
    push_string(r->b, b->contents, transfer);
    b->start += remaining;
    return transfer;
}

static status extract_buffer(void *z, buffer b)
{
    nfs4_dir d = z;
    b->reference_count++;
    read_buffer(b, d->verifier, NFS4_VERIFIER_SIZE);
    d->entries = b;
}

status rpc_readdir(nfs4_dir d, buffer *result)
{
    rpc r = file_rpc(d->c, d->filehandle);
    push_op(r, OP_READDIR, extract_buffer, d->verifier);
    push_be64(r->b, d->cookie); 
    push_bytes(r->b, d->verifier, sizeof(d->verifier));
    push_be32(r->b, d->c->maxresp); // per-entry length..make sure we get at least 1?
    push_be32(r->b, d->c->maxresp);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    return transact(r);
}
