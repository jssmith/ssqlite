#include <nfs4_internal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <errno.h>

rpc allocate_rpc(nfs4 c) 
{
    rpc r = freelist_allocate(c->rpcs);
    buffer b = r->b;
    reset_buffer(r->b);
    r->xid = ++c->xid;
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
        // what does it mean to override this on a per-rpc basis?
        push_auth_sys(r->b, c->default_properties.user, c->default_properties.group);
    }
    push_auth_null(r->b); // verf

    // v4 compound
    push_be32(b, 0); // tag
    push_be32(b, 1); // minor version
    // this deferred write shows up elsewhere - wrap
    r->opcountloc = b->end;
    b->end += 4;
    r->c = c;
    print_buffer("start", r->b);
    return r;
}

static status parse_response(rpc r, buffer b)
{
    int opcount = read_beu32(b);
    status st = NFS4_OK;

    while (buffer_length(b)) {
        int op = read_beu32(b);
        if (config_boolean("NFS_TRACE", false))  {
            eprintf("received op: %C %x\n", nfsops, op, op);
            print_buffer("", b);
        }

        remote_op rop = fifo_pop(r->ops);

        if (!rop){
            st = error(NFS4_PROTOCOL, "extra data in response");
            break;
        }        
        
        u32 code = read_beu32(b);
        if (code != 0) {
            st = error(code, codestring(nfsstatus, code));
            break;
        }
        
        if (op != rop->op) {
            st =  error(NFS4_EINVAL, "operation mismatch expected %C got %C",
                            nfsops, op, nfsops, rop->op);
            break;
        }
        if (rop->parse) {
            st = rop->parse(rop->parse_argument, b);
            if (st) break;
        }
    }
    if (vector_length(r->ops)) {
        st = error(NFS4_PROTOCOL, "insufficient data in response");
    }    
    r->outstanding = false;
    freelist_deallocate(r->c->buffers, r->b);
    freelist_deallocate(r->c->rpcs, r);    
    return st;
}

status parse_rpc(nfs4 c, buffer b, boolean *badsession)
{
    *badsession = false;
    u32 xid = read_beu32(b);
    rpc r;

    vector p = c->outstanding;
    int len = vector_length(p);
    for (int i = 0; i < len; i++){
        r = vector_get(p, i);
        if (r && (r->xid == xid)) {
            vector_set(p, i, 0);
            break;            
        }
    }
    // error - unassociated response
    
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
            if (config_boolean("NFS_TRACE", false)){
                printf ("Bad session\n");
            }
            *badsession = true;
            return NFS4_OK;
        }
        return error(nstatus, codestring(nfsstatus, nstatus));
    }

    verify_and_adv(b, 0); // tag
    return parse_response(r, b);
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

status read_input(nfs4 c, boolean *badsession)
{
    status st = NFS4_OK;
    char framing[4];
    check(read_fully(c->fd, framing, 4));
    int frame = ntohl(*(u32 *)framing) & 0x07fffffff;
    buffer b = freelist_allocate(c->buffers);
    reset_buffer(b);
    eprintf("input: %d\n", frame);
    check(read_fully(c->fd, buffer_ref(b, 0), frame));
    b->end = frame;
    if (config_boolean("NFS_PACKET_TRACE", false)) {
        print_buffer("resp", b);
    }
    st = parse_rpc(c, b, badsession);
    freelist_deallocate(c->buffers, b);
    return st;
}

status rpc_send(rpc r)
{
    vector_push(r->c->outstanding, r);
    u32 oplen = vector_length(r->ops);
    *(u32 *)(r->b->contents + r->opcountloc) = htonl(oplen);
    // framer length
    *(u32 *)(r->b->contents) = htonl(0x80000000 + buffer_length(r->b)-4);
    if (config_boolean("NFS_PACKET_TRACE", false))
        print_buffer("sent", r->b);
    
    int res = write(r->c->fd, buffer_ref(r->b, 0), buffer_length(r->b));
    if (res != buffer_length(r->b)) {
        return error(NFS4_EIO, "failed rpc write");
    }
    r->outstanding = true;
    return NFS4_OK;
}

    
status nfs4_connect(nfs4 c)
{
    int temp;
    struct sockaddr_in a;

    struct hostent *he = gethostbyname(c->hostname->contents);
    if (!he)  return error(NFS4_ENODEV, "unable to resolve %s", c->hostname->contents);
    memcpy(&c->server_address, he->h_addr, 4);
    
    c->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);    
    // xxx - abstract
    memcpy(&a.sin_addr, &c->server_address, 4);
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
    if (res != 0) 
        return error(NFS4_ENXIO, "connect failure");
    return NFS4_OK;
}

rpc file_rpc(nfs4 c, buffer filehandle)
{
    rpc r = allocate_rpc(c);
    push_sequence(r);
    push_op(r, OP_PUTFH, 0, 0);
    push_string(r->b, filehandle->contents, buffer_length(filehandle));
    return (r);
}


status base_transact(rpc r, boolean *badsession)
{
    boolean myself = false;
    check(rpc_send(r));
    
    // drain the pipe, this should be per-slot...this drain doesnt that
    while (r->outstanding) check(read_input(r->c, badsession));
    
    return NFS4_OK;
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

status transact(rpc r)
{
    int tries = 0;
    boolean badsession = true;
    status s;
    
    while ((tries < 2) && (badsession == true)) {
        // xxx - if there are outstanding operations on the
        // old connection .. a late synch() will never
        // complete
        s = base_transact(r, &badsession);
        if (badsession) {
            check(rpc_connection(r->c));
            if (config_boolean("NFS_TRACE", false))
                eprintf("session failure, attempting to reestablish\n", 0);
            replay_rpc(r);
            tries++;
        }
    }
    return s;
}

