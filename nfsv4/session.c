#include <nfs4_internal.h>


#define checkr(__r, __st) \
    ({                       \
        status __s = __st;\
        if (nfs4_is_error(__s))  {deallocate_rpc(__r); return __s;} \
    })


status exchange_id(nfs4 c)
{
    rpc r = allocate_rpc(c);
    r->prescan_op = OP_EXCHANGE_ID;
    push_exchange_id(r);
    boolean bs;
    checkr(r, base_transact(r, &bs));
    if (bs) {
        return error(NFS4_PROTOCOL, "bad session durng exhange id");
    }
    checkr(r, parse_exchange_id(c, r->result));
    deallocate_rpc(r);
    return NFS4_OK;
}


status get_root_fh(nfs4 c, buffer b)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_PUTROOTFH);
    push_op(r, OP_GETFH);
    transact(r, OP_GETFH);
    checkr(r, parse_filehandle(r->result, b));
    deallocate_rpc(r);
    return NFS4_OK;
}

status create_session(nfs4 c)
{
    rpc r = allocate_rpc(c);
    r->c->sequence = 1;  // 18.36.4 says that a new session starts at 1 implicitly
    r->c->lock_sequence = 1;
    push_create_session(r);
    boolean trash;
    r->prescan_op = OP_CREATE_SESSION;
    checkr(r, base_transact(r, &trash));
    checkr(r, parse_create_session(c, r->result));
    deallocate_rpc(r);
    return NFS4_OK;
}


status destroy_session(nfs4 c)
{
    rpc r = allocate_rpc(c);
    push_op(r, OP_DESTROY_SESSION);
    push_session_id(r, c->session);
    boolean bs2;
    r->prescan_op = OP_DESTROY_SESSION;
    return base_transact(r, &bs2);
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
    checkr(r, transact(r, OP_READ));
    // we dont care if its the end of file -- we might for a single round trip read entire
    u32 eof = read_beu32(r->result);
    u32 len = read_beu32(r->result);
    // guard against len != length
    memcpy(dest, buffer_ref(r->result, 0), len);
    return NFS4_OK;
}

static void ignore (void *x, buffer b)
{
}

// if we break transact, can writev with the header and 
// source buffer as two fragments
// add synch
status write_chunk(nfs4_file f, void *source, u64 offset, u32 size)
{
    rpc r = file_rpc(f);
    push_op(r, OP_WRITE);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    // modulatable
    push_be32(r->b, FILE_SYNC4);
    push_string(r->b, source, size);
    dprintf("asynch: %d\n", f->asynch_writes);
    if (f->asynch_writes) {
        r->completion = ignore;
        check(rpc_send(r));
        dprintf("sent\n", 0);
        return NFS4_OK;
    }
    return transact(r, OP_WRITE);    
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
        check(each(f, x + done, offset+done, xfer));
        done += xfer;
    }
    return NFS4_OK;
}

status reclaim_complete(nfs4 c)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    boolean bs;
    r->prescan_op = OP_RECLAIM_COMPLETE;
    checkr(r, base_transact(r, &bs));
    if (bs) return error(NFS4_PROTOCOL, "early session expiration during setup");
    return NFS4_OK;
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
