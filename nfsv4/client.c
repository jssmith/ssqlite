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

// should return the number of bytes read, can be short
status readfile(file f, void *dest, u64 offset, u32 length)
{
    // size calc off by the headers
    return segment(read_chunk, f->c->maxresp, f, dest, offset, length);
}

status writefile(file f, void *dest, u64 offset, u32 length, u32 synch)
{
    // size calc off by the headers
    return segment(write_chunk, f->c->maxreq, f, dest, offset, length);
}

status file_open_read(client c, vector path, file *dest)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->c = c;
    *dest = f;
    rpc r = allocate_rpc(f->c, c->forward);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    buffer res = c->reverse;
    status st = transact(r, OP_GETFH, res);
    if (!is_ok(st)) {
        deallocate_rpc(r);
        return st;
    }
    st = read_buffer(f->c, res, &f->filehandle, NFS4_FHSIZE);
    deallocate_rpc(r);    
    if (!is_ok(st)) return st;
    return STATUS_OK;
}


static status file_open_internal(file f, vector path, boolean create)
{
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_open(r, final, create);
    push_op(r, OP_GETFH);
    buffer res = f->c->reverse;    
    status st = transact(r, OP_OPEN, res);
    // macro this shortcut return
    if (!is_ok(st)) {
        deallocate_rpc(r);
        return st;
    }
    st = parse_open(f, res);
    if (!is_ok(st)) {
        deallocate_rpc(r);
        return st;
    }
    verify_and_adv(f->c, res, OP_GETFH);
    verify_and_adv(f->c, res, 0); // status
    verify_and_adv(f->c, res, 0x80); // length
    st = read_buffer(f->c, res, &f->filehandle, NFS4_FHSIZE);
    deallocate_rpc(r);
    if (!is_ok(st)) return st;
    return STATUS_OK;
}

status file_open_write(client c, vector path, file *dest)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->c = c;
    *dest = f;
    return file_open_internal(f, path, false) ;
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
    *dest = f;
    return file_open_internal(f, path, true) ;
}

status exists(client c, vector path)
{
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    buffer res = c->reverse;    
    status st = transact(r, OP_GETFH, res);
    deallocate_rpc(r);    
    if (!is_ok(st)) return st;    
    return STATUS_OK;

}

status delete(client c, vector path)
{
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_op(r, OP_REMOVE);
    push_string(r->b, final->contents + final->start, length(final));
    buffer res = r->c->reverse;
    status s = transact(r, OP_REMOVE, res);
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
    
    c->hostname = allocate_buffer(0, strlen(hostname) + 1);
    push_bytes(c->hostname, hostname, strlen(hostname));
    push_char(c->hostname, 0);

    c->xid = 0xb956bea4;
    c->maxops = config_u64("NFS_OPS_LIMIT", 16);
    c->maxreqs = config_u64("NFS_REQUESTS_LIMIT", 32);
    c->forward = allocate_buffer(0, 16384);
    c->reverse = allocate_buffer(0, 16384);

    // xxx - we're actually using very few bits from tv_usec, make a better
    // instance id
    struct timeval p;
    gettimeofday(&p, 0);
    memcpy(c->instance_verifier, &p.tv_usec, NFS4_VERIFIER_SIZE);

    c->maxresp = config_u64("NFS_READ_LIMIT", 1024*1024);
    c->maxreq = config_u64("NFS_WRITE_LIMIT", 1024*1024);

    *dest = c;
    return rpc_connection(c);
}
 
