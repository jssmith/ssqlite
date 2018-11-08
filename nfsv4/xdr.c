#include <nfs4_internal.h>

void push_auth_null(buffer b)
{
    push_be32(b, 0); // auth null
    push_be32(b, 0); // auth null body length
}

void push_auth_sys(buffer b, u32 uid, u32 gid)
{
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    push_be32(b, AUTH_SYS);
    b->end += 4;
    int start = buffer_length(b);
    // from rfc1057 pp 11
    // The "stamp" is an arbitrary ID which the caller machine may generate.
    push_be32(b, 0x01063369); 
    push_string(b, hostname, strlen(hostname));
    push_be32(b, uid); 
    push_be32(b, gid); 
    push_be32(b, 0); // linux client sends a set containing zero
    // wrap me
    *(u32 *)(b->contents + b->start + start -4) = htonl(buffer_length(b) - start);
}

static status parse_sequence(void *z, buffer b)
{
    nfs4 c = z;
    b->start+=16; // session id
    read_beu32(b); // sequenceid
    read_beu32(b); // slotid
    read_beu32(b); // highest slotid
    read_beu32(b); // target highest slotid
    read_beu32(b); // status flags
    return NFS4_OK;
}

void push_sequence(rpc r)
{
    push_op(r, OP_SEQUENCE, parse_sequence, r->c);
    r->session_offset = r->b->end; // is this always safe?
    push_session_id(r, r->c->session);
    printf ("push sequence %x\n", r->c->sequence);
    push_be32(r->b, r->c->sequence);
    push_be32(r->b, 0x00000000);  // slotid
    push_be32(r->b, 0x00000000);  // highest slotid
    push_be32(r->b, 0x00000000);  // sa_cachethis
    r->c->sequence++;
    r->response_length += 36;
}

void push_bare_sequence(rpc r)
{
    // xxx not sure what this sequence should be but seems ok as 0
    push_be32(r->b, 0);
}

void push_lock_sequence(rpc r)
{
    // xxx not sure what this sequence should be but seems ok as 0
    push_be32(r->b, 0);
}

void push_owner(rpc r)
{
    char own[64];
    // 5661 18.16.3 says server must ignore clientid    
    push_fixed_string(r->b, (void *)&r->c->clientid, sizeof(clientid));
    int len = sprintf(own, "open id:%lx", *(unsigned long *)r->c->instance_verifier);
    push_string(r->b, own, len);
}


void push_claim_null(buffer b, char *name)
{
    push_be32(b, CLAIM_NULL);
    push_string(b, name, strlen(name));
}

void push_claim_deleg_cur(buffer b, buffer name)
{
    push_be32(b, CLAIM_DELEGATE_CUR);
    // state id 4 
}

status parse_stateid(void *z, buffer b)
{
    stateid sid = z;
    sid->sequence = read_beu32(b); // seq
    return read_buffer(b, &sid->opaque, NFS4_OTHER_SIZE);
}


void push_stateid(rpc r, stateid s)
{
    push_be32(r->b, s->sequence);
    push_fixed_string(r->b, s->opaque, NFS4_OTHER_SIZE);
}

status parse_ace(void *z, buffer b)
{
    read_beu32(b); // type
    read_beu32(b); // flag
    read_beu32(b); // mask
    u32 namelen = read_beu32(b); 
    return read_buffer(b, 0, namelen);
}


status parse_change_info(void *z, buffer b)
{
    read_beu32(b); // atomic
    u32 before = read_beu64(b); // before
    u32 after = read_beu64(b); // after
    return NFS4_OK;
}

// containing directory stateid update included
status parse_open(void *z, buffer b)
{
    nfs4_file f = z;
    struct stateid delegation_sid;

    check(parse_stateid(&f->open_sid, b));
    f->latest_sid = f->open_sid;
    parse_change_info(0, b);
    read_beu32(b); // rflags
    buffer m = allocate_buffer(f->c->h, 10);
    parse_attrmask(b, m);
    u32 delegation_type = read_beu32(b);

    switch (delegation_type) {
    case OPEN_DELEGATE_NONE:
        break;
    case OPEN_DELEGATE_READ:
        check(parse_stateid(&delegation_sid, b));
        read_beu32(b); // recall
        parse_ace(0, b);
        break;
    case OPEN_DELEGATE_WRITE:
        check(parse_stateid(&delegation_sid, b));
        read_beu32(b); // recall
        u32 lt = read_beu32(b); // space limit - this is pretty ridiculous
        switch (lt) {
        case NFS_LIMIT_SIZE:
            read_beu64(b); // space bytes
        case NFS_LIMIT_BLOCKS:
            read_beu32(b); // nblocks
            read_beu64(b); // bytes per
        default:
            return error(NFS4_PROTOCOL, "bad limit size");
        }
        parse_ace(0, b);
        break;
    case OPEN_DELEGATE_NONE_EXT: /* New to NFSv4.1 */
        {
            u32 why = read_beu32(b);
            switch (why) {
            case WND4_CONTENTION:
                read_beu32(b); // ond_server_will_push_deleg;
            case WND4_RESOURCE:
                read_beu32(b); // server_will_signal avail
            }
        }
        break;
    default:
        return error(NFS4_PROTOCOL, "bad delegation return");
    }
}

