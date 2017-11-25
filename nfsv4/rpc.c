#include <nfs4.h>
#include <sys/socket.h>
#include <netdb.h>
#include <nfs4xdr.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>

typedef struct server {
    int fd;
    heap h;
    u32 xid;
    u32 address;
    u64 clientid;
    u8 session[16];
    u32 sequence;
    u32 server_sequence;
} *server;

struct rpc {
    server s;
    bytes opcountloc;
    int opcount;
    buffer b;
};



static inline void verify_and_adv(buffer b, u32 v)
{
    u32 v2 = read_beu32(b);
    if (v != v2) {
        printf("%x not equal to %x!\n", v, v2);
    }
}

static inline void push_op(rpc r, u32 op)
{
    push_be32(r->b, op);
    r->opcount++;
}

static inline void push_string(buffer b, char *x, u32 length) {
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

static inline void push_channel_attrs(rpc r)
{
    push_be32(r->b, 0); // headerpadsize
    push_be32(r->b, 1024*1024); // maxreqsize
    push_be32(r->b, 1024*1024); // maxresponsesize
    push_be32(r->b, 1024*1024); // maxresponsesize_cached
    push_be32(r->b, 32); // ca_maxoperations
    push_be32(r->b, 10); // ca_maxrequests
    push_be32(r->b, 0); // ca_rdma_id
}

static inline void push_session_id(rpc r)
{
    buffer_extend(r->b, sizeof(r->s->session));
    memcpy(r->b->contents + r->b->end, r->s->session, sizeof(r->s->session));
    r->b->end += sizeof(r->s->session);
}

static inline void push_client_id(rpc r)
{
    buffer_extend(r->b, sizeof(r->s->clientid));
    memcpy(r->b->contents + r->b->end, &r->s->clientid, sizeof(r->s->clientid));
    r->b->end += sizeof(r->s->clientid);
}

#define CREATE_SESSION4_FLAG_PERSIST 1
static inline void push_create_session(rpc r)
{
    push_op(r, OP_CREATE_SESSION);
    push_client_id(r);

    r->s->sequence = 0;
    push_be32(r->b, r->s->server_sequence);
    push_be32(r->b, CREATE_SESSION4_FLAG_PERSIST);
    push_channel_attrs(r); //forward
    push_channel_attrs(r); //return
    push_be32(r->b, NFS_PROGRAM);
    push_be32(r->b, 0); // auth params null
}

static inline void parse_create_session(server s, buffer b)
{
    verify_and_adv(b, 0); // why is there another nfs status here?
    // check length
    memcpy(s->session, b->contents + b->start, sizeof(s->session));
    b->start +=sizeof(s->session);
    s->sequence = read_beu32(b);
}


static inline void push_sequence(rpc r)
{
    push_op(r, OP_SEQUENCE);
    push_session_id(r);
    push_be32(r->b, r->s->sequence);  //sequence id
    push_be32(r->b, 0x00000000);  // slotid
    push_be32(r->b, 0x00000000);  // highest slotid
    push_be32(r->b, 0x00000000);  // sa_cachethis
    r->s->sequence++;
}

static inline void push_auth_sys(rpc r)
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

buffer read_response(server s)
{
    char framing[4];
    int chars = read(s->fd, framing, 4);
    if (chars != 4) {
        printf ("Read error");
    }
    
    int frame = ntohl(*(u32 *)framing) & 0x07fffffff;
    buffer b = allocate_buffer(0, frame);
    chars = read(s->fd, b->contents, frame);
    if (chars != frame ) {
        printf ("Read error");
    }
    b->end = chars;

    verify_and_adv(b, s->xid);
    verify_and_adv(b, 1); // reply
    verify_and_adv(b, 0); // status
    verify_and_adv(b, 0); // eh?
    verify_and_adv(b, 0); // verf
    verify_and_adv(b, 0); // verf
    verify_and_adv(b, 0); // nfs status
    verify_and_adv(b, 0); // tag

    //    buffer pb = print_buffer_u32(0, b);
    //    write(1, pb->contents, length(pb));
    //    write (1, "-----------\n", 12);
    
    return b;
}


void rpc_send(rpc r)
{
    buffer temp = allocate_buffer(r->s->h, 2048);

    *(u32 *)(r->b->contents + r->opcountloc) = htonl(r->opcount);
    // framer length
    *(u32 *)(r->b->contents) = htonl(0x80000000 + length(r->b)-4);
    
    // buffer p = print_buffer_u32(0, r->b);
    // write(1, p->contents, p->end);
    
    int res = write(r->s->fd, r->b->contents + r->b->start, r->b->end - r->b->start);
}

    
void nfs4_connect(server s)
{
    int temp;
    struct sockaddr_in a;
    
    s->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);    
    // xxx - abstract
    memcpy(&a.sin_addr, &s->address, 4);
    a.sin_family = AF_INET;
    a.sin_port = htons(2049); //configure
    
    int res = connect(s->fd,
                      (struct sockaddr *)&a,
                      sizeof(struct sockaddr_in));
    if (res != 0) {
        printf("connect failure %x %d\n", ntohl(s->address), res);
    }
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
    push_client_id(r);

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



// add a file struct across this boundary
// error protocol
u64 file_size(server s, char *pathname)
{
    rpc r = allocate_rpc(s);
    push_sequence(r);
    push_op(r, OP_PUTROOTFH);
    push_lookup(r, pathname);
    push_op(r, OP_GETATTR);
    push_be32(r->b, 1); 
    u32 mask = 1<<FATTR4_SIZE;
    push_be32(r->b, mask);      
    rpc_send(r);
    
    buffer b = read_response(s);
    // demux attr more better
    b->start = b->end - 8;
    u64 x = 0;
    return read_beu64(b); 
}


void read_until(buffer b, u32 which)
{
    int opcount = read_beu32(b);
    while (1) {
        int op =  read_beu32(b);
        if (op == which) {
            return;
        }
        switch (op) {
        case OP_SEQUENCE:
            b->start += 40;
            break;
        case OP_PUTROOTFH:
            b->start += 4;
            break;
        case OP_LOOKUP:
            b->start += 4;
            break;
        }
    }
}

void readfile(server s, char *pathname, void *dest, u64 offset, u32 length)
{
    rpc r = allocate_rpc(s);

    push_sequence(r);
    push_op(r, OP_PUTROOTFH);
    push_lookup(r, pathname);
    push_op(r, OP_READ);
    push_stateid(r);
    push_be64(r->b, offset);
    push_be32(r->b, length);

    rpc_send(r);
    buffer b = read_response(s);
    read_until(b, OP_READ);
    verify_and_adv(b, 0);
    b->start += 4; // we dont care if its the end of file
    u32 len = read_beu32(b);

    memcpy(dest, b->contents+b->start, len);
}


server create_server(char *hostname)
{
    server s = allocate(0, sizeof(struct server));
    struct hostent *he = gethostbyname(hostname);
    memcpy(&s->address, he->h_addr, 4);
    nfs4_connect(s);     // xxx - single server assumption
    s->xid = 0xb956bea4;
    struct timeval tv;
    gettimeofday(&tv, 0);
    // hash a bunch of stuff really
    memcpy(&s->clientid, &tv.tv_usec, 4);
    memcpy((unsigned char *)&s->clientid+4, "sqlit", 4);

    rpc r = allocate_rpc(s);
    push_exchange_id(r);
    rpc_send(r);
    buffer b = read_response(s);
    verify_and_adv(b, 1);
    verify_and_adv(b, OP_EXCHANGE_ID);
    parse_exchange_id(s, b);

    r = allocate_rpc(s);
    push_create_session(r);
    rpc_send(r);
    b = read_response(s);
    verify_and_adv(b, 1);
    verify_and_adv(b, OP_CREATE_SESSION);
    parse_create_session(s, b);
        
    return s;
}
