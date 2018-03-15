#include <nfs4_internal.h>

void push_fixed_string(buffer b, char *x, u32 length) {
    u32 plen = pad(length, 4) - length;
    buffer_extend(b, length + plen + 4);
    memcpy(b->contents + b->end, x, length);
    b->end += length;
    if (plen) {
        memset(b->contents + b->end, 0, plen);
        b->end += plen;
    }
}

void push_string(buffer b, char *x, u32 length)
{
    u32 plen = pad(length, 4) - length;
    buffer_extend(b, 4);
    push_be32(b, length);
    push_fixed_string(b, x, length);
}


void push_channel_attrs(rpc r)
{
    push_be32(r->b, 0); // headerpadsize
    push_be32(r->b, r->c->maxreq); // maxreqsize
    push_be32(r->b, r->c->maxresp); // maxresponsesize
    push_be32(r->b, 1024*1024); // maxresponsesize_cached
    push_be32(r->b, r->c->maxops); // ca_maxoperations
    push_be32(r->b, r->c->maxreqs); // ca_maxrequests
    push_be32(r->b, 0); // ca_rdma_id
}

void push_session_id(rpc r, u8 *session)
{
    buffer_extend(r->b, NFS4_SESSIONID_SIZE);
    memcpy(r->b->contents + r->b->end, session, NFS4_SESSIONID_SIZE);
    r->b->end += NFS4_SESSIONID_SIZE;
}

void push_client_id(rpc r)
{
    buffer_extend(r->b, sizeof(clientid));
    memcpy(r->b->contents + r->b->end, &r->c->clientid, sizeof(clientid));
    r->b->end += sizeof(r->c->clientid);
}


// section 18.36, pp 513, rfc 5661
#define CREATE_SESSION4_FLAG_PERSIST 1
void push_create_session(rpc r)
{
    push_op(r, OP_CREATE_SESSION);
    push_client_id(r);
    push_be32(r->b, r->c->server_sequence);
    push_be32(r->b, CREATE_SESSION4_FLAG_PERSIST);
    push_channel_attrs(r); //forward
    push_channel_attrs(r); //return
    push_be32(r->b, NFS_PROGRAM);
    push_be32(r->b, 0); // auth params null
}

