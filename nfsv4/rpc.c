#include <nfs4_internal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <errno.h>

static struct codepoint nfsops[] = {
{"ACCESS"               , 3},
{"CLOSE"                , 4},
{"COMMIT"               , 5},
{"CREATE"               , 6},
{"DELEGPURGE"           , 7},
{"DELEGRETURN"          , 8},
{"GETATTR"              , 9},
{"GETFH"                , 10},
{"LINK"                 , 11},
{"LOCK"                 , 12},
{"LOCKT"                , 13},
{"LOCKU"                , 14},
{"LOOKUP"               , 15},
{"LOOKUPP"              , 16},
{"NVERIFY"              , 17},
{"OPEN"                 , 18},
{"OPENATTR"             , 19},
{"OPEN_CONFIRM"         , 20},
{"OPEN_DOWNGRADE"       , 21},
{"PUTFH"                , 22},
{"PUTPUBFH"             , 23},
{"PUTROOTFH"            , 24},
{"READ"                 , 25},
{"READDIR"              , 26},
{"READLINK"             , 27},
{"REMOVE"               , 28},
{"RENAME"               , 29},
{"RENEW"                , 30}, 
{"RESTOREFH"            , 31},
{"SAVEFH"               , 32},
{"SECINFO"              , 33},
{"SETATTR"              , 34},
{"SETCLIENTID"          , 35}, 
{"SETCLIENTID_CONFIRM"  , 36}, 
{"VERIFY"               , 37},
{"WRITE"                , 38},
{"RELEASE_LOCKOWNER"    , 39}, 
{"BACKCHANNEL_CTL"      , 40},
{"BIND_CONN_TO_SESSION" , 41},
{"EXCHANGE_ID"          , 42},
{"CREATE_SESSION"       , 43},
{"DESTROY_SESSION"      , 44},
{"FREE_STATEID"         , 45},
{"GET_DIR_DELEGATION"   , 46},
{"GETDEVICEINFO"        , 47},
{"GETDEVICELIST"        , 48},
{"LAYOUTCOMMIT"         , 49},
{"LAYOUTGET"            , 50},
{"LAYOUTRETURN"         , 51},
{"SECINFO_NO_NAME"      , 52},
{"SEQUENCE"             , 53},
{"SET_SSV"              , 54},
{"TEST_STATEID"         , 55},
{"WANT_DELEGATION"      , 56},
{"DESTROY_CLIENTID"     , 57},
{"RECLAIM_COMPLETE"     , 58},
{"ALLOCATE"             , 59},
{"COPY"                 , 60},
{"COPY_NOTIFY"          , 61},
{"DEALLOCATE"           , 62},
{"IO_ADVISE"            , 63},
{"LAYOUTERROR"          , 64},
{"LAYOUTSTATS"          , 65},
{"OFFLOAD_CANCEL"       , 66},
{"OFFLOAD_STATUS"       , 67},
{"READ_PLUS"            , 68},
{"SEEK"                 , 69},
{"WRITE_SAME"           , 70},
{"CLONE"                , 71},
{"ILLEGAL"              , 10044}};

char *status_string(status s)
{
    if (s == 0) return "ok";
    return s->cause;
}

rpc allocate_rpc(client c, buffer b) 
{
    // can use a single entity or a freelist
    rpc r = allocate(s->h, sizeof(struct rpc));
    r->b = b;
    b->start = b->end = 0;
    
    // tcp framer - to be filled on transmit
    push_be32(b, 0);
    
    // rpc layer 
    push_be32(b, ++c->xid);
    push_be32(b, 0); //call
    push_be32(b, 2); //rpcvers
    push_be32(b, NFS_PROGRAM);
    push_be32(b, 4); //version
    push_be32(b, 1); //proc
    push_be32(b, 0); //auth
    push_be32(b, 0); //authbody
    push_be32(b, 0); //verf
    push_be32(b, 0); //verf body kernel client passed the auth_sys structure

    // v4 compound
    push_be32(b, 0); // tag
    push_be32(b, 1); // minor version
    r->opcountloc = b->end;
    b->end += 4;
    r->c = c;
    r->opcount = 0;
    
    return r;
}

