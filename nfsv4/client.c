#include <nfs4_internal.h>
#include <time.h>

// api check can take an r i think
#define api_check(__d, __st) \
    ({                       \
        status __s = __st;\
        if (nfs4_is_error(__s))  __d->error_string = __s->description;  \
        (__s)?(__s)->error:0;                                         \
    })

#define api_checkr(__r, __st) \
    ({                       \
        status __s = __st;\
        if (nfs4_is_error(__s))  {__r->c->error_string = __s->description; deallocate_rpc(__r); return __s->error;} \
    })


char *nfs4_error_string(nfs4 n)
{
    return n->error_string->contents;
}

// should return the number of bytes read, can be short
int nfs4_pread(nfs4_file f, void *dest, bytes offset, bytes length)
{
    // size calc off by the headers
    api_check(f->c, segment(read_chunk, f->c->maxresp, f, dest, offset, length));
    return NFS4_OK;
}

int nfs4_pwrite(nfs4_file f, void *source, bytes offset, bytes length)
{
    // size calc off by the headers
    api_check(f->c, segment(write_chunk, f->c->maxreq, f, source, offset, length));
    return NFS4_OK;    
}


int nfs4_append(nfs4_file f, void *source, bytes length)
{
    // an atomic append has to be .. atomic, so it has to
    // fit in an option. it would be nice if we could
    // bound this exactly. use a flag so that large
    // non-contiguous or non-contending writes can proceed?
    if (length > (f->c->maxreq - 100)) {
        return api_check(f->c, error(NFS4_EFBIG, "request too large to preserve atomicity"));
    }
    
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_op(r, OP_VERIFY);
    struct nfs4_properties p;
    push_fattr(r, &p);
    buffer res = f->c->reverse;
    api_check(f->c, transact(r, OP_WRITE, res));    
}

static status parse_filehandle_open(buffer b, buffer fh)
{
    verify_and_adv(b, OP_GETFH);
    verify_and_adv(b, 0);
    return parse_filehandle(b, fh);
}

int nfs4_open(nfs4 c, char *path, int flags, nfs4_properties p, nfs4_file *dest)
{
    nfs4_file f = allocate(c->h, sizeof(struct nfs4_file));
    f->path = path;
    f->c = c;
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    push_open(r, final, flags, p);
    push_op(r, OP_GETFH);
    buffer res = f->c->reverse;    
    api_checkr(r, transact(r, OP_OPEN, res));
    api_checkr(r, parse_open(f, res));
    f->filehandle = allocate_buffer(c->h, NFS4_FHSIZE);
    api_checkr(r, parse_filehandle_open(res, f->filehandle));
    deallocate_rpc(r);
    *dest = f;
    return NFS4_OK;
}

int nfs4_close(nfs4_file f)
{
    deallocate(0, f, sizeof(struct nfs4_file));
}

int nfs4_stat(nfs4 c, char *path, nfs4_properties dest)
{
    status st = NFS4_OK;
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETATTR);    
    buffer res = c->reverse;    
    api_checkr(r, transact(r, OP_GETATTR, res));
    api_checkr(r, read_fattr(res, dest));
    deallocate_rpc(r);    
    return NFS4_OK;
}

int nfs4_fstat(nfs4_file f, nfs4_properties dest)
{
    rpc r = file_rpc(f);
    push_op(r, OP_GETATTR);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    buffer res = f->c->reverse;
    // wrap reclaim rpc
    api_checkr(r, transact(r, OP_GETATTR, res));
    api_checkr(r, read_fattr(res, dest));
    deallocate_rpc(r);    
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
    api_checkr(r, transact(r, OP_REMOVE, res));
    deallocate_rpc(r);    
    return NFS4_OK;
}

int nfs4_opendir(nfs4 c, char *path, nfs4_dir *dest)
{
    // use open?
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);    
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    buffer res = c->reverse;    
    api_checkr(r, transact(r, OP_GETFH, res));
    nfs4_dir d = allocate(0, sizeof(struct nfs4_dir));
    d->filehandle = allocate_buffer(0, NFS4_FHSIZE);
    d->entries = allocate_buffer(c->h, 4096);
    api_checkr(r, parse_filehandle(res, d->filehandle));
    d->cookie = 0;
    memset(d->verifier, 0, sizeof(d->verifier));
    d->c = c;
    *dest = d;
    deallocate_rpc(r);    
    return NFS4_OK;
}
                      
int nfs4_readdir(nfs4_dir d, nfs4_properties dest)
{
    int more = 1;
    if (!d->complete)  {
        if (length(d->entries)  == 0) 
            api_check(d->c, rpc_readdir(d, d->entries));
        // more could be in status
        api_check(d->c, parse_dirent(d->entries, dest, &more, &d->cookie));
        if (more) return NFS4_OK;
        d->complete = true;
    }
    return NFS4_ENOENT;   
}

int nfs4_closedir(nfs4_dir d)
{
    deallocate_buffer(d->entries);
}

// new filename in properties? could also take the mode from there
// this folds in with open 
int nfs4_mkdir(nfs4 c, char *path, nfs4_properties p)
{
    struct nfs4_properties real;
    // merge p and default into real
    real.mask = NFS4_PROP_MODE;
    real.mode = 0755;
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    char *term = push_initial_path(r, path);
    strncpy(real.name, term, strlen(term) + 1);
    push_create(r, &real);
    api_checkr(r, transact(r, OP_CREATE, c->reverse));
    deallocate_rpc(r);    
    return NFS4_OK;
}


int nfs4_synch(nfs4 c)
{
    while (vector_length(c->outstanding)) {
        boolean bs;
        read_response(c, c->reverse);
        // we stop after the first error - this is effectively fatal
        // for the session        
        api_check(c, parse_rpc(c, c->reverse, &bs));
    }
    return NFS4_OK;
}

// length -> 0 means truncate
// rename should also be here
int nfs4_change_properties(nfs4 c, char *path, nfs4_properties p)
{
    rpc r = allocate_rpc(c, c->forward);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_SETATTR);
    push_fattr(r, p);
    api_checkr(r, transact(r, OP_SETATTR, c->reverse));
    return NFS4_OK;
}

int nfs4_default_properties(nfs4 c, nfs4_properties p)
{
    c->default_properties = *p;
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
    api_checkr(r, transact(r, OP_LOCK, res));
    parse_stateid(f->c, res, &f->latest_sid);
    deallocate_rpc(r);
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
    api_checkr(r, transact(r, OP_LOCKU, res));
    api_checkr(r, parse_stateid(f->c, res, &f->latest_sid));
    deallocate_rpc(r);    
    return NFS4_OK;
}

int nfs4_create(char *hostname, nfs4 *dest)
{
    nfs4 c = allocate(0, sizeof(struct nfs4));
    
    c->hostname = allocate_buffer(0, strlen(hostname) + 1);
    push_bytes(c->hostname, hostname, strlen(hostname));
    // guarenteed to have room
    push_character(c->hostname, 0);

    c->h = 0;
    c->xid = 0xb956bea4;
    c->maxops = config_u64("NFS_OPS_LIMIT", 16);
    c->maxreqs = config_u64("NFS_REQUESTS_LIMIT", 32);
    c->forward = allocate_buffer(0, 16384);
    c->reverse = allocate_buffer(0, 16384);
    c->uid = NFS4_ID_ANONYMOUS;
    c->gid = NFS4_ID_ANONYMOUS;    

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
    api_check(c, rpc_connection(c));
    return NFS4_OK;
}
 

void nfs4_destroy(nfs4 d)
{
    // destroy heap
}


