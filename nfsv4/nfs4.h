#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <buffer.h>
#include <vector.h>
#include <nfs4xdr.h>

typedef struct server {
    int fd;
    heap h;
    u32 xid;
    u32 address;
    u64 clientid;
    u8 session[NFS4_SESSIONID_SIZE];
    u32 sequence;
    u32 server_sequence;
} *server;

typedef struct rpc *rpc;

typedef struct file *file;

rpc allocate_rpc(server s);
// for the moment, allocating a file for read doesn't adversely affect overhead
u64 file_size(file f);
void nfs4_connect(server);
server create_server(char *hostname);
void rpc_send(rpc);
void readfile(file f, void *dest, u64 offset, u32 length);

struct rpc {
    server s;
    bytes opcountloc;
    int opcount;
    buffer b;
};


static inline void push_op(rpc r, u32 op)
{
    push_be32(r->b, op);
    r->opcount++;
}

void push_sequence(rpc r, u8* session, u32 sequenceid);


static inline void verify_and_adv(buffer b, u32 v)
{
    u32 v2 = read_beu32(b);
    if (v != v2) {
        printf("%x not equal to %x!\n", v, v2);
    }
}

#define true (1)
#define false (0)
typedef int boolean;


typedef u64 clientid;

// refactor
void push_stateid(rpc r);
void push_exchange_id(rpc r);
void parse_exchange_id(server, buffer);
void push_create_session(rpc r, clientid id, u32 sequence);
void parse_create_session(server, buffer);
void push_lookup(rpc r, char *path);
buffer filename(heap h, file f);
file file_open_read(server s, char *filename);
