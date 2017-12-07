#include <nfs4_internal.h>


void push_string(buffer b, char *x, u32 length) {
    u32 plen = pad(length, 4) - length;
    buffer_extend(b, length + plen + 4);
    push_be32(b, length);
    memcpy(b->contents + b->end, x, length);
    b->end += length;
    if (plen) {
        memset(b->contents + b->end, 0, plen);
        b->end += plen;
    }
}

void push_channel_attrs(rpc r)
{
    push_be32(r->b, 0); // headerpadsize
    push_be32(r->b, 1024*1024); // maxreqsize
    push_be32(r->b, 1024*1024); // maxresponsesize
    push_be32(r->b, 1024*1024); // maxresponsesize_cached
    push_be32(r->b, 32); // ca_maxoperations
    push_be32(r->b, 10); // ca_maxrequests
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

    push_be32(r->b, r->c->sequence);
    push_be32(r->b, CREATE_SESSION4_FLAG_PERSIST);
    push_channel_attrs(r); //forward
    push_channel_attrs(r); //return
    push_be32(r->b, NFS_PROGRAM);
    push_be32(r->b, 0); // auth params null
}

status parse_create_session(client c, buffer b)
{
    // check length
    memcpy(c->session, b->contents + b->start, sizeof(c->session));
    b->start +=sizeof(c->session);
    c->sequence = read_beu32(c, b);
    return STATUS_OK;
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

void push_owner(rpc r)
{
    char own[12];
    push_client_id(r); // 5661 18.16.3 says server must ignore clientid
    sprintf(own, "open id:%lx", *(unsigned long *)r->c->instance_verifier);
    push_string(r->b, own, 12);
}

void push_claim_null(buffer b, buffer name)
{
    push_be32(b, CLAIM_NULL);
    push_string(b, name->contents + name->start, length(name));
}

void push_claim_deleg_cur(buffer b, buffer name)
{
    push_be32(b, CLAIM_DELEGATE_CUR);
    // state id 4 
}

void push_open(rpc r, buffer name, boolean create)
{
    push_op(r, OP_OPEN);
    push_be32(r->b, 0); // seqid
    push_be32(r->b, 2); // share access
    push_be32(r->b, 0); // share deny
    push_owner(r);
    if (create) {
        push_be32(r->b, OPEN4_CREATE);
        push_be32(r->b, UNCHECKED4);
        // xxx - encode this properly, this is the fattr for
        // the newly created file
        push_be32(r->b, 0x00000002);
        push_be32(r->b, 0x00000000);
        push_be32(r->b, 0x00000002);
        push_be32(r->b, 0x00000004);
        push_be32(r->b, 0x000001a4);
    } else {
        push_be32(r->b, OPEN4_NOCREATE);
    }
    push_claim_null(r->b, name);
}

void push_stateid(rpc r)
{
    // where do we get one of these?
    push_be32(r->b, 0); // seq
    push_be32(r->b, 0); // opaque
    push_be32(r->b, 0); 
    push_be32(r->b, 0);
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

    // xxx - recive flag usage, this is just copied from the kernel
    push_be32(r->b, EXCHGID4_FLAG_SUPP_MOVED_REFER |
              EXCHGID4_FLAG_SUPP_MOVED_MIGR  |
              EXCHGID4_FLAG_BIND_PRINC_STATEID);

    push_be32(r->b, SP4_NONE); // state protect how
    push_be32(r->b, 1); // i guess a single impl id?
    push_string(r->b, author, sizeof(author) - 1); // authors domain name
    push_string(r->b, version, sizeof(version) - 1); // name
    push_be32(r->b, 0); // build date... populate
    push_be32(r->b, 0);
    push_be32(r->b, 0);
}

status parse_exchange_id(client c, buffer b)
{
    memcpy(&c->clientid, b->contents + b->start, sizeof(c->clientid));
    b->start += sizeof(c->clientid);
    c->server_sequence = read_beu32(c, b);
    // xxx record the server sequence id for recovery, and stash the negotiated transfer limits
    //    clientid4        eir_clientid;
    //    sequenceid4      eir_sequenceid;
    //    uint32_t         eir_flags;
    //    state_protect4_r eir_state_protect;
    //    server_owner4    eir_server_owner;
    //    opaque           eir_server_scope<NFS4_OPAQUE_LIMIT>;
    //    nfs_impl_id4     eir_server_impl_id<1>;
    return STATUS_OK;
}