status parse_rpc(client c, buffer b, boolean *badsession)
{
    *badsession = false;
    verify_and_adv(c, b, c->xid);
    verify_and_adv(c, b, 1); // reply
    
    u32 rpcstatus = read_beu32(c, b);
    if (rpcstatus != NFS4_OK) 
        return allocate_status(c, codestring(nfsstatus, rpcstatus));

    verify_and_adv(c, b, 0); // eh?
    verify_and_adv(c, b, 0); // verf
    verify_and_adv(c, b, 0); // verf
    u32 nstatus = read_beu32(c, b);
    if (nstatus != NFS4_OK) {
        if (config_boolean("NFS_TRACE", false))
            eprintf("nfs rpc error %s\n", codestring(nfsstatus, nstatus));

        if (nstatus == NFS4ERR_BADSESSION) {
            *badsession = true;
        }
        return allocate_status(c, codestring(nfsstatus, nstatus));
    }

    verify_and_adv(c, b, 0); // tag
    return STATUS_OK;
}

static int read_fully(int fd, void* buf, size_t nbyte)
{
    ssize_t sz_read = 0;
    char* ptr = buf;
    while (sz_read < nbyte) {
        ssize_t bread = read(fd, ptr, nbyte);
        if (bread == -1) {
            eprintf("socket read error %s\n", strerror(errno));
            return -1;
        } else {
            sz_read += bread;
            // xxx can optimize this out in success case
            ptr += bread;
        }
    }
    return sz_read;
}
 
 
static status read_response(client c, buffer b)
{
    char framing[4];
    int chars = read_fully(c->fd, framing, 4);
    if (chars != 4) 
        return (allocate_status(c, "server socket read error"));
    
    int frame = ntohl(*(u32 *)framing) & 0x07fffffff;
    buffer_extend(b, frame);
    chars = read_fully(c->fd, b->contents + b->start, frame);
    if (chars != frame ) 
        return (allocate_status(c, "server socket read error"));        

    b->end = chars;
    if (config_boolean("NFS_PACKET_TRACE", false)) {
        print_buffer("resp", b);
    }
    return STATUS_OK;
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
        return allocate_status(r->c, "failed rpc write");
    }
    return STATUS_OK;
}

    
status nfs4_connect(client c)
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
        return allocate_status(c, "connect failure");
    }
    return STATUS_OK;
}

void push_resolution(rpc r, vector path)
{
    push_op(r, OP_PUTROOTFH);
    buffer i;
    vector_foreach(i, path) {
        push_op(r, OP_LOOKUP);
        push_string(r->b, i->contents + i->start, length(i));
    }
}

static status read_until(client c, buffer b, u32 which)
{
    int opcount = read_beu32(c, b);
    while (1) {
        int op =  read_beu32(c, b);
        if (op == which) {
            return STATUS_OK;
        }
        u32 code = read_beu32(c, b);
        if (code != 0) return allocate_status(c, codestring(nfsstatus, code));
        switch (op) {
        case OP_SEQUENCE:
            b->start += NFS4_SESSIONID_SIZE; // 16
            u32 seq = read_beu32(c, b);  // 20
            b->start += 16; // 36
            break;
        case OP_PUTROOTFH:
            break;
        case OP_PUTFH:
            break;
        case OP_LOOKUP:
            break;
        default:
            // printf style with code
            return allocate_status(c, "unhandled scan code");
        }
    }
    // fix
    return STATUS_OK;
}

static rpc file_rpc(file f)
{
    rpc r = allocate_rpc(f->c, f->c->forward);
    push_sequence(r);

    push_op(r, OP_PUTFH);
    if (config_boolean("NFS_USE_FILEHANDLE", true)){
        push_string(r->b, f->filehandle, NFS4_FHSIZE);
    } else {
        push_resolution(r, f->path);
    }
    return (r);
}


status base_transact(rpc r, int op, buffer result, boolean *badsession)
{
    status s = rpc_send(r);
    if (!is_ok(s)) return s;
    result->start = result->end = 0;
    s = read_response(r->c, result);
    if (!is_ok(s)) return s;
    // should instead keep session alive
    s = parse_rpc(r->c, result, badsession);
    if (!is_ok(s)) return s;
    s = read_until(r->c, result, op);
    if (!is_ok(s)) return s;
    u32 code = read_beu32(r->c, result);
    if (code == 0) return STATUS_OK;
    return allocate_status(r->c, codestring(nfsstatus, code));    
}