void push_open(rpc r, char *name, int flags, nfs4_file f, nfs4_properties p)
{
    push_op(r, OP_OPEN, parse_open, f);
    push_be32(r->b, 0); // seqid
    u32 share_access = (flags & NFS4_WRONLY)? OPEN4_SHARE_ACCESS_BOTH : OPEN4_SHARE_ACCESS_READ;
    push_be32(r->b, share_access); // share access
    push_be32(r->b, 0); // share deny
    push_owner(r);
    if (flags & NFS4_CREAT) {
        buffer attr = allocate_buffer(r->c->h, 256); // leak
        push_be32(r->b, OPEN4_CREATE);
        push_be32(r->b, UNCHECKED4);
        push_fattr(r, p);
    } else {
        push_be32(r->b, OPEN4_NOCREATE);
    }
    push_claim_null(r->b, name);
}

status parse_create(void *z, buffer b)
{
    nfs4_properties a = z;
    u32 before = read_beu64(b); // before
    u32 after = read_beu64(b); // after
    parse_fattr(a, b);
}

status push_create(rpc r, nfs4_properties p)
{
    push_op(r,OP_CREATE, parse_create, p);
    push_be32(r->b, NF4DIR);
    push_string(r->b, p->name, strlen(p->name));
    push_fattr(r, p);
    return NFS4_OK;
}

status read_time(buffer b, ticks *dest)
{
    u64 seconds = read_beu64(b);
    u32 nanoseconds = read_beu32(b);
    // assuming unix epoch
    *dest  = (seconds << 32) |  ((u64)nanoseconds * 1000000000)/ (1ull<<32);
    return NFS4_OK;
}

status read_cstring(buffer b, char *dest, int len)
{
    u32 namelen = read_beu32(b);
    if (namelen > (len-1)) return error(NFS4_EFBIG, "");
    if (namelen > buffer_length(b)) return error(NFS4_PROTOCOL, "");            
    memcpy(dest, b->contents + b->start, namelen);
    dest[namelen] = 0;
    b->start += pad(namelen, 4);
    return NFS4_OK;
}

status parse_filehandle(void *z, buffer b)
{
    buffer dest = z;
    u32 len = read_beu32(b);
    if (len > NFS4_FHSIZE) return error(NFS4_PROTOCOL, "filehandle too large");
    if ((b->capacity - buffer_length(dest)) < len) return error(NFS4_PROTOCOL, "filehandle too large");    
    dest->end += len;
    read_buffer(b, dest->contents, len);
    return NFS4_OK;
}
                        

status discard_string(buffer b)
{
    u32 len = read_beu32(b);
    
    // bytes are followed by enough (0 to 3) residual zero bytes, r, to make 
    // the total byte count a multiple of four.
    len = (len + 3) & ~0x03;

    b->start += len;
}

status read_fs4_status(buffer b)
{
    read_beu32(b); //fss_absent
    read_beu32(b); //fss_type
    check(discard_string(b)); //fss_source
    check(discard_string(b)); //fss_current
    read_beu32(b);//fss_age
    ticks version;
    read_time(b, &version); //fss_version        
}

static void push_fhroot(rpc r)
{
    if (config_boolean("NFS_USE_ROOTFH", true)) {
        push_op(r, OP_PUTROOTFH, 0, 0);
    } else {
        push_op(r, OP_PUTFH, 0, 0);
        push_buffer(r->b, r->c->root_filehandle);
    }
}

char *push_initial_path(rpc r, char *path)
{
    int offset = 1;
    push_fhroot(r);
    for (int i = offset; path[i]; i++) 
        if (path[i] == '/') {
            push_op(r, OP_LOOKUP, 0, 0);
            push_string(r->b, path+offset, i-offset);            
            offset = i + 1;
        }
    return path + offset;
}

void push_resolution(rpc r, char *path)
{
    char *final = push_initial_path(r, path);

    if (*final) {
        push_op(r, OP_LOOKUP, 0, 0);
        push_string(r->b, final, strlen(final));
    }
}

status parse_dirent(buffer b, nfs4_properties p, int *more, u64 *cookie)
{
    u32 present = read_beu32(b);
    if (!present) {
        boolean eof = read_beu32(b);
        if (eof) {
            *more = 0;
        }
    } else {
        *cookie = read_beu64(b);
        check(read_cstring(b, p->name, sizeof(p->name)));
        parse_fattr(p, b);
        *more = 1;
    }
    return NFS4_OK;
}

void push_lock(rpc r, stateid sid, int locktype, bytes offset, bytes length, stateid n)
{
    push_op(r, OP_LOCK, parse_stateid, n);
    push_be32(r->b, locktype);
    push_boolean(r->b, false); // reclaim
    push_be64(r->b, offset);
    push_be64(r->b, length);
    
    push_boolean(r->b, true); // new lock owner
    push_bare_sequence(r);
    push_stateid(r, sid);
    push_lock_sequence(r);
    push_owner(r);
}

void push_unlock(rpc r, stateid sid, int locktype, bytes offset, bytes length)
{
    push_op(r, OP_LOCKU, 0, 0);
    push_be32(r->b, locktype);
    push_bare_sequence(r);
    push_stateid(r, sid);
    push_be64(r->b, offset);
    push_be64(r->b, length);
}
