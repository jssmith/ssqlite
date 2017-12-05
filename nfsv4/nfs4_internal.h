#include <nfs4.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <nfs4xdr.h>

struct client {
    int fd;
    heap h;
    u32 xid;
    u32 address;
    u64 clientid;
    u8 session[NFS4_SESSIONID_SIZE];
    u32 sequence;
    u32 server_sequence;
    u8 instance_verifier[NFS4_VERIFIER_SIZE];
    boolean packet_trace;
    buffer b;
};


struct file {
    // filehandle
    // stateid
    client c;
    vector path;
    u8 filehandle[NFS4_FHSIZE];
};

static inline void push_be32(buffer b, u32 w) {
    buffer_extend(b, 4);
    *(u32 *)(b->contents + b->end) = htonl(w);
    b->end += 4;
}

static inline void push_be64(buffer b, u64 w) {
    buffer_extend(b, 8);
    *(u32 *)(b->contents + b->end) = htonl(w>>32);
    *(u32 *)(b->contents + b->end + 4) = htonl(w&0xffffffffull);
    b->end += 8;
}

static u32 read_beu32(buffer b)
{
    if (length(b) < 4 ) {
        printf("out of data!\n");
    }
    u32 v = ntohl(*(u32*)(b->contents + b->start));
    b->start += 4;
    return v;
}

static u64 read_beu64(buffer b)
{
    if (length(b) < 8 ) {
        printf("out of data!\n");
    }
    u64 v = ntohl(*(u32*)(b->contents + b->start));
    u64 v2 = ntohl(*(u32*)(b->contents + b->start + 4));    
    b->start += 8;
    
    return v<<32 | v2;
}

typedef struct rpc *rpc;
rpc allocate_rpc(client s);

struct rpc {
    client c;
    bytes opcountloc;
    int opcount;
    buffer b;
};


static inline void push_op(rpc r, u32 op)
{
    push_be32(r->b, op);
    r->opcount++;
}

void push_sequence(rpc r);


static inline void verify_and_adv(buffer b, u32 v)
{
    u32 v2 = read_beu32(b);
    if (v != v2) {
        printf("%x not equal to %x!\n", v, v2);
    }
}


typedef u64 clientid;

void push_stateid(rpc r);
void push_exchange_id(rpc r);
status parse_exchange_id(client, buffer);
void push_create_session(rpc r);
status parse_create_session(client, buffer);
void push_lookup(rpc r, buffer i);
buffer filename(file f);
status parse_rpc(client s, buffer b);
void push_open(rpc r, buffer name, boolean create);
void push_string(buffer b, char *x, u32 length);


status segment(status (*each)(file, void *, u64, u32), int chunksize, file f, void *x, u64 offset, u32 length);
buffer push_initial_path(rpc r, vector path);
status transact(rpc r, int op);

status write_chunk(file f, void *source, u64 offset, u32 length);
status read_chunk(file f, void *source, u64 offset, u32 length);
void push_resolution(rpc r, vector path);
status nfs4_connect(client s, char *hostname);
