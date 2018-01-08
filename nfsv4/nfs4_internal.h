#include <nfs4.h>
#include <runtime.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <codepoint.h>
#include <nfs4xdr.h>
#include <config.h>
#include <unistd.h>

typedef int status;

struct nfs4 {
    int fd;
    heap h;
    u32 xid;
    u32 address;
    u64 clientid;
    u8 session[NFS4_SESSIONID_SIZE];
    u32 sequence;
    u32 server_sequence;
    u32 lock_sequence;    
    u8 instance_verifier[NFS4_VERIFIER_SIZE];
    buffer forward;
    buffer reverse; 
    bytes maxreq;
    bytes maxresp;
    u32 maxops;
    u32 maxreqs;
    buffer hostname;
    vector outstanding; // should be a map
    buffer error_string;
};

typedef struct  stateid {
    u32 sequence;
    u8 opaque [NFS4_OTHER_SIZE];
} *stateid;

struct nfs4_file {
    nfs4 c;
    char *path;  // should be used for diagnostics only
    u8 filehandle[NFS4_FHSIZE];
    struct stateid latest_sid;
    struct stateid open_sid;
};

static inline int error(nfs4 c, int code, char* format, ...)
{
}

                        
static inline void push_boolean(buffer b, boolean x)
{
      buffer_extend(b, 4);
      *(u32 *)(b->contents + b->end) = x ? htonl(1) : 0;
       b->end += 4;
}

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
    if ((__b->end - __b->start) < 4 ) return error(__c, NFS4_PROTOCOL, "out of data"); \
    u32 v = ntohl(*(u32*)(__b->contents + __b->start));\
    __b->start += 4;\
    v;})

#define read_beu64(__c, __b) ({\
    if ((__b->end - __b->start) < 8 ) return error(__c, NFS4_PROTOCOL, "out of data"); \
    u64 v = ntohl(*(u32*)(__b->contents + __b->start));\
    u64 v2 = ntohl(*(u32*)(__b->contents + __b->start + 4));    \
    __b->start += 8;                                        \
    v<<32 | v2;})

typedef struct rpc *rpc;
rpc allocate_rpc(nfs4 s, buffer b);


struct rpc {
    nfs4 c;
    bytes opcountloc;
    int opcount;
    buffer b;
    vector completions;
};

// should check client maxops and throw status
static inline void push_op(rpc r, u32 op)
{
    push_be32(r->b, op);
    r->opcount++;
}

void push_sequence(rpc r);
void push_bare_sequence(rpc r);
void push_lock_sequence(rpc r);

// pull in printf - "%x not equal to %x!\n", v, v2"
#define verify_and_adv(__c, __b , __v) { u32 v2 = read_beu32(__c, __b); if (__v != v2) return error(__c, NFS4_PROTOCOL, "encoding mismatch");}


typedef u64 clientid;

void push_stateid(rpc r, stateid s);
void push_exchange_id(rpc r);
status parse_exchange_id(nfs4, buffer);
void push_create_session(rpc r);
status parse_create_session(nfs4, buffer);
void push_lookup(rpc r, buffer i);
status parse_rpc(nfs4 s, buffer b, boolean *badsession);
void push_open(rpc r, char *name, int flags);
status parse_open(nfs4_file f, buffer b);
status parse_stateid(nfs4 c, buffer b, stateid sid);
void push_string(buffer b, char *x, u32 length);


status segment(status (*each)(nfs4_file, void *, u64, u32), int chunksize, nfs4_file f, void *x, u64 offset, u32 length);
char *push_initial_path(rpc r, char *path);
status transact(rpc r, int op, buffer b);

status write_chunk(nfs4_file f, void *source, u64 offset, u32 length);
status read_chunk(nfs4_file f, void *source, u64 offset, u32 length);
void push_resolution(rpc r, char *path);
status nfs4_connect(nfs4 s);

static void deallocate_rpc(rpc r)
{
    deallocate(0, r, sizeof(struct r));
}

buffer print_path(heap h, vector v);

static void print_buffer(char *tag, buffer b)
{
    eprintf("%s:\n", tag);
    buffer temp = print_buffer_u32(0, b);
    write(1, temp->contents + temp->start, length(temp));
    eprintf("----------\n");
    deallocate_buffer(temp);
}

static inline status read_buffer(nfs4 c, buffer b, void *dest, u32 len)
{
    if (length(b) < len) return error(c, NFS4_ESPIPE, "out of data");
    if (dest != (void *)0) memcpy(dest, b->contents + b->start, len);
    b->start += len;
    return NFS4_OK;
}


int create_session(nfs4 c);
int exchange_id(nfs4 c);
int reclaim_complete(nfs4 c);
void push_session_id(rpc r, u8 *session);
status rpc_connection(nfs4 c);
void push_owner(rpc r);
int read_response(nfs4 c, buffer b);
