#include <nfs4_internal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <errno.h>


static void toggle(void *a, buffer b)
{
    *(int *)a = 1;
}

rpc allocate_rpc(nfs4 c) 
{
    // can use a single entity or a freelist
    rpc r = allocate(s->h, sizeof(struct rpc));
    buffer b = r->b = get_buffer(c);
    r->xid = ++c->xid;
    r->result = 0;
    r->completion = 0;
    push_be32(b, 0); // tcp framer - to be filled on transmit
    
    // rpc layer 
    push_be32(b, r->xid);
    push_be32(b, 0); //call
    push_be32(b, 2); //rpcvers
    push_be32(b, NFS_PROGRAM);
    push_be32(b, 4); //version
    push_be32(b, 1); //proc
    if (config_boolean("NFS_AUTH_NULL", false)) {
        push_auth_null(r->b);
    } else {
        push_auth_sys(r->b, c->uid, c->gid);
    }
    push_auth_null(r->b); // verf

    // v4 compound
    push_be32(b, 0); // tag
    push_be32(b, 1); // minor version
    // this deferred count shows up elsewhere - wrap
    r->opcountloc = b->end;
    b->end += 4;
    r->c = c;
    r->opcount = 0;
    
    return r;
}

status parse_rpc(nfs4 c, buffer b, boolean *badsession, rpc *r)
{
    *badsession = false;
    u32 xid = read_beu32(b);

    vector p = c->outstanding;
    int len = vector_length(p);
    for (int i = 0; i < len; i++){
        *r = vector_get(p, i);
        if (*r && ((*r)->xid == xid)) {
            vector_set(p, i, 0);
            break;            
        }
    }
    
    // poor compaction - use a map
    // peek?
    while (vector_length(p) && !vector_get(p, vector_length(p)-1))
        p->end -= sizeof (void *);
    
    verify_and_adv(b, 1); // rpc reply
    
    u32 rpcstatus = read_beu32(b);
    if (rpcstatus != NFS4_OK) 
        return error(NFS4_EINVAL, codestring(nfsstatus, rpcstatus));

    verify_and_adv(b, 0); // eh?

    verify_and_adv(b, 0); // verify auth null
    verify_and_adv(b, 0); // verify auth null

    u32 nstatus = read_beu32(b);
    if (nstatus != NFS4_OK) {
        if (config_boolean("NFS_TRACE", false))
            eprintf("nfs rpc error %s\n", codestring(nfsstatus, nstatus));

        if (nstatus == NFS4ERR_BADSESSION) {
            printf ("Bad session\n");
            *badsession = true;
            return NFS4_OK;
        }
        return error(NFS4_EINVAL, codestring(nfsstatus, nstatus));
    }

    verify_and_adv(b, 0); // tag
    return NFS4_OK;
}

static status read_fully(int fd, void* buf, size_t nbyte)
{
    ssize_t sz_read = 0;
    char* ptr = buf;
    while (sz_read < nbyte) {
        ssize_t bread = read(fd, ptr, nbyte);
        if (bread <= 0) {
            return error(NFS4_EIO,  strerror(errno));
        } else {
            sz_read += bread;
            // xxx can optimize this out in success case
            ptr += bread;
        }
    }
    return NFS4_OK;
}
 
 
status read_response(nfs4 c, buffer b)
{
    char framing[4];
    check(read_fully(c->fd, framing, 4));
    int frame = ntohl(*(u32 *)framing) & 0x07fffffff;
    buffer_extend(b, frame);
    check(read_fully(c->fd, b->contents + b->start, frame));
    b->end = frame;
    if (config_boolean("NFS_PACKET_TRACE", false)) {
        print_buffer("resp", b);
    }
    return NFS4_OK;
}


status rpc_send(rpc r)
{
    vector_push(r->c->outstanding, r);    
    *(u32 *)(r->b->contents + r->opcountloc) = htonl(r->opcount);
    // framer length
    *(u32 *)(r->b->contents) = htonl(0x80000000 + length(r->b)-4);
    if (config_boolean("NFS_PACKET_TRACE", false))
        print_buffer("sent", r->b);
    
    int res = write(r->c->fd, r->b->contents + r->b->start, length(r->b));
    if (res != length(r->b)) {
        return error(NFS4_EIO, "failed rpc write");
    }
    return NFS4_OK;
}

    
status nfs4_connect(nfs4 c)
{
    int temp;
    struct sockaddr_in a;

    struct hostent *he = gethostbyname(c->hostname->contents);
    memcpy(&c->address, he->h_addr, 4);
    
    c->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);    
    // xxx - abstract
    memcpy(&a.sin_addr, &c->address, 4);
    a.sin_family = AF_INET;
    a.sin_port = htons(2049); //configure

    if (config_boolean("NFS_TCP_NODELAY", true)) {
        unsigned char x = 1;
        setsockopt(c->fd, /*SOL_TCP*/0, TCP_NODELAY,
                   (char *)&x, sizeof(x));
    }
    
    int res = connect(c->fd,
                      (struct sockaddr *)&a,
                      sizeof(struct sockaddr_in));
    if (res != 0) {
        // make printf status variant
        return error(NFS4_ENXIO, "connect failure");
    }
    return NFS4_OK;
}

