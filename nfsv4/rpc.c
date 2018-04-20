#include <nfs4_internal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <errno.h>

extern struct codepoint nfsops[];

static void toggle(void *a, buffer b)
{
    *(int *)a = 1;
}

// stack like allocation?
rpc allocate_rpc(nfs4 c, buffer b) 
{
    // can use a single entity or a freelist
    rpc r = allocate(s->h, sizeof(struct rpc));
    r->b = b;
    b->start = b->end = 0;
    r->xid = ++c->xid;
    
    // tcp framer - to be filled on transmit
    push_be32(b, 0);
    
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
        push_auth_sys(r->b);
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

// sad, but low on memory pressure, and trying to avoid pulling in
// more runtime (i.e. maps)
static void enqueue_completion(nfs4 c, u32 xid, void (*f)(void *, buffer), void *a)
{
    int len = vector_length(c->outstanding);
    int i;
    for (i = 0; i < len; i+=3) 
        if (vector_get(c->outstanding, i) == 0)
            break;
    vector_set(c->outstanding, i, (void *)(u64)xid);
    vector_set(c->outstanding, i + 1, f);
    vector_set(c->outstanding, i + 2, a);
}

static status completion_notify(nfs4 c, u32 xid)
{
    vector p = c->outstanding;
    int len = vector_length(p);
    for (int i = 0; i < len; i+=3) {
        if ((u64)vector_get(p, i) == xid) {
            void (*f)(void *) = vector_get(p, i +1);
            if (f) f(vector_get(p, i+2));
            vector_set(p, i, 0);
            break;
        }
    }

    // compaction
    while (vector_length(p) && !vector_get(p, vector_length(p)-3))
        p->end -= 3*sizeof (void *);
}

status parse_rpc(nfs4 c, buffer b, boolean *badsession)
{
    *badsession = false;
    u32 xid = read_beu32(b);
    status s = completion_notify(c, xid);
    if (s) return(s);
        
    verify_and_adv(b, 1); // rpc reply
    
    u32 rpcstatus = read_beu32(b);
    if (rpcstatus != NFS4_OK) 
        return error(NFS4_EINVAL, codestring(nfsstatus, rpcstatus));

    verify_and_adv(b, 0); // eh?
    verify_and_adv(b, 0); // verf
    verify_and_adv(b, 0); // verf
    u32 nstatus = read_beu32(b);
    if (nstatus != NFS4_OK) {
        if (config_boolean("NFS_TRACE", false))
            eprintf("nfs rpc error %s\n", codestring(nfsstatus, nstatus));

        if (nstatus == NFS4ERR_BADSESSION) {
            printf ("Bad session\n");
            *badsession = true;
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
        if (bread == -1) {
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


static status rpc_send(rpc r)
{
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

static status read_until(nfs4 c, buffer b, u32 which)
{
    int opcount = read_beu32(b);
    while (1) {
        int op =  read_beu32(b);
        if (config_boolean("NFS_TRACE", false)) {
            eprintf("received op: %s\n", codestring(nfsops, op));
        }        
        if (op == which) {
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
        default:
            return error(NFS4_PROTOCOL, "unhandled scan code");
        }
    }
    // fix
    return NFS4_OK;
}

rpc file_rpc(nfs4_file f)
{
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_sequence(r);

    push_op(r, OP_PUTFH);
    push_string(r->b, f->filehandle->contents, length(f->filehandle));
    return (r);
}


status base_transact(rpc r, int op, buffer result, boolean *badsession)
{
    int myself = 0;
    enqueue_completion(r->c, r->xid, toggle, &myself);
    check(rpc_send(r));
    result->start = result->end = 0;
    check(read_response(r->c, result));
    // drain the pipe, this should be per-slot
    while (!myself) check(parse_rpc(r->c, result, badsession));

    check(read_until(r->c, result, op));
    u32 code = read_beu32(result);
    if (code == 0) return NFS4_OK;
    return error(NFS4_EINVAL, codestring(nfsstatus, code));    
}


static status replay_rpc(rpc r)
{
    // ok sad, framer + xid + call + rpc + program + version + proc + auth + authbody + verf + verf2
    // tag + minor + opcount + sequence op
    // verify that we're starting with a sequence, which should always be the case
    // except for exchangeid and create session
    u32 offset = 4 * 15;
    memcpy(r->b->contents + offset, r->c->session, NFS4_SESSIONID_SIZE);
    u32 nseq = htonl(r->c->sequence);
    r->c->sequence++;
    memcpy(r->b->contents + offset + NFS4_SESSIONID_SIZE, &nseq, 4);
    u32 nxid = htonl(++r->c->xid);
    memcpy(r->b->contents + 4, &nxid, 4);    
}

status transact(rpc r, int op, buffer result)
{
    int tries = 0;
    boolean badsession = true;
    status s;
    
    while ((tries < 2 ) && (badsession == true)) {
        // xxx - if there are outstanding operations on the
        // old connection .. a late synch() will never
        // complete
        s = base_transact(r, op, result, &badsession);
        if (badsession) {
            status s2 = rpc_connection(r->c);
            if (s2) return s2;
            replay_rpc(r);
            tries++;
        }
    }
    return s;
}


status rpc_readdir(nfs4_dir d, buffer result)
{
    rpc r = allocate_rpc(d->c, d->c->forward);
    push_sequence(r);
    push_op(r, OP_PUTFH);
    push_string(r->b, d->filehandle->contents, length(d->filehandle));
    push_op(r, OP_READDIR);
    push_be64(r->b, d->cookie); 
    push_bytes(r->b, d->verifier, sizeof(d->verifier));
    push_be32(r->b, 512); // entry length is..meh, this is the per entry length ? 512? wth
    push_be32(r->b, result->capacity);
    push_fattr_mask(r, STANDARD_PROPERTIES);
    check(transact(r, OP_READDIR, result));
    read_buffer(result, d->verifier, NFS4_VERIFIER_SIZE);
    return NFS4_OK;
}
