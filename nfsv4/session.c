#include <nfs4_internal.h>

#define checkr(__r, __st)    \
    ({                       \
        status __s = __st;   \
        if (nfs4_is_error(__s)) {freelist_deallocate(__r->c->rpcs, __r); return __s;} \
    })

#define normalize(__c, __x)                                             \
    if (__c->__x > __x) {                                               \
        if (config_boolean("NFS_TRACE", false)) eprintf ("nfs server downgraded "#__x" from %ld to %ld\n", (u64)__c->__x, (u64)__x); \
        __c->__x = __x;                                                 \
    }

void push_session_id(rpc r, u8 *session)
{
    buffer_extend(r->b, NFS4_SESSIONID_SIZE);
    memcpy(r->b->contents + r->b->end, session, NFS4_SESSIONID_SIZE);
    r->b->end += NFS4_SESSIONID_SIZE;
}

status parse_exchange_id(void *z, buffer b)
{
    nfs4 c = z;
    memcpy(&c->clientid, b->contents + b->start, sizeof(c->clientid));
    b->start += sizeof(c->clientid);
    c->server_sequence = read_beu32(b);
    u32 flags = read_beu32(b); // flags
    //    state_protect4_r eir_state_protect;
    //    server_owner4    eir_server_owner;
    b->start += 12;

    //    opaque           eir_server_scope<NFS4_OPAQUE_LIMIT>;
    discard_string(b);     
    
    //    opaque           eir_server_scope<NFS4_OPAQUE_LIMIT>;
    discard_string(b);        
    
    //    nfs_impl_id4     eir_server_impl_id<1>;

    // utf8str_cis   nii_domain;
    discard_string(b);
    // utf8str_cs    nii_name;
    discard_string(b);
    
    // nfstime4      nii_date;
    // int64_t         seconds;
    u64 seconds = read_beu64(b);
    // uint32_t        nseconds;
    u32 nseconds = read_beu32(b);

    return NFS4_OK;
}

// section 18.35, page 494, rfc 5661.txt
void push_exchange_id(rpc r)
{
    // this is supposed to be an instance descriptor that
    // spans multiple invocations of the same logical instance...
    // either expose in create_client or assume we will never
    // try to reclaim locks
    char co_owner_id[16];
    // For now we are populating with random data    
    fill_random(co_owner_id, sizeof(co_owner_id));    
    char author[] = "edu.berkeley";
    char version[] = "NFS for Serverless v. 0.1-snapshot";
    push_op(r, OP_EXCHANGE_ID, parse_exchange_id, r->c);

    // clientowner4
    push_bytes(r->b, r->c->instance_verifier, NFS4_VERIFIER_SIZE);
    push_string(r->b, co_owner_id, sizeof(co_owner_id));

    push_be32(r->b, 0); // flags

    push_be32(r->b, SP4_NONE); // state protect how
    push_be32(r->b, 1); // i guess a single impl id?
    push_string(r->b, author, sizeof(author) - 1); // authors domain name
    push_string(r->b, version, sizeof(version) - 1); // name
    push_be32(r->b, 0); // build date... populate
    push_be32(r->b, 0);
    push_be32(r->b, 0);
}

status exchange_id(nfs4 c)
{
    rpc r = allocate_rpc(c);
    push_exchange_id(r);
    boolean bs;
    checkr(r, base_transact(r, &bs));
    if (bs) {
        return error(NFS4_PROTOCOL, "bad session durng exchange id");
    }
    return NFS4_OK;
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

// section 18.36, pp 513, rfc 5661
#define CREATE_SESSION4_FLAG_PERSIST 1

status parse_create_session(void *z, buffer b)
{
    nfs4 c = z;
    // check length - maybe do that generically in transact (parse callback, empty buffer)

    memcpy(c->session, b->contents + b->start, sizeof(c->session));
    b->start +=sizeof(c->session);
    u32 seq = read_beu32(b); // ?
    c->sequence = seq;
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

    b->start += 7 * 4;
    
    return NFS4_OK;
}


status create_session(nfs4 c)
{
    rpc r = allocate_rpc(c);
    r->c->sequence = 1;  // 18.36.4 says that a new session starts at 1 implicitly
    r->c->lock_sequence = 1;
    push_op(r, OP_CREATE_SESSION, parse_create_session, r->c);
    push_fixed_string(r->b, (void *)&c->clientid, sizeof(clientid));    
    push_be32(r->b, r->c->server_sequence++);
    push_be32(r->b, CREATE_SESSION4_FLAG_PERSIST);
    push_channel_attrs(r); //forward
    push_channel_attrs(r); //return
    push_be32(r->b, NFS_PROGRAM);
    //push_auth_sys(r->b);
    
    push_be32(r->b, 0); // auth params null    
    boolean trash;
    checkr(r, base_transact(r, &trash));
    return NFS4_OK;
}


status destroy_session(nfs4 c)
{
    rpc r = allocate_rpc(c);
    push_op(r, OP_DESTROY_SESSION, 0, 0);
    push_session_id(r, c->session);
    boolean bs2;
    return base_transact(r, &bs2);
}

status reclaim_complete(nfs4 c)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE, 0, 0);
    // if true, current fh is the object for which reclaim is complete
    push_be32(r->b, 0);
    boolean bs;
    print_buffer("reclaim", r->b);
    checkr(r, base_transact(r, &bs));
    if (bs) return error(NFS4_PROTOCOL, "early session expiration during setup");
    return NFS4_OK;
}

status rpc_connection(nfs4 c)
{
    check(nfs4_connect(c));
    check(exchange_id(c));
    check(create_session(c));
    if (!config_boolean("NFS_USE_PUTROOTFH", false)){
        rpc r = allocate_rpc(c);
        push_sequence(r);
        push_op(r, OP_PUTROOTFH, 0, 0);
        c->root_filehandle = allocate_buffer(c->h, NFS4_FHSIZE);
        push_op(r, OP_GETFH, parse_filehandle, c->root_filehandle);
        check(transact(r));
    }
    return NFS4_OK;
    // return reclaim_complete(c);
}
