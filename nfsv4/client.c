#include <nfs4_internal.h>
#include <sys/time.h>

char *nfs4_error_string(nfs4 n)
{
    return n->error_string->contents;
}

// should return the number of bytes read, can be short
int nfs4_pread(nfs4_file f, void *dest, bytes offset, bytes length)
{
    // size calc off by the headers
    return segment(read_chunk, f->c->maxresp, f, dest, offset, length);
}

int nfs4_pwrite(nfs4_file f, void *dest, bytes offset, bytes length)
{
    // size calc off by the headers
    return segment(write_chunk, f->c->maxreq, f, dest, offset, length);
}

int nfs4_open(nfs4 c, char *path, int flags, nfs4_mode_t mode, nfs4_file *dest)
{
    nfs4_file f = allocate(s->h, sizeof(struct nfs4_file));
    f->path = path;
    f->c = c;
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    push_open(r,final, flags);
    push_op(r, OP_GETFH);
    buffer res = f->c->reverse;    
    status st = transact(r, OP_OPEN, res);
    // macro this shortcut return
    if (st) {
        deallocate_rpc(r);
        return st;
    }
    st = parse_open(f, res);
    if (st) {
        deallocate_rpc(r);
        return st;
    }
    verify_and_adv(c, res, OP_GETFH);
    verify_and_adv(c, res, 0); // status
    verify_and_adv(c, res, 0x80); // length
    st = read_buffer(c, res, &f->filehandle, NFS4_FHSIZE);
    deallocate_rpc(r);
    if (st) return st;
    *dest = f;
    return NFS4_OK;
}

int nfs4_close(nfs4_file f)
{
    deallocate(0, f, sizeof(struct nfs4_file));
    
}

int nfs4_stat(nfs4 c, char *path, nfs4_properties dest)
{
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    buffer res = c->reverse;    
    status st = transact(r, OP_GETFH, res);
    deallocate_rpc(r);    
    if (st) return st;    
    return NFS4_OK;

}

int nfs4_unlink(nfs4 c, char *path)
{
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    push_op(r, OP_REMOVE);
    push_string(r->b, final, strlen(final));
    buffer res = r->c->reverse;
    status s = transact(r, OP_REMOVE, res);
    deallocate_rpc(r);
    if (s) return s;    
    return NFS4_OK;
}

int nfs4_opendir(nfs4 c, char *path, nfs4_dir *dest)
{
    nfs4_dir d = allocate(0, sizeof(struct nfs4_dir));
    nfs4_file f;
    int res = nfs4_open(c, path, NFS4_RDONLY, 0000, &f);
    if (res != 0) {
        return res;
    }
    *dest = d;
}


int nfs4_closedir(nfs4_dir d)
{
    nfs4_close(d->f);
}

int nfs4_readdir(nfs4_dir d, nfs4_properties *dest)
{
}

int nfs4_mkdir(nfs4 c, char *path)
{
    struct nfs4_properties p;
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    push_create(r, &p);
    return transact(r, OP_CREATE, c->reverse);    
}


int nfs4_synch(nfs4 c)
{
    while (vector_length(c->outstanding)) {
        boolean bs;
        read_response(c, c->reverse);
        int k = parse_rpc(c, c->reverse, &bs);
        // we stop after the first error - this is effectively fatal
        // for the session
        if (k) return k;
    }
    return NFS4_OK;
}

int nfs4_lock_range(nfs4_file f, int locktype, bytes offset, bytes length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_LOCK);
    push_be32(r->b, locktype);
    push_boolean(r->b, false); // reclaim
    push_be64(r->b, offset);
    push_be64(r->b, length);

    push_boolean(r->b, true); // new lock owner
    push_bare_sequence(r);
    push_stateid(r, &f->open_sid);
    push_lock_sequence(r);
    push_owner(r);

    buffer res = f->c->reverse;
    status s = transact(r, OP_LOCK, res);
    if (s) return s;
    parse_stateid(f->c, res, &f->latest_sid);
    return NFS4_OK;
}


int nfs4_unlock_range(nfs4_file f, int locktype, bytes offset, bytes length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_LOCKU);
    push_be32(r->b, locktype);
    push_bare_sequence(r);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be64(r->b, length);
    buffer res = f->c->reverse;
    status s = transact(r, OP_LOCKU, res);
    if (s) return s;
    parse_stateid(f->c, res, &f->latest_sid);
    return NFS4_OK;
}

int nfs4_create(char *hostname, nfs4 *dest)
{
    nfs4 c = allocate(0, sizeof(struct nfs4));
    
    c->hostname = allocate_buffer(0, strlen(hostname) + 1);
    push_bytes(c->hostname, hostname, strlen(hostname));
    push_char(c->hostname, 0);

    c->h = 0;
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
    c->error_string = allocate_buffer(c->h, 128);
    c->outstanding = allocate_vector(c->h, 5);

    *dest = c;
    return rpc_connection(c);
}
 

void nfs4_destroy(nfs4 d)
{
    // destroy heap
}


