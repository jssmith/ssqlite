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

static inline status error(int code, char* fmt, ...)
{
    heap h = init_heap(); // context
    status st = allocate(h, sizeof(struct status));
    st->error = code;
    // double fault
    st->description = allocate_buffer(h, 100);
    va_list ap;
    va_start(ap, fmt);
    buffer f = alloca_wrap_buffer(fmt, strlen(fmt));
    vbprintf(st->description, f, ap);    
    return st;
}

struct nfs4 {
    int fd;
    heap h;
    freelist rpcs;
    freelist buffers;
    freelist files;
    freelist remoteops;
    freelist dirs;                
    u32 xid; // should be per slot
    u32 server_address;
    u64 clientid;
    u8 session[NFS4_SESSIONID_SIZE];
    u32 sequence;
    u32 server_sequence;
    u32 lock_sequence;    
    u8 instance_verifier[NFS4_VERIFIER_SIZE];
    bytes maxreq;
    bytes maxresp;
    u32 buffersize;
    u32 maxops;
    u32 maxreqs;
    buffer hostname;
    vector outstanding; // should be a map
    buffer error_string;
    buffer root_filehandle;
    struct nfs4_properties default_properties;
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
    boolean asynch_writes;
    u64 expected_size;
    boolean is_trunc;
};

struct nfs4_dir {
    struct nfs4_file f;
    u64 cookie;
    u8 verifier[NFS4_VERIFIER_SIZE];
    struct buffer entries;
    buffer ref;
    boolean complete;
};

typedef struct rpc {
    nfs4 c;
    u32 xid;
    bytes opcountloc;
    buffer b;
    u32 session_offset;
    vector ops;
    // optional payload for zero copy
    boolean outstanding; // replace with a map on the slot
    u32 response_length;
} *rpc;

// closure
typedef status (*completion)(void *, buffer);

typedef struct remote_op {
    int op;
    completion parse;
    void *parse_argument;
} *remote_op;

typedef u64 clientid;

extern struct codepoint nfsops[];


// xxx - we currently dont use bits > 64, but they are there 80 defined
// in 4.2..use a bitset or a map
typedef u64 attrmask;

#include <xdr.h>

// should check client maxops and throw status
static inline void push_op(rpc r, u32 op, completion c, void *a)
{
    remote_op rop = freelist_allocate(r->c->remoteops);
    rop->parse = c;
    rop->parse_argument = a;
    rop->op = op;
    vector_push(r->ops, rop);
    push_be32(r->b, op);
    r->response_length += 8;
    if (config_boolean("NFS_TRACE", false))
        eprintf("pushed op: %C\n", nfsops, op);
}


static status print_buffer(char *tag, buffer b)
{
    eprintf("%s:\n", tag);
    heap h = init_heap(); // xxx
    buffer temp = allocate_buffer_check(h, buffer_length(b) * 3);
    print_buffer_u32(temp, b);
    eprintf("%b", temp);
    eprintf("----------\n", 0);
    deallocate_buffer(temp);
}


#define STANDARD_PROPERTIES (NFS4_PROP_MODE |\
                             NFS4_PROP_TYPE |\
                             NFS4_PROP_USER |\
                             NFS4_PROP_GROUP |\
                             NFS4_PROP_SIZE |\
                             NFS4_PROP_ACCESS_TIME |\
                             NFS4_PROP_MODIFY_TIME)

static const char charset[] = "-_0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static void fill_random(char* buffer, size_t len)
{
    for (int i = 0; i < len; i++) {
        buffer[i] = charset[rand() % len];
    }
}

rpc allocate_rpc(nfs4 s);
void push_open(rpc r, char *name, int flags, nfs4_file f, nfs4_properties p);
status parse_open(void *, buffer b);
char *push_initial_path(rpc r, char *path);
void push_resolution(rpc r, char *path);
status transact(rpc r);
status read_input(nfs4 c, boolean *badsession);
status nfs4_connect(nfs4 s);
status rpc_connection(nfs4 c);
rpc file_rpc(nfs4_file f);
status push_create(rpc r, nfs4_properties p);
status rpc_readdir(nfs4_dir d);
status base_transact(rpc r, boolean *badsession);
status rpc_send(rpc r);

