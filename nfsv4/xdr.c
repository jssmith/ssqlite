#include <nfs4.h>

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

void push_client_id(rpc r, clientid id)
{
    buffer_extend(r->b, sizeof(clientid));
    memcpy(r->b->contents + r->b->end, &id, sizeof(clientid));
    r->b->end += sizeof(clientid);
}

#define CREATE_SESSION4_FLAG_PERSIST 1
void push_create_session(rpc r, clientid id, u32 sequence)
{
    push_op(r, OP_CREATE_SESSION);
    push_client_id(r, id);

    push_be32(r->b, sequence);
    push_be32(r->b, CREATE_SESSION4_FLAG_PERSIST);
    push_channel_attrs(r); //forward
    push_channel_attrs(r); //return
    push_be32(r->b, NFS_PROGRAM);
    push_be32(r->b, 0); // auth params null
}

void parse_create_session(server s, buffer b)
{
    verify_and_adv(b, 0); // why is there another nfs status here?
    // check length
    memcpy(s->session, b->contents + b->start, sizeof(s->session));
    b->start +=sizeof(s->session);
    s->sequence = read_beu32(b);
}


void push_sequence(rpc r, u8* session, u32 sequenceid)
{
    push_op(r, OP_SEQUENCE);
    push_session_id(r, session);
    push_be32(r->b, sequenceid); 
    push_be32(r->b, 0x00000000);  // slotid
    push_be32(r->b, 0x00000000);  // highest slotid
    push_be32(r->b, 0x00000000);  // sa_cachethis
    r->s->sequence++;
}

void push_auth_sys(rpc r)
{
    push_be32(r->b, 1);     // enum - AUTH_SYS
    buffer temp = allocate_buffer(0, 24);
    push_be32(temp, 0x01063369); // stamp
    char host[] = "ip-172-31-27-113";
    push_string(temp, host, sizeof(host)-1); // stamp
    push_be32(temp, 0); // uid
    push_be32(temp, 0); // gid
    push_be32(temp, 0); // gids
    push_string(r->b, temp->contents, length(temp));
}

rpc allocate_rpc(server s) 
{
    rpc r = allocate(s->h, sizeof(struct rpc));
    r->b = allocate_buffer(s->h, 100);

    // tcp framer
    push_be32(r->b, 0);
    
    // rpc layer 
    push_be32(r->b, ++s->xid);
    push_be32(r->b, 0); //call
    push_be32(r->b, 2); //rpcvers
    push_be32(r->b, NFS_PROGRAM);
    push_be32(r->b, 4); //version
    push_be32(r->b, 1); //proc
    //    push_auth_sys(r); // optional?
    push_be32(r->b, 0); //auth
    push_be32(r->b, 0); //authbody
    
    push_be32(r->b, 0); //verf
    push_be32(r->b, 0); //verf body kernel client passed the auth_sys structure

    // v4 compound
    push_be32(r->b, 0); // tag
    //push_string(r->b, "foo", 3); // tag
    push_be32(r->b, 1); // minor version
    // push_be32(r->b, 17); // cb ident 1
    // reserve the operation array size
    r->opcountloc = r->b->end;
    r->b->end += 4;
    
    r->s = s;
    r->opcount = 0;
    
    return r;
}

void push_lookup(rpc r, char *path)
{
    push_op(r, OP_LOOKUP);
    push_string(r->b, path, strlen(path));
}

boolean parse_rpc(server s, buffer b)
{
    verify_and_adv(b, s->xid);
    verify_and_adv(b, 1); // reply
    u32 status = read_beu32(b);
    if (status != NFS4_OK) {
        int i;
        for (i=0; (status_strings[i].id > 0) && (status_strings[i].id != status) ; i++);
        if (status_strings[i].id == -1 ){
            printf ("unknown error\n");
        } else {
            printf ("%s\n", status_strings[i].text);
        }
        return false;
    }
    
    verify_and_adv(b, 0); // status
    verify_and_adv(b, 0); // eh?
    verify_and_adv(b, 0); // verf
    verify_and_adv(b, 0); // verf
    verify_and_adv(b, 0); // nfs status
    verify_and_adv(b, 0); // tag
    return true;
}

void push_owner(buffer b, u64 clientid)
{
    char own[] = "sqlite";
    
    push_string(b, own, sizeof(own)- 1);
}

void push_claim_null(buffer b)
{

}


void push_open(rpc r, u64 clientid, boolean create)
{
    push_op(r, OP_OPEN);
    push_be32(r->b, 0); // seqid
    push_be32(r->b, 0); // share access
    push_be32(r->b, 0); // share deny
    push_owner(r->b, clientid);
    push_be32(r->b, create?OPEN4_CREATE:OPEN4_NOCREATE); // openhow
    push_be32(r->b, CLAIM_FH);
}


void push_stateid(rpc r)
{
    // where do we get one of these?
    push_be32(r->b, 0); // seq
    push_be32(r->b, 0); // opaque
    push_be32(r->b, 0); 
    push_be32(r->b, 0);
}

void push_exchange_id(rpc r)
{
    char coid[] = "Linux.NFSv4.1.ip-172-31-27-113";
    char author[] = "kernel.org";
    char version[] = "Linux 4.4.0-1038-aws.#47-Ubuntu.SMP Thu Sep 28 20:05:35 UTC.2017 x86_64";
    push_op(r, OP_EXCHANGE_ID);
    push_client_id(r, r->s->clientid);

    push_string(r->b, coid, sizeof(coid) - 1);
    push_be32(r->b, EXCHGID4_FLAG_SUPP_MOVED_REFER |
              EXCHGID4_FLAG_SUPP_MOVED_MIGR  |
              EXCHGID4_FLAG_BIND_PRINC_STATEID);
    push_be32(r->b, 0);
    push_be32(r->b,1);
    push_string(r->b, author, sizeof(author) - 1);
    push_string(r->b, version, sizeof(version) - 1);
    push_be32(r->b, 0);
    push_be32(r->b, 0);
    push_be32(r->b, 0);
}

void parse_exchange_id(server s, buffer b)
{
    verify_and_adv(b, 0); // why is there another nfs status here?
    memcpy(&s->clientid, b->contents + b->start, sizeof(s->clientid));
    b->start += sizeof(s->clientid);
    s->server_sequence = read_beu32(b);
    //    clientid4        eir_clientid;
    //    sequenceid4      eir_sequenceid;
    //    uint32_t         eir_flags;
    //    state_protect4_r eir_state_protect;
    //    server_owner4    eir_server_owner;
    //    opaque           eir_server_scope<NFS4_OPAQUE_LIMIT>;
    //    nfs_impl_id4     eir_server_impl_id<1>;
}
