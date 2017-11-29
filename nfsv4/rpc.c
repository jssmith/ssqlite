#include <nfs4.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>


struct file {
    // filehandle
    // stateid
    server s;
    vector path;
    u8 filehandle[NFS4_FHSIZE];
};

buffer filename(heap h, file f)
{
    buffer z = join(0, f->path, '/');
    push_char(z, 0);
    return z;
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

static void push_resolution(rpc r, file f)
{
    // oh, we can put the cacheed fh here
    push_op(r, OP_PUTROOTFH);
    int len = vector_length(f->path);
    for (int i = 0 ; i < len ; i++ ) {
        push_lookup(r, vector_get(f->path, i));
    }
}


// add a file struct across this boundary
// error protocol
u64 file_size(file f)
{
    rpc r = allocate_rpc(f->s);
    push_sequence(r, f->s->session, f->s->sequence);
    push_resolution(r, f);
    push_op(r, OP_GETATTR);
    push_be32(r->b, 1); 
    u32 mask = 1<<FATTR4_SIZE;
    push_be32(r->b, mask);      
    rpc_send(r);
    
    buffer b = read_response(f->s);
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

    
void readfile(file f, void *dest, u64 offset, u32 length)
{
    rpc r = allocate_rpc(f->s);

    push_sequence(r, f->s->session, f->s->sequence);
    push_resolution(r, f);
    push_op(r, OP_READ);
    push_stateid(r);
    push_be64(r->b, offset);
    push_be32(r->b, length);

    rpc_send(r);
    buffer b = read_response(f->s);
    read_until(b, OP_READ);
    verify_and_adv(b, 0);
    b->start += 4; // we dont care if its the end of file
    u32 len = read_beu32(b);

    memcpy(dest, b->contents+b->start, len);
}


// xxx -get locks 
file file_open_read(server s, char *filename)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = split(s->h, filename, '/');
    // xxx lookup filehandle here
}

boolean file_upgrade_write(file f)
{

}

file file_open_write(server s, char *filename)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = split(s->h, filename, '/');    
}

void file_close(file f)
{

}
  
// permissions, user, a tuple?
file file_create(server s, char *filename)
{
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
    push_create_session(r, s->clientid, s->sequence);
    r->s->sequence = 0;
    rpc_send(r);
    b = read_response(s);
    verify_and_adv(b, 1);
    verify_and_adv(b, OP_CREATE_SESSION);
    parse_create_session(s, b);
        
    return s;
}
