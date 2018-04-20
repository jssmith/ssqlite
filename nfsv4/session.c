#include <nfs4_internal.h>

status exchange_id(nfs4 c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_exchange_id(r);
    buffer res = c->reverse;
    boolean bs;
    status st = base_transact(r, OP_EXCHANGE_ID, res, &bs);
    if (st) {
        deallocate_rpc(r);    
        return st;
    }
    st = parse_exchange_id(c, res);
    if (st) return st;
    deallocate_rpc(r);
    return NFS4_OK;
}


status get_root_fh(nfs4 c, buffer b)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_sequence(r);
    push_op(r, OP_PUTROOTFH);
    push_op(r, OP_GETFH);
    buffer res = c->reverse;
    boolean bs2;
    status st = base_transact(r, OP_GETFH, res, &bs2);
    if (nfs4_is_error(st)) {
        deallocate_rpc(r);
        return st;
    }
    parse_filehandle(res, b);
    deallocate_rpc(r);
    if (!nfs4_is_error(st)) return st;
    return NFS4_OK;
}

status create_session(nfs4 c)
{
    rpc r = allocate_rpc(c, c->reverse);
    r->c->sequence = 1;  // 18.36.4 says that a new session starts at 1 implicitly
    r->c->lock_sequence = 1;
    push_create_session(r);
    buffer res = c->reverse;
    status st = transact(r, OP_CREATE_SESSION, res);
    if (st) {
        deallocate_rpc(r);    
        return st;
    }    
    st = parse_create_session(c, res);
    if (st) return st;
    deallocate_rpc(r);
    return NFS4_OK;
}


status destroy_session(nfs4 c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_op(r, OP_DESTROY_SESSION);
    push_session_id(r, c->session);
    boolean bs2;
    return base_transact(r, OP_DESTROY_SESSION, c->reverse, &bs2);
}


// we can actually use the framing length to delineate 
// header and data, and read directly into the dest buffer
// because the data is always at the end
status read_chunk(nfs4_file f, void *dest, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_READ);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be32(r->b, length);
    buffer res = f->c->reverse;
    status s = transact(r, OP_READ, res);
    if (s) return s;
    // we dont care if its the end of file -- we might for a single round trip read entire
    res->start += 4; 
    u32 len = read_beu32(res);
    // guard against len != length
    memcpy(dest, res->contents+res->start, len);
    return NFS4_OK;
}

// if we break transact, can writev with the header and 
// source buffer as two fragments
// add synch
status write_chunk(nfs4_file f, void *source, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_WRITE);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be32(r->b, FILE_SYNC4);
    push_string(r->b, source, length);
    buffer b = f->c->reverse;
    return transact(r, OP_WRITE, b);
}


status segment(status (*each)(nfs4_file, void *, u64, u32),
            int chunksize,
            nfs4_file f,
            void *x,
            u64 offset,
            u32 length)
{
    for (u32 done = 0; done < length;) {
        u32 xfer = MIN(length - done, chunksize);
        status s = each(f, x + done, offset+done, xfer);
        if (s) return s;
        done += xfer;
    }
    return NFS4_OK;
}


status reclaim_complete(nfs4 c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    boolean bs;
    status st = base_transact(r, OP_RECLAIM_COMPLETE, c->reverse, &bs);
    deallocate_rpc(r);        
    return st;
}


status rpc_connection(nfs4 c)
{
    check(nfs4_connect(c));
    check(exchange_id(c));
    check(create_session(c));
    if (!config_boolean("NFS_USE_PUTROOTFH", false)) {
        c->root_filehandle = allocate_buffer(0, NFS4_FHSIZE);
        get_root_fh(c, c->root_filehandle);
    }
    return reclaim_complete(c);
}
