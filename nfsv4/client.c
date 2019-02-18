#include <nfs4_internal.h>
#include <time.h>
#include <pwd.h>
#include <signal.h>

// consider not.. including?
void fill_default_user(nfs4_properties p)
{
    struct passwd *u = getpwent();
    p->user = u->pw_uid;
    p->group = u->pw_gid;
    // consider populating the vector of other group memberships
}


// we're passing through nfs error codes without translation
#define api_check(__d, __st) \
    ({                       \
        status __s = __st;\
        if (nfs4_is_error(__s)){  __d->error_string = __s->description; return __s->error;} \
        (__s)?(__s)->error:0;                                          \
    })

char *nfs4_error_string(nfs4 n)
{
    return n->error_string->contents;
}

// Read from f into dest and return the number of bytes read or -1 upon error
// the error is recorded in the nfs4 
int nfs4_pread(nfs4_file f, void *dest, bytes offset, bytes len)
{
    u64 total = 0;

    while (1) {
        rpc r = file_rpc(f);
        u64 transferred = push_read(r, offset,  dest + total, len - total, &f->latest_sid);
        status s;
        if (total < len) {
            // reestablish connection here
            s = rpc_send(r);
        } else {
            // block on all the preceeding(?) reads
            s = transact(r);
        }
        if (nfs4_is_error(s)) {
            if (total == 0) {
                f->c->error_string = s->description;
                f->c->nfs_error_num = s->error;
                return -1;
            } else {
                return total;
            }
        }
        total += transferred;
        offset += transferred;
        if (total >= len) {
            return total;
        }
    }
}

// Write from source into f and return bytes written or -1 upon error.
int nfs4_pwrite(nfs4_file f, void *source, bytes offset, bytes length)
{
    u64 total = 0;

    // not in the asynch case
    buffer b = alloca_wrap_buffer(source, length);
    while (1) {
        rpc r = file_rpc(f);
        u64 transferred = push_write(r, offset, b, &f->latest_sid);
        status s;
        if (buffer_length(b)) {
            s = rpc_send(r);
        } else {
            s = transact(r);
        }
        if (nfs4_is_error(s)) {
            if (total == 0) {
                f->c->error_string = s->description;
                f->c->nfs_error_num = s->error;
                return -1;
            } else {
                return total;
            }
        }
        total += transferred;
        offset += transferred;
        if (buffer_length(b) == 0) {
            return total;
        }
    }
}

static status set_expected_size(void *z, buffer b)
{
    nfs4_file f = z;
    struct nfs4_properties p;
    parse_fattr(&p, b);
    f->expected_size = p.size;
    return NFS4_OK;
}

int nfs4_append(nfs4_file f, void *source, bytes length)
{
    rpc r = file_rpc(f);
    struct nfs4_properties p;
    p.mask = NFS4_PROP_SIZE;
    p.size = f->expected_size;
    // get the offset in case we fail
    push_op(r, OP_GETATTR, set_expected_size, &f);    
    push_fattr_mask(r, NFS4_PROP_SIZE);
    push_op(r, OP_VERIFY, 0, 0);
    push_fattr(r, &p);
    push_op(r, OP_SETATTR, parse_attrmask, 0);
    push_stateid(r, &f->open_sid);    
    push_fattr(r, &p);    
    push_lock(r, &f->open_sid, WRITE_LT, f->expected_size, f->expected_size + length, &f->latest_sid);
    buffer b = alloca_wrap_buffer(source, length);
    u64 offset = f->expected_size;
        
    while (buffer_length(b)) {
        if (!r) r = file_rpc(f);
        // join
        offset += push_write(r, offset, b, &f->latest_sid);
        rpc_send(r);
        r = 0;
    }
    // drain
    
    r = file_rpc(f);
    push_unlock(r, &f->latest_sid, WRITE_LT, f->expected_size, f->expected_size + length);
    rpc_send(r);    
    // fix direct transmission of nfs4 error codes
    status s = transact(r);
    return api_check(f->c, s);
}

static status parse_getfilehandle(buffer b, buffer fh)
{
    verify_and_adv(b, OP_GETFH);
    verify_and_adv(b, 0);
    return parse_filehandle(fh, b);
}

status get_expected_size(void *z, buffer b)
{
    nfs4_file f = z;
    struct nfs4_properties p;
    parse_fattr(&p, b);
    f->expected_size = p.size;
    return NFS4_OK;
}