status exchange_id(client c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_exchange_id(r);
    buffer res = c->reverse;
    boolean bs;
    status st = base_transact(r, OP_EXCHANGE_ID, res, &bs);
    if (!is_ok(st)) {
        deallocate_rpc(r);    
        return st;
    }
    st = parse_exchange_id(c, res);
    if (!is_ok(st)) return st;
    deallocate_rpc(r);
    return STATUS_OK;
}

                  
status create_session(client c)
{
    rpc r = allocate_rpc(c, c->reverse);
    r->c->sequence = 1;  // 18.36.4 says that a new session starts at 1 implicitly
    r->c->lock_sequence = 1;
    push_create_session(r);
    r->c->server_sequence++;    
    buffer res = c->reverse;
    status st = transact(r, OP_CREATE_SESSION, res);
    if (!is_ok(st)) {
        deallocate_rpc(r);    
        return st;
    }    
    st = parse_create_session(c, res);
    if (!is_ok(st)) return st;
    deallocate_rpc(r);
    return STATUS_OK;
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

static status destroy_session(client c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_op(r, OP_DESTROY_SESSION);
    push_session_id(r, c->session);
    boolean bs2;
    return base_transact(r, OP_DESTROY_SESSION, c->reverse, &bs2);
}

status transact(rpc r, int op, buffer result)
{
    int tries = 0;
    boolean badsession = true;
    status s;
    
    while ((tries < 2 ) && (badsession == true)) {
        s = base_transact(r, op, result, &badsession);
        if (badsession) {
            status s2 = rpc_connection(r->c);
            if (!is_ok(s2)) return s2;
            replay_rpc(r);
            tries++;
        }
    }
    return s;
}

status file_size(file f, u64 *dest)
{
    rpc r = file_rpc(f);
    push_op(r, OP_GETATTR);
    push_be32(r->b, 1); 
    u32 mask = 1<<FATTR4_SIZE;
    push_be32(r->b, mask);
    buffer res =f->c->reverse;
    status s = transact(r, OP_GETATTR, res);
    if (!is_ok(s)) return s;
    // demux attr more better
    res->start = res->end - 8;
    u64 x = 0;
    *dest = read_beu64(f->c, res);  
    return STATUS_OK;
}

// we can actually use the framing length to delineate 
// header and data, and read directly into the dest buffer
// because the data is always at the end
status read_chunk(file f, void *dest, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_READ);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be32(r->b, length);
    buffer res = f->c->reverse;
    status s = transact(r, OP_READ, res);
    if (!is_ok(s)) return s;
    // we dont care if its the end of file -- we might for a single round trip read entire
    res->start += 4; 
    u32 len = read_beu32(r->c, res);
    // guard against len != length
    memcpy(dest, res->contents+res->start, len);
    return STATUS_OK;
}

// if we break transact, can writev with the header and 
// source buffer as two fragments
// add synch
status write_chunk(file f, void *source, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_WRITE);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be32(r->b, FILE_SYNC4);
    push_string(r->b, source, length);
    buffer b = f->c->reverse;
    return transact(r, OP_WRITE, b);
}

buffer push_initial_path(rpc r, vector path)
{
    struct buffer initial;
    memcpy(&initial, path, sizeof(struct buffer));
    initial.end  -= sizeof(void *);
    push_resolution(r, &initial);
    return vector_get(path, vector_length(path)-1);
}

status segment(status (*each)(file, void *, u64, u32), int chunksize, file f, void *x, u64 offset, u32 length)
{
    for (u32 done = 0; done < length;) {
        u32 xfer = MIN(length - done, chunksize);
        status s = each(f, x + done, offset+done, xfer);
        if (!is_ok(s)) return s;
        done += xfer;
    }
    return STATUS_OK;
}

status reclaim_complete(client c)
{
    rpc r = allocate_rpc(c, c->reverse);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    boolean bs;
    status st = base_transact(r, OP_RECLAIM_COMPLETE, c->reverse, &bs);
    deallocate_rpc(r);        
    return st;
}


status rpc_connection(client c)
{
    status s = nfs4_connect(c);
    if (!is_ok(s)) return s;
    s = exchange_id(c);
    if (!is_ok(s)) return s;
    s = create_session(c);
    if (!is_ok(s)) return s;
    return reclaim_complete(c);
}

status lock_range(file f, u32 locktype, u64 offset, u64 length)
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
    status s = transact(r, OP_LOCK, res);
    if (!is_ok(s)) return s;
    parse_stateid(f->c, res, &f->latest_sid);
    return STATUS_OK;
}

status unlock_range(file f, u32 locktype, u64 offset, u64 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_LOCKU);
    push_be32(r->b, locktype);
    push_bare_sequence(r);
    push_stateid(r, &f->latest_sid);
    push_be64(r->b, offset);
    push_be64(r->b, length);
    buffer res = f->c->reverse;
    status s = transact(r, OP_LOCKU, res);
    if (!is_ok(s)) return s;
    parse_stateid(f->c, res, &f->latest_sid);
    return STATUS_OK;
}
