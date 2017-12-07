#include <nfs4_internal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

char *status_string(status s)
{
    return "error";
}


void print_buffer(char *tag, buffer b)
{
    printf("%s:\n", tag);
    buffer temp = print_buffer_u32(0, b);
    write(1, temp->contents + temp->start, length(temp));
    printf("----------\n");
    deallocate_buffer(temp);
}



static char *nfs4_error_string(int code)
{
    int i;
    for (i=0; (status_strings[i].id >= 0) && (status_strings[i].id != code) ; i++);
    if (status_strings[i].id == -1 ){
        return "unknown error";
    } else {
        return status_strings[i].text;
    }
}

rpc allocate_rpc(client c) 
{
    rpc r = allocate(s->h, sizeof(struct rpc));
    buffer b = r->b = c->b;
    b->start = b->end = 0;
    
    // tcp framer
    push_be32(b, 0);
    
    // rpc layer 
    push_be32(b, ++c->xid);
    push_be32(b, 0); //call
    push_be32(b, 2); //rpcvers
    push_be32(b, NFS_PROGRAM);
    push_be32(b, 4); //version
    push_be32(b, 1); //proc
    //    push_auth_sys(r); // optional?
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


// should be in the same file as create rpc..
status parse_rpc(client c, buffer b)
{
    verify_and_adv(c, b, c->xid);
    verify_and_adv(c, b, 1); // reply
    
    u32 rpcstatus = read_beu32(c, b);        
    if (rpcstatus != NFS4_OK) 
        return allocate_status(c, nfs4_error_string(rpcstatus));

    verify_and_adv(c, b, 0); // eh?
    verify_and_adv(c, b, 0); // verf
    verify_and_adv(c, b, 0); // verf
    u32 nfsstatus = read_beu32(c, b); 
    if (nfsstatus != NFS4_OK) 
        return allocate_status(c, nfs4_error_string(nfsstatus));

    verify_and_adv(c, b, 0); // tag
    return STATUS_OK;
}


static status read_response(client c, buffer b)
{
    char framing[4];
    int chars = read(c->fd, framing, 4);
    if (chars != 4) 
        return (allocate_status(c, "server socket read error"));

    
    int frame = ntohl(*(u32 *)framing) & 0x07fffffff;
    buffer_extend(b, frame);
    chars = read(c->fd, b->contents + b->start, frame);
    if (chars != frame ) 
        return (allocate_status(c, "server socket read error"));        

    b->end = chars;
    if (c->packet_trace) {
        print_buffer("resp", b);
    }
    return STATUS_OK;
}


static status rpc_send(rpc r)
{
    *(u32 *)(r->b->contents + r->opcountloc) = htonl(r->opcount);
    // framer length
    *(u32 *)(r->b->contents) = htonl(0x80000000 + length(r->b)-4);
    if (r->c->packet_trace)
        print_buffer("sent", r->b);
    
    int res = write(r->c->fd, r->b->contents + r->b->start, length(r->b));
    if (res != length(r->b)) {
        return STATUS_ERROR;
    }
}

    
status nfs4_connect(client c, char *hostname)
{
    int temp;
    struct sockaddr_in a;

    struct hostent *he = gethostbyname(hostname);
    memcpy(&c->address, he->h_addr, 4);
    
    c->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);    
    // xxx - abstract
    memcpy(&a.sin_addr, &c->address, 4);
    a.sin_family = AF_INET;
    a.sin_port = htons(2049); //configure
    
    int res = connect(c->fd,
                      (struct sockaddr *)&a,
                      sizeof(struct sockaddr_in));
    if (res != 0) {
        // make printf status variant
        return allocate_status(c, "connect failure");
    }
}

void push_resolution(rpc r, vector path)
{
    // oh, we can put the cacheed fh here
    push_op(r, OP_PUTROOTFH);
    buffer i;
    foreach(i, path) push_lookup(r, i);
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
        if (code != 0) return allocate_status(c, nfs4_error_string(code));

        switch (op) {
        case OP_SEQUENCE:
            b->start += 36;
            break;
        case OP_PUTROOTFH:
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
    rpc r = allocate_rpc(f->c);
    push_sequence(r);
    push_resolution(r, f->path);
    return (r);
}


status transact(rpc r, int op)
{
    rpc_send(r);
    r->b->start = r->b->end = 0;
    status s = read_response(r->c, r->b);
    if (!is_ok(s)) return s;
    s = parse_rpc(r->c, r->b);
    if (!is_ok(s)) return s;
    s = read_until(r->c, r->b, op);
    if (!is_ok(s)) return s;
    u32 code = read_beu32(r->c, r->b);
    if (code == 0) return STATUS_OK;
    return allocate_status(r->c, nfs4_error_string(code));
}

status file_size(file f, u64 *dest)
{
    rpc r = file_rpc(f);
    push_op(r, OP_GETATTR);
    push_be32(r->b, 1); 
    u32 mask = 1<<FATTR4_SIZE;
    push_be32(r->b, mask);
    status s = transact(r, OP_GETATTR);
    if (!is_ok(s)) return s;
    // demux attr more better
    r->b->start = r->b->end - 8;
    u64 x = 0;
    *dest = read_beu64(f->c, r->b);  
    return STATUS_OK;
}


status read_chunk(file f, void *dest, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_READ);
    push_stateid(r);
    push_be64(r->b, offset);
    push_be32(r->b, length);
    status s = transact(r, OP_READ);
    if (!is_ok(s)) return s;
    // we dont care if its the end of file -- we might for a single round trip read entire
    r->b->start += 4; 
    u32 len = read_beu32(r->c, r->b);
    // guard against len != length
    memcpy(dest, r->b->contents+r->b->start, len);
    return STATUS_OK;
}

status write_chunk(file f, void *source, u64 offset, u32 length)
{
    rpc r = file_rpc(f);
    push_op(r, OP_WRITE);
    push_stateid(r);
    push_be64(r->b, offset);
    // parameterization of file?
    push_be32(r->b, FILE_SYNC4);
    push_string(r->b, source, length);
    buffer b;
    return transact(r, OP_WRITE);
}

buffer push_initial_path(rpc r, vector path)
{
    struct buffer initial;
    memcpy(&initial, path, sizeof(struct buffer));
    initial.end  -= sizeof(void *);
    push_resolution(r, &initial);
    return vector_get(path, vector_length(path)-1);
}

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a):(b))
#endif

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

