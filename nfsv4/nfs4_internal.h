#include <nfs4.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <codepoint.h>
#include <nfs4xdr.h>
#include <config.h>
#include <unistd.h>

struct client {
    int fd;
    heap h;
    u32 xid;
    u32 address;
    u64 clientid;
    u8 session[NFS4_SESSIONID_SIZE];
    u32 sequence;
    u32 server_sequence;
    u32 session_sequence;    
    u8 instance_verifier[NFS4_VERIFIER_SIZE];
    buffer forward;
    buffer reverse; 
    bytes maxreq;
    bytes maxresp;
    u32 maxops;
    u32 maxreqs;
};

typedef struct  stateid {
    u32 sequence;
    u8 opaque [NFS4_OTHER_SIZE];
} *stateid;

struct file {
    client c;
    vector path;
    u8 filehandle[NFS4_FHSIZE];
    struct stateid sid;
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

#define read_beu32(__c, __b) ({\
    if ((__b->end - __b->start) < 4 ) return allocate_status(__c, "out of data"); \
    u32 v = ntohl(*(u32*)(__b->contents + __b->start));\
    __b->start += 4;\
    v;})

#define read_beu64(__c, __b) ({\
    if ((__b->end - __b->start) < 8 ) return allocate_status(__c, "out of data");\
    u64 v = ntohl(*(u32*)(__b->contents + __b->start));\
    u64 v2 = ntohl(*(u32*)(__b->contents + __b->start + 4));    \
    __b->start += 8;                                        \
    v<<32 | v2;})

typedef struct rpc *rpc;
rpc allocate_rpc(client s, buffer b);

struct status {
    char *cause;
    file f;
};
    
struct rpc {
    client c;
    bytes opcountloc;
    int opcount;
    buffer b;
    vector completions;
};

// consider pulling in proper closures 
typedef struct callback {
    void (*f)(void *a);
    void *a;
} *callback;

// should check client maxops and throw status
static inline void push_op(rpc r, u32 op)
{
    push_be32(r->b, op);
    r->opcount++;
}

void push_sequence(rpc r);


// pull in printf - "%x not equal to %x!\n", v, v2"
#define verify_and_adv(__c, __b , __v) { u32 v2 = read_beu32(__c, __b); if (__v != v2) {printf ("%d %d\n", __v, v2); return allocate_status(__c, "encoding mismatch");}}


typedef u64 clientid;

void push_stateid(rpc r);
void push_exchange_id(rpc r);
status parse_exchange_id(client, buffer);
void push_create_session(rpc r);
status parse_create_session(client, buffer);
void push_lookup(rpc r, buffer i);
buffer filename(file f);
status parse_rpc(client s, buffer b, boolean *badsession);
void push_open(rpc r, buffer name, boolean create);
status parse_open(file f, buffer b);
void push_string(buffer b, char *x, u32 length);


status segment(status (*each)(file, void *, u64, u32), int chunksize, file f, void *x, u64 offset, u32 length);
buffer push_initial_path(rpc r, vector path);
status transact(rpc r, int op, buffer b);

status write_chunk(file f, void *source, u64 offset, u32 length);
status read_chunk(file f, void *source, u64 offset, u32 length);
void push_resolution(rpc r, vector path);
status nfs4_connect(client s, char *hostname);

// statusses should have a different allocation policy entirely
static inline status allocate_status(client c, char *cause)
{
    status s = allocate(0, sizeof(struct status));
    s->cause = cause;
    return s;
}

static void deallocate_rpc(rpc r)
{
    deallocate(0, r, sizeof(struct r));
}

buffer print_path(heap h, vector v);


static void print_buffer(char *tag, buffer b)
{
    printf("%s:\n", tag);
    buffer temp = print_buffer_u32(0, b);
    write(1, temp->contents + temp->start, length(temp));
    printf("----------\n");
    deallocate_buffer(temp);
}

static inline status read_buffer(client c, buffer b, void *dest, u32 len)
{
    if (length(b) < len) return allocate_status(c, "out of data");
    if (dest != (void *)0) memcpy(dest, b->contents + b->start, len);
    b->start += len;
    return STATUS_OK;
}


status create_session(client c);
status exchange_id(client c);
status reclaim_complete(client c);
