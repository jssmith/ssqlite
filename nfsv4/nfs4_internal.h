#include <nfs4.h>
#include <runtime.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <codepoint.h>
#include <nfs4xdr.h>
#include <config.h>
#include <unistd.h>
#include <string.h>

static int nfs4_is_error(status s) {return s?1:0;}


static inline status error(int code, char* description, ...)
{
    status st = allocate(0, sizeof(struct status));
    st->error = code;
    // double fault
    st->description = allocate_buffer(0, 100);
    push_bytes(st->description, description, strlen(description));
    return st;
}

struct nfs4 {
    int fd;
    heap h;
    u32 xid; // should be per slot
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
    buffer root_filehandle;
    struct nfs4_properties default_properties;
    u32 gid, uid;
};

typedef struct stateid {
    u32 sequence;
    u8 opaque [NFS4_OTHER_SIZE];
} *stateid;

struct nfs4_file {
    nfs4 c;
    char *path;  // should be used for diagnostics only
    buffer filehandle;
    struct stateid latest_sid;
    struct stateid open_sid;
    
};

#define DIR_FILE_BATCH_SIZE 32
struct nfs4_dir {
    nfs4 c;
    buffer filehandle;
    u64 cookie;
    u8 verifier[NFS4_VERIFIER_SIZE];
    struct nfs4_properties props[DIR_FILE_BATCH_SIZE];
    buffer entries;
    boolean complete;
};
    
                        
#define push_boolean(__b, __x) push_be32(__b, __x) 

#define push_be32(__b, __w) {\
    u32 __v = __w;\
    buffer_extend(__b, 4);\
    *(u32 *)(__b->contents + __b->end) = htonl(__v);\
    __b->end += 4;\
  }

#define push_be64(__b, __w) {\
    buffer_extend(__b, 8);\
    *(u32 *)(__b->contents + __b->end) = htonl(__w>>32);\
    *(u32 *)(__b->contents + __b->end + 4) = htonl(__w&0xffffffffull);\
    __b->end += 8;\
}

#define read_beu32(__b) ({\
    if ((__b->end - __b->start) < 4 ) return error(NFS4_PROTOCOL, "out of data"); \
    u32 v = ntohl(*(u32*)(__b->contents + __b->start));\
    __b->start += 4;\
    v;})

#define read_beu64(__b) ({\
    if ((__b->end - __b->start) < 8 ) return error(NFS4_PROTOCOL, "out of data"); \
    u64 v = ntohl(*(u32*)(__b->contents + __b->start));\
    u64 v2 = ntohl(*(u32*)(__b->contents + __b->start + 4));    \
    __b->start += 8;                                        \
    v<<32 | v2;})

typedef struct rpc *rpc;
rpc allocate_rpc(nfs4 s, buffer b);


struct rpc {
    nfs4 c;
    u32 xid;
    bytes opcountloc;
    int opcount;
    buffer b;
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
#define verify_and_adv(__b , __v) { u32 v2 = read_beu32(__b); if (__v != v2) return error(NFS4_PROTOCOL, "encoding mismatch");}


typedef u64 clientid;

void push_stateid(rpc r, stateid s);
void push_exchange_id(rpc r);
status parse_exchange_id(nfs4, buffer);
void push_create_session(rpc r);
status parse_create_session(nfs4, buffer);
void push_lookup(rpc r, buffer i);
status parse_rpc(nfs4 s, buffer b, boolean *badsession);
void push_open(rpc r, char *name, int flags, nfs4_properties p);
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

static status print_buffer(char *tag, buffer b)
{
    eprintf("%s:\n", tag);
    buffer temp = allocate_buffer_check(0, length(b) * 3);
    print_buffer_u32(temp, b);
    write(1, temp->contents + temp->start, length(temp));
    eprintf("----------\n");
    deallocate_buffer(temp);
}

static inline status read_buffer(buffer b, void *dest, u32 len)
{
    if (dest != (void *)0) memcpy(dest, b->contents + b->start, len);
    b->start += len;
    return NFS4_OK;
}

#define STANDARD_PROPERTIES (NFS4_PROP_MODE |\
                             NFS4_PROP_TYPE |\
                             NFS4_PROP_UID |\
                             NFS4_PROP_GID |\
                             NFS4_PROP_SIZE |\
                             NFS4_PROP_ACCESS_TIME |\
                             NFS4_PROP_MODIFY_TIME)

status create_session(nfs4 c);
status exchange_id(nfs4 c);
status reclaim_complete(nfs4 c);
void push_session_id(rpc r, u8 *session);
status rpc_connection(nfs4 c);
void push_owner(rpc r);
status read_response(nfs4 c, buffer b);
rpc file_rpc(nfs4_file f);
status push_create(rpc r, nfs4_properties p);
status push_fattr(rpc r, nfs4_properties p);

// xxx - we currently dont use bits > 64, but they are there 80 defined
// in 4.2..use a bitset, or ...a map
status push_fattr_mask(rpc r, u64 mask);
status push_create(rpc r, nfs4_properties p);
status rpc_readdir(nfs4_dir d, buffer result);
status read_fattr(buffer b, nfs4_properties p);
status parse_filehandle(buffer b, buffer dest);
status read_dirent(buffer b, nfs4_properties p, int *more, u64 *cookie);


static const char charset[] = "-_0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void fill_random(char* buffer, size_t len)
{
    for (int i = 0; i < len; i++) {
        buffer[i] = charset[rand() % len];
    }
}

status parse_dirent(buffer b, nfs4_properties p, int *more, u64 *cookie);

void push_auth_null(buffer b);
status base_transact(rpc r, int op, buffer result, boolean *badsession);
void push_auth_sys(buffer b, u32 uid, u32 gid);