static status read_until(rpc r)
{
    buffer b = r->result;
    int opcount = read_beu32(b);
    while (1) {
        int op =  read_beu32(b);
        if (config_boolean("NFS_TRACE", false)) 
            dprintf("received op: %C\n",nfsops, op);

        if (op == r->prescan_op) {
            return NFS4_OK;
        }
        u32 code = read_beu32(b);
        if (code != 0) return error(NFS4_EINVAL, codestring(nfsstatus, code));

        switch (op) {
        case OP_SEQUENCE:
            b->start += NFS4_SESSIONID_SIZE; // 16
            u32 seq = read_beu32(b);  // 20
            b->start += 16; // 36
            break;
        case OP_PUTROOTFH:
            break;
        case OP_PUTFH:
            break;
        case OP_LOOKUP:
            break;
        case OP_WRITE:
            {
                // if we care about any of these..
                /*struct stateid callback_id;
                  parse_stateid(b, &callback_id);
                  read_beu32(b); //length
                  read_beu32(b); //stablehow4 committed
                  u8 verf[NFS4_VERIFIER_SIZE];
                  read_verifier(b, verf); //stablehow4 committed
                */
                break;
            }
        default:
            return error(NFS4_PROTOCOL, "unhandled scan code %C", nfsops, op);
        }
    }
    // fix
    return NFS4_OK;
}

rpc file_rpc(nfs4_file f)
{
    rpc r = allocate_rpc(f->c);
    push_sequence(r);
    push_op(r, OP_PUTFH);
    push_string(r->b, f->filehandle->contents, length(f->filehandle));
    return (r);
}


status base_transact(rpc r, boolean *badsession)
{
    int myself = 0;
    r->completion = toggle;
    r->completion_argument = &myself;
    check(rpc_send(r));
    buffer result = get_buffer(r->c);
    // drain the pipe, this should be per-slot
    // any messages that need more than basic parsing should error out
    while (!myself) {
        rpc recv;
        result->start = result->end = 0;
        check(read_response(r->c, result));        
        check(parse_rpc(r->c, result, badsession, &recv));
        r->result = result;
        check(read_until(r));
        if (r->completion) r->completion(r->completion_argument, result);
    }
    u32 code = read_beu32(r->result);
    if (code == 0) return NFS4_OK;
    return error(NFS4_EINVAL, codestring(nfsstatus, code));    
}


static status replay_rpc(rpc r)
{
    memcpy(r->b->contents + r->session_offset, r->c->session, NFS4_SESSIONID_SIZE);
    u32 nseq = htonl(r->c->sequence);
    r->c->sequence++;
    memcpy(r->b->contents + r->session_offset + NFS4_SESSIONID_SIZE, &nseq, 4);
    r->xid = ++r->c->xid;
    r->b->start = 0;
    u32 nxid = htonl(r->xid);
    memcpy(r->b->contents + r->b->start +4, &nxid, 4);
    return NFS4_OK;
}

status transact(rpc r, int op)
{
    int tries = 0;
    boolean badsession = true;
    status s;
    r->prescan_op = op;
    
    while ((tries < 2) && (badsession == true)) {
        // xxx - if there are outstanding operations on the
        // old connection .. a late synch() will never
        // complete
        s = base_transact(r, &badsession);
        if (badsession) {
            check(rpc_connection(r->c));
            if (config_boolean("NFS_TRACE", false))
                eprintf("session failure, attempting to reestablish\n");
            replay_rpc(r);
            free_buffer(r->c, r->result);
            tries++;
        }
    }
    return s;
}


status rpc_readdir(nfs4_dir d, buffer *result)
{
    rpc r = allocate_rpc(d->c);
    push_sequence(r);
    push_op(r, OP_PUTFH);
    push_string(r->b, d->filehandle->contents, length(d->filehandle));
    push_op(r, OP_READDIR);
    push_be64(r->b, d->cookie); 
    push_bytes(r->b, d->verifier, sizeof(d->verifier));
    push_be32(r->b, 512); // per-entry length..maybe this should just be maxresult?
    push_be32(r->b, d->c->maxresp);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    check(transact(r, OP_READDIR));
    read_buffer(r->result, d->verifier, NFS4_VERIFIER_SIZE);
    *result = r->result;
    return NFS4_OK;
}
