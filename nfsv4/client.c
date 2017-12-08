#include <nfs4_internal.h>
#include <sys/time.h>

// replace with dedicated printf
buffer print_path(heap h, vector v)
{
    buffer b = allocate_buffer(h, 100);
    buffer i;
    push_char(b, '/');
    vector_foreach (i, v) {
        if (length(b) != 1) push_char(b, '/');
        buffer_concat(b, i);
    }
    return b;
}

buffer filename(file f)
{
    buffer z = join(0, f->path, '/');
    push_char(z, 0);
    return z;
}

static void pf(char *header, file f)
{
    buffer b = print_path(0, f->path);
    push_char(b, 0);
    printf ("%s %s\n", header, (char *)b->contents);
}

// change to negotiated sizes!
status readfile(file f, void *dest, u64 offset, u32 length)
{
    return segment(read_chunk, f->c->read_limit, f, dest, offset, length);
}

status writefile(file f, void *dest, u64 offset, u32 length)
{
    return segment(write_chunk, f->c->read_limit, f, dest, offset, length);
}

status file_open_read(client c, vector path, file *dest)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->c = c;
    *dest = f;
    // xxx lookup filehandle here - so we can ENOSUCHFILE
    return STATUS_OK;
}


status file_open_write(client c, vector path, file *dest)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->c = c;

    rpc r = allocate_rpc(f->c);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_open(r, final, false);
    // macro this shortcut return
    status st = transact(r, OP_OPEN);
    deallocate_rpc(r);
    if (!is_ok(st)) return st;
    *dest = f;
    return STATUS_OK;
}

void file_close(file f)
{
    // xxx - release delegations
}

// create a tuple interface to parameterize user/access/etc
status file_create(client c, vector path, file *dest)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->c = c;

    rpc r = allocate_rpc(f->c);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_open(r, final, true);
    status st = transact(r, OP_OPEN);
    deallocate_rpc(r);
    if (!is_ok(st)) return st;    
    *dest = f;
    return STATUS_OK;
}

status exists(client c, vector path)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    status st = transact(r, OP_GETFH);
    deallocate_rpc(r);    
    if (!is_ok(st)) return st;    
    return STATUS_OK;

}

status delete(client c, vector path)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    buffer final  = push_initial_path(r, path);
    push_op(r, OP_REMOVE);
    push_string(r->b, final->contents + final->start, length(final));
    status s= transact(r, OP_REMOVE);
    deallocate_rpc(r);
    if (!is_ok(s)) return s;    
    return STATUS_OK;
}

status readdir(client c, vector path, vector result)
{
}


status mkdir(client c, vector path)
{
}


status create_client(char *hostname, client *dest)
{
    client c = allocate(0, sizeof(struct client));
    c->packet_trace = false;
#if TRACE==2
    c->packet_trace = true;
#endif
    nfs4_connect(c, hostname);     
    c->xid = 0xb956bea4;
    // because the current implementation has no concurrency,
    // we use a single buffer for all transmit and receive
    c->b = allocate_buffer(0, 16384);
    
    // xxx - we're actually using very few bits from tv_usec, make a better
    // instance id
    struct timeval p;
    gettimeofday(&p, 0);
    memcpy(c->instance_verifier, &p.tv_usec, NFS4_VERIFIER_SIZE);
    
    rpc r = allocate_rpc(c);
    push_exchange_id(r);
    status st = transact(r, OP_EXCHANGE_ID);
    if (!is_ok(st)) {
        deallocate_rpc(r);    
        return st;
    }
    // xxx - extract from exhange id result
    c->read_limit = 8192;
    c->write_limit = 8192;
    st = parse_exchange_id(c, r->b);
    if (!is_ok(st)) return st;
    deallocate_rpc(r);

    r = allocate_rpc(c);
    // xxx - determine if we should officially start at 1 or if its part of some server exchange
    r->c->sequence = 1;
    push_create_session(r);
    st = transact(r, OP_CREATE_SESSION);
    if (!is_ok(st)) {
        deallocate_rpc(r);    
        return st;
    }    
    st = parse_create_session(c, r->b);
    if (!is_ok(st)) return st;
    deallocate_rpc(r);    

    r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    st = transact(r, OP_RECLAIM_COMPLETE);
    deallocate_rpc(r);        
    if (!is_ok(st)) return st;
    *dest = c;
    return STATUS_OK;
}
 
