#include <nfs4_internal.h>


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
