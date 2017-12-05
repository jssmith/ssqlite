#include <nfs4_internal.h>
#include <sys/time.h>


static buffer print_path(heap h, vector v)
{
    buffer b = allocate_buffer(h, 100);
    buffer i;
    push_char(b, '/');
    foreach (i, v) {
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


//getting to be printf time
static void pf(char *header, file f)
{
    buffer b = print_path(0, f->path);
    push_char(b, 0);
    printf ("%s %s\n", header, (char *)b->contents);
}


status readfile(file f, void *dest, u64 offset, u32 length)
{
    return segment(read_chunk, 8192, f, dest, offset, length);
}

status writefile(file f, void *dest, u64 offset, u32 length)
{
    return segment(write_chunk, 8192, f, dest, offset, length);
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
    if (!is_ok(st)) return st;
    *dest = f;
    return STATUS_OK;
}

void file_close(file f)
{
    pf("close", f);
}

// permissions, user, a tuple?
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
    if (!is_ok(st)) return st;    
    *dest = f;
    return STATUS_OK;
}

// might as well implement something like stat()
status exists(client c, vector path)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    status st = transact(r, OP_GETFH);
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
    nfs4_connect(c, hostname);     
    c->xid = 0xb956bea4;
    c->b = allocate_buffer(0, 16384);
    
    // sketch
    struct timeval p;
    gettimeofday(&p, 0);
    memcpy(c->instance_verifier, &p.tv_usec, NFS4_VERIFIER_SIZE);
    
    rpc r = allocate_rpc(c);
    push_exchange_id(r);
    buffer b;
    transact(r, OP_EXCHANGE_ID);
    parse_exchange_id(c, b);

    r = allocate_rpc(c);
    // check - zero results in a seq error, it would be nice if
    // this were explicitly defined someplace
    r->c->sequence = 1;
    push_create_session(r);
    transact(r, OP_CREATE_SESSION);
    parse_create_session(c, r->b);

    r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    transact(r, OP_RECLAIM_COMPLETE);

    *dest = c;
    return STATUS_OK;
}
 