int nfs4_open(nfs4 c, char *path, int flags, nfs4_properties p, nfs4_file *dest)
{
    nfs4_file f = freelist_allocate(c->files);
    f->path = path;
    f->c = c;
    f->asynch_writes = flags & NFS4_SERVER_ASYNCH; 
    rpc r = allocate_rpc(f->c);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    // property merge
    push_open(r, final, flags, f, p);
    push_op(r, OP_GETFH, parse_filehandle, f->filehandle);
    push_op(r, OP_GETATTR, get_expected_size, f);

    push_fattr_mask(r, NFS4_PROP_SIZE);
    *dest = f;
    int nfs4_status = api_check(c, transact(r));
    // force write for trunc
    if (nfs4_status == NFS4_OK && (flags & NFS4_TRUNC)) {
        //struct nfs4_properties t;
        p->mask = NFS4_PROP_SIZE;
        p->size = 0;
        f->expected_size = 0;
        nfs4_change_properties(f, p); // nfs4_change_properties seems to be buggy
    } 
    return nfs4_status;
}

int nfs4_close(nfs4_file f)
{
    // release locks
    freelist_deallocate(f->c->files, f);
}

int nfs4_stat(nfs4 c, char *path, nfs4_properties dest)
{
    status st = NFS4_OK;
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETATTR, parse_fattr, dest);    
    push_fattr_mask(r, STANDARD_PROPERTIES);    
    return api_check(c, transact(r));
}

int nfs4_fstat(nfs4_file f, nfs4_properties dest)
{
    rpc r = file_rpc(f);
    push_op(r, OP_GETATTR, parse_fattr, dest);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    return api_check(r->c, transact(r));
}   

// underneath really a dir
int nfs4_unlink(nfs4 c, char *path)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    char *final = push_initial_path(r, path);
    push_op(r, OP_REMOVE, parse_change_info, 0);
    push_string(r->b, final, strlen(final));
    return api_check(c, transact(r));    
}

int nfs4_opendir(nfs4 c, char *path, nfs4_dir *dest)
{
    // use open?
    rpc r = allocate_rpc(c);
    push_sequence(r);    
    push_resolution(r, path);
    nfs4_dir d = freelist_allocate(c->dirs);
    d->ref = 0;
    d->cookie = 0;
    memset(d->verifier, 0, sizeof(d->verifier));    
    push_op(r, OP_GETFH, parse_filehandle, d->f.filehandle);
    *dest = d;
    return api_check(r->c, transact(r));    
}

// this swapping buffer descriptor dance could be eliminated if we
// develop a plan for zero-copy
int nfs4_readdir(nfs4_dir d, nfs4_properties dest)
{
    int valid = 1;
    if (!d->complete) {
        if (!d->ref || (buffer_length(&d->entries) == 0))
            api_check(d->f.c, rpc_readdir(d));
        api_check(d->f.c, parse_dirent(&d->entries, dest, &valid, &d->cookie));
        if (valid) {
            if (!buffer_length(&d->entries)) {
                deallocate_buffer(d->ref);
                d->ref = 0;
            }
            return NFS4_OK;
        }
        d->complete = true;
    }
    return NFS4_ENOENT;   
}

int nfs4_closedir(nfs4_dir d)
{
    if (d->ref)
        deallocate_buffer(d->ref);
}

// new filename in properties? could also take the mode from there
// this folds in with open 
int nfs4_mkdir(nfs4 c, char *path, nfs4_properties p)
{
    struct nfs4_properties real;
    // merge p and default into real
    real.mask = NFS4_PROP_MODE;
    real.mode = 0755;
    rpc r = allocate_rpc(c);
    push_sequence(r);
    char *term = push_initial_path(r, path);
    strncpy(real.name, term, strlen(term) + 1);
    push_create(r, &real);
    return api_check(r->c, transact(r));    
}


int nfs4_set_default_properties(nfs4 c, nfs4_properties p)
{
    // could write some cpp stuff around this but meh
    if (p->mask &NFS4_PROP_MODE) {
        c->default_properties.mask |= NFS4_PROP_MODE;
        c->default_properties.mode = p->mode;
    }
    if (p->mask &NFS4_PROP_USER) {
        c->default_properties.mask |= NFS4_PROP_USER;
        c->default_properties.user = p->user;
    }
    if (p->mask &NFS4_PROP_GROUP) {
        c->default_properties.mask |= NFS4_PROP_GROUP;
        c->default_properties.user = p->user;
    }        
}

int nfs4_synch(nfs4 c)
{
    boolean badsession;
    while (vector_length(c->outstanding))
        api_check(c, read_input(c, &badsession));
    return NFS4_OK;
}

