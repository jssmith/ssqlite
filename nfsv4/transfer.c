#include <nfs4_internal.h>

// right now we are assuming that the amount of data being returned
// is the requested length, except possibly for the last fragment.
// not ideal, but just trying to avoid closing over length

status finish_read(void *dest, buffer b)
{
    u32 eof = read_beu32(b);
    u32 len = read_beu32(b);
    memcpy(dest, buffer_ref(b, 0), len);
    // assumes ordering

    // data is padded into multiple of 4
    // round length up to nearest multiple of 4
    u32 four_multiple_len = (len + 3) & ~0x03;
    b->start += four_multiple_len;
    return NFS4_OK;
}

u64 push_read(rpc r, bytes offset, void *dest, bytes length, stateid sid)
{
    push_op(r, OP_READ, finish_read, dest);
    push_stateid(r, sid);
    push_be64(r->b, offset);
    // minus any other return values
    // to do the natural thing and let the server truncate
    // to taste leads us with round trips
    // +4 is the eof flag
    u64 transfer = MIN(length, r->c->buffersize - r->response_length + 4);
    push_be32(r->b, transfer);
    return transfer;
}

status parse_write(void *dest, buffer b)
{
    // count4         count;
    u32 count = read_beu32(b);
    //stable_how4     committed;
    u32 committed = read_beu32(b);
    //verifier4       writeverf;
    b->start += NFS4_VERIFIER_SIZE;

    return NFS4_OK;
}

u64 push_write(rpc r, bytes offset, buffer b, stateid sid)
{
    push_op(r, OP_WRITE, parse_write, 0);
    push_stateid(r, sid);
    push_be64(r->b, offset);
    push_be32(r->b, FILE_SYNC4); // could pass
    u64 remaining = buffer_length(r->b);
    // always assume we should fill the outgoing buffer
    u64 transfer = MIN(buffer_length(b), r->b->capacity - r->b->start);    
    push_string(r->b, b->contents, transfer);
    b->start += transfer;
    return transfer;
}

static status extract_buffer(void *z, buffer b)
{
    nfs4_dir d = z;
    b->reference_count++;
    read_buffer(b, d->verifier, NFS4_VERIFIER_SIZE);
    d->ref = b;
    d->entries = *b;
    b->start = b->end;
    return NFS4_OK;
}

status rpc_readdir(nfs4_dir d)
{
    rpc r = file_rpc((nfs4_file)d);
    push_op(r, OP_READDIR, extract_buffer, d);
    push_be64(r->b, d->cookie); 
    push_bytes(r->b, d->verifier, sizeof(d->verifier));
    push_be32(r->b, d->f.c->maxresp); // per-entry length..make sure we get at least 1?
    push_be32(r->b, d->f.c->maxresp);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    return transact(r);
}
