#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <buffer.h>

typedef struct server *server;
typedef struct rpc *rpc;

rpc allocate_rpc(server s); 
u64 file_size(server s, char *pathname);
void nfs4_connect(server);
server create_server(char *hostname);
void rpc_send(rpc);
void readfile(server s, char *pathname, void *dest, u64 offset, u32 length);