// length -> 0 means truncate
// rename should also be here
// its turns out... that since modifying the size attribute is
// semantically the same as a write, that we need to have
// an open file stateid. we could try to hide that, but ...
int nfs4_change_properties(nfs4_file f, nfs4_properties p)
{
    rpc r = file_rpc(f);
    push_op(r, OP_SETATTR, 0, 0);
    push_stateid(r, &f->latest_sid);
    push_fattr(r, p);
    return api_check(f->c, transact(r));
}

int nfs4_default_properties(nfs4 c, nfs4_properties p)
{
    c->default_properties = *p;
}

int nfs4_lock_range(nfs4_file f, int locktype, bytes offset, bytes length)
{
    rpc r = file_rpc(f);
    push_lock(r, &f->open_sid, locktype, offset, length, &f->latest_sid);
    return api_check(r->c, transact(r));        
}

int nfs4_unlock_range(nfs4_file f, int locktype, bytes offset, bytes length)
{
    rpc r = file_rpc(f);
    push_unlock(r, &f->latest_sid, locktype, offset, length);
    return api_check(r->c, transact(r));            
}

static void *nfs4_allocate_rpc(void *z)
{
    nfs4 c = z;
    rpc r = allocate(c->h, sizeof(struct rpc));
    r->b = freelist_allocate(c->buffers);
    r->ops = allocate_vector(c->h, c->buffersize);
    return(r);
}

static void *nfs4_allocate_buffer(void *z)
{
    nfs4 c = z;
    return(allocate_buffer(c->h, c->buffersize));
}

static void *nfs4_allocate_remoteop(void *z)
{
    return allocate(((nfs4)z)->h, sizeof(struct remote_op));
}

static void *nfs4_allocate_file(void *z)
{
    nfs4 c = z;
    nfs4_file f = allocate(c->h, sizeof(struct nfs4_file));
    f->filehandle = allocate_buffer(c->h, NFS4_FHSIZE);
    return(f);
}

static void *nfs4_allocate_dir(void *z)
{
    nfs4 c = z;
    nfs4_dir d = allocate(c->h, sizeof(struct nfs4_dir));
    d->f.filehandle = allocate_buffer(c->h, NFS4_FHSIZE);
    d->f.c = c;
    return(d);
}

heap mallocheap;

int nfs4_create(char *hostname, nfs4 *dest)
{
    heap h = init_heap();
    mallocheap = h;
    nfs4 c = allocate(h, sizeof(struct nfs4));
    c->h = h;
    c->hostname = allocate_buffer(c->h, strlen(hostname) + 1);
    push_bytes(c->hostname, hostname, strlen(hostname));
    push_character(c->hostname, 0);

    c->rpcs = create_freelist(h, nfs4_allocate_rpc, c);
    c->buffers = create_freelist(h, nfs4_allocate_buffer, c);
    c->remoteops = create_freelist(h, nfs4_allocate_remoteop, c);
    c->files = create_freelist(h, nfs4_allocate_file, c);
    c->dirs = create_freelist(h, nfs4_allocate_dir, c);                
    
    c->xid = 0xb956bea4; // also randomize
    c->maxops = config_u64("NFS_OPS_LIMIT", 16);
    c->maxreqs = config_u64("NFS_REQUESTS_LIMIT", 32);

    c->default_properties.user = NFS4_ID_ANONYMOUS;
    c->default_properties.group = NFS4_ID_ANONYMOUS;    

    // this can be much larger
    c->maxresp = config_u64("NFS_READ_LIMIT", 65536);
    c->maxreq = config_u64("NFS_WRITE_LIMIT", 65536);
    c->buffersize = MAX(c->maxresp,  c->maxreq);
    c->error_string = allocate_buffer(c->h, 128);
    c->outstanding = allocate_vector(c->h, 5);
    fill_default_user(&c->default_properties);
    
    // would be nice if this werent a global property, or if it didnt exist at all
    if (config_boolean("NFS_IGNORE_SIGPIPE", true))
        signal(SIGPIPE, SIG_IGN);

    // xxx - we're actually using very few bits from tv_usec, make a better
    // instance id
    struct timeval p;
    gettimeofday(&p, 0);
    memcpy(c->instance_verifier, &p.tv_usec, NFS4_VERIFIER_SIZE);
    
    *dest = c;
    // defer connection to allow for override of user and group fields?
    api_check(c, rpc_connection(c));
    return NFS4_OK;
}
 

void nfs4_destroy(nfs4 d)
{
    destroy(d->h);
}