#define normalize(__c, __x)                                             \
    if (__c->__x > __x) {                                               \
        if (config_boolean("NFS_TRACE", false)) eprintf ("nfs server downgraded "#__x" from %ld to %ld\n", (u64)__c->__x, (u64)__x); \
        __c->__x = __x;                                                 \
    }

status parse_create_session(nfs4 c, buffer b)
{
    // check length - maybe do that generically in transact (parse callback, empty buffer)

    memcpy(c->session, b->contents + b->start, sizeof(c->session));
    b->start +=sizeof(c->session);
    read_beu32(b); // ?
    read_beu32(b); // flags

    // forward direction
    read_beu32(b); // headerpadsize
    u32 maxreq = read_beu32(b); // maxreqsize
    normalize(c, maxreq);
    u32 maxresp = read_beu32(b); // maxresponsesize
    normalize(c, maxresp);
    read_beu32(b); // maxresponsesize_cached
    u32 maxops = read_beu32(b); // ca_maxoperations
    normalize(c, maxops);    
    u32 maxreqs = read_beu32(b); // ca_maxrequests
    normalize(c, maxreqs);        
    read_beu32(b); // ca_rdma_id
    
    return NFS4_OK;
}

void push_sequence(rpc r)
{
    push_op(r, OP_SEQUENCE);
    push_session_id(r, r->c->session);
    push_be32(r->b, r->c->sequence);
    push_be32(r->b, 0x00000000);  // slotid
    push_be32(r->b, 0x00000000);  // highest slotid
    push_be32(r->b, 0x00000000);  // sa_cachethis
    r->c->sequence++;
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
    push_client_id(r); // 5661 18.16.3 says server must ignore clientid
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

void print_stateid(stateid sid )
{
    printf("stateid:%08x:", sid->sequence);
    for (int i = 0; i < NFS4_OTHER_SIZE; i++) {
        printf("%02x", sid->opaque[i]);
    }
}

status parse_stateid(nfs4 c, buffer b, stateid sid)
{
    sid->sequence = read_beu32(b); // seq
    status s = read_buffer(b, &sid->opaque, NFS4_OTHER_SIZE);
    // print_stateid(sid);
    return s;
}

status parse_ace(buffer b)
{
    read_beu32(b); // type
    read_beu32(b); // flag
    read_beu32(b); // mask
    u32 namelen = read_beu32(b); 
    return read_buffer(b, 0, namelen);
}

status parse_attrmask(nfs4 c, buffer b, buffer dest)
{
    u32 count = read_beu32(b);
    for (int i = 0 ; i < count; i++) {
        u32 m = read_beu32(b);
        for (int j = 0 ; j < 32; j++) 
            if (m  & (1 << j)) bitvector_set(dest, i*32 + j);
    }
}

// directory has a stateid update in here
status parse_open(nfs4_file f, buffer b)
{
    struct stateid delegation_sid;

    parse_stateid(f->c, b, &f->open_sid);
    memcpy(&f->latest_sid, &f->open_sid, sizeof(struct stateid));
    // change info
    read_beu32( b); // atomic
    u32 before = read_beu64(b); // before
    u32 after = read_beu64(b); // after
    read_beu32(b); // rflags
    buffer z = allocate_buffer(f->c->h, 10);
    parse_attrmask(f->c, b, z);
    u32 delegation_type = read_beu32(b);

    switch (delegation_type) {
    case OPEN_DELEGATE_NONE:
        break;
    case OPEN_DELEGATE_READ:
        parse_stateid(f->c, b, &delegation_sid);
        read_beu32(b); // recall
        parse_ace(b);
        break;
    case OPEN_DELEGATE_WRITE:
        parse_stateid(f->c, b, &delegation_sid);
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
        parse_ace(b);
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

void push_attr_bitmap(rpc r, buffer b)
{
    int len = length(b);
    int p = pad(len, 4);
    extend_total(b, p);
    push_be32(r->b, p/4);
    for (int i = 0; i<  length(b)/4; i++)
        push_be32(r->b, ((u32 *)b->contents)[i]);
}


void push_open(rpc r, char *name, int flags, nfs4_properties p)
{
    push_op(r, OP_OPEN);
    push_be32(r->b, 0); // seqid
    u32 share_access = (flags & NFS4_WRONLY)? OPEN4_SHARE_ACCESS_BOTH : OPEN4_SHARE_ACCESS_READ;
    push_be32(r->b, share_access); // share access
    push_be32(r->b, 0); // share deny
    push_owner(r);
    if (flags & NFS4_CREAT) {
        buffer attr = allocate_buffer(r->c->h, 256); // leak
        push_be32(r->b, OPEN4_CREATE);
        push_be32(r->b, UNCHECKED4);
        buffer mask = allocate_buffer(r->c->h, 8);
        bitvector_set(mask, FATTR4_MODE);
        //  bitvector_set(mask, FATTR4_OWNER);

        push_attr_bitmap(r, mask); 
        push_be32(attr, p->mode);
        // push_string(attr, "4134", 4);
        push_string(r->b, attr->contents, length(attr));
    } else {
        push_be32(r->b, OPEN4_NOCREATE);
    }
    push_claim_null(r->b, name);
}

void push_stateid(rpc r, stateid s)
{
    push_be32(r->b, s->sequence);
    push_fixed_string(r->b, s->opaque, NFS4_OTHER_SIZE);
}

// section 18.35, page 494, rfc 5661.txt
void push_exchange_id(rpc r)
{
    // this is supposed to be an instance descriptor that
    // spans multiple invocations of the same logical instance...
    // either expose in create_client or assume we will never
    // try to reclaim locks
    char co_owner_id[] = "sqlite.ip-172-31-27-113";
    // change this to something more appropriate
    char author[] = "kernel.org";
    char version[] = "Linux 4.4.0-1038-aws.#47-Ubuntu.SMP Thu Sep 28 20:05:35 UTC.2017 x86_64";
    push_op(r, OP_EXCHANGE_ID);

    // clientowner4
    push_bytes(r->b, r->c->instance_verifier, NFS4_VERIFIER_SIZE);
    push_string(r->b, co_owner_id, sizeof(co_owner_id) - 1);

    push_be32(r->b, 0); // flags

    push_be32(r->b, SP4_NONE); // state protect how
    push_be32(r->b, 1); // i guess a single impl id?
    push_string(r->b, author, sizeof(author) - 1); // authors domain name
    push_string(r->b, version, sizeof(version) - 1); // name
    push_be32(r->b, 0); // build date... populate
    push_be32(r->b, 0);
    push_be32(r->b, 0);
}

status parse_exchange_id(nfs4 c, buffer b)
{
    memcpy(&c->clientid, b->contents + b->start, sizeof(c->clientid));
    b->start += sizeof(c->clientid);
    c->server_sequence = read_beu32(b);
    u32 flags = read_beu32(b); // flags
    //    state_protect4_r eir_state_protect;
    //    server_owner4    eir_server_owner;
    //    opaque           eir_server_scope<NFS4_OPAQUE_LIMIT>;
    //    nfs_impl_id4     eir_server_impl_id<1>;
    return NFS4_OK;
}

status push_fattr_mask(rpc r, u64 mask)
{
    int count = 0;
    if (mask) count++;
    if (mask > (1ull<<32)) count ++;
    push_be32(r->b, count);
    u64 k = mask;
    for (int i = 0; i < count ; i++) {
        push_be32(r->b, (u32)k);
        k>>=32;
    }
    return NFS4_OK;
}


    
// just like in stat, we never set ino, size, access time or modify time
status push_fattr(rpc r, nfs4_properties p)
{
    // filter by supported types
    push_fattr_mask(r, p->mask);
    if (p->mask & NFS4_PROP_MODE) {
        push_be32(r->b, p->mode);
    }
    if (p->mask & NFS4_PROP_UID) {
        push_string(r->b, p->user, strlen(p->user));
    }
    if (p->mask & NFS4_PROP_GID) {
        push_string(r->b, p->group, strlen(p->group));
    }
    return NFS4_OK;
}

status push_create(rpc r, nfs4_properties p)
{
    push_op(r,OP_CREATE);
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
    if (namelen > length(b)) return error(NFS4_PROTOCOL, "");            
    memcpy(dest, b->contents + b->start, namelen);
    dest[namelen] = 0;
    b->start += pad(namelen, 4);
    return NFS4_OK;
}

status read_fattr(buffer b, nfs4_properties p)
{
    int masklen = read_beu32(b);
    u64 maskword = 0; // bitstring
    // need a table of throwaway lengths
    // can destructive use first bit set to cut this down
    for (int i = 0; i < masklen; i++) {
        u64 f = read_beu32(b);
        maskword |=  f << (32*i);
    }
    read_beu32(b); // the opaque length
    for (int j = 0; j < 64; j++) {
        if (maskword & (1ull<<j)) {
            // more general typecase
            switch(1ull<<j) {
            case NFS4_PROP_MODE:
                p->mode = read_beu32(b);
                break;
            case NFS4_PROP_UID:
                read_cstring(b, p->user, sizeof(p->user));
                break;
            case NFS4_PROP_GID:
                read_cstring(b, p->group, sizeof(p->group));
                break;
            case NFS4_PROP_SIZE:
                p->size = read_beu64(b);
                break;
            case NFS4_PROP_ACCESS_TIME:
                check(read_time(b, (ticks *)&p->access_time));
                break;
            case NFS4_PROP_MODIFY_TIME:
                check(read_time(b, (ticks *)&p->modify_time));
                break;
            default:
                printf ("why not supporto\n");
                return error(NFS4_EINVAL, "remote attribute not suported");
            }
            
        }
    }
    return NFS4_OK;
}

// this is a pretty sad iteration
status read_dirent(buffer b, nfs4_properties p, int *more, u64 *cookie)
{
    if (length(b) == 0) {
        *more = 1;
    } else {
        u32 present = read_beu32(b);
        if (!present) {
            *more = 1;
            boolean eof = read_beu32(b);
            if (eof) *more = 2;            
        } else {
            *cookie = read_beu64(b);
            check(read_cstring(b, p->name, sizeof(p->name)));
            read_fattr(b, p);
            *more = 0;
        }
    }
    return NFS4_OK;
}

status parse_filehandle(buffer b, u8 *dest)
{
    verify_and_adv(b, 0x80); // length
    return read_buffer(b, dest, NFS4_FHSIZE);    
}

                        

status discard_string(buffer b)
{
    u32 len = read_beu32(b);
    b->start += len*4;
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


