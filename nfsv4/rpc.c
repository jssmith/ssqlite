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

static buffer print_path(heap h, vector v)
{
    buffer b = allocate_buffer(h, 100);
    buffer i;
    push_char(b, '/');
    foreach (i, v) {
        if (length(b) != 1) push_char(b, '/');
        buffer_concat(b, i);
    }
    return b;
}

//getting to be printf time
static void pf(char *header, file f)
{
    buffer b = print_path(0, f->path);
    push_char(b, 0);
    printf ("%s %s\n", header, (char *)b->contents);
}

void print_buffer(char *tag, buffer b)
{
    printf("%s:\n", tag);
    buffer temp = print_buffer_u32(0, b);
    write(1, temp->contents + temp->start, length(temp));
    printf("----------\n");
}


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
    if (s->packet_trace) {
        print_buffer("resp", b);
    }
    
    return b;
}


void rpc_send(rpc r)
{
    buffer temp = allocate_buffer(r->s->h, 2048);

    *(u32 *)(r->b->contents + r->opcountloc) = htonl(r->opcount);
    // framer length
    *(u32 *)(r->b->contents) = htonl(0x80000000 + length(r->b)-4);
    if (r->s->packet_trace)
        print_buffer("sent", r->b);
    
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

static void push_resolution(rpc r, vector path)
{
    // oh, we can put the cacheed fh here
    push_op(r, OP_PUTROOTFH);
    buffer i;
    foreach(i, path) push_lookup(r, i);
}


// add a file struct across this boundary
// error protocol
u64 file_size(file f)
{
    rpc r = allocate_rpc(f->s);
    push_sequence(r);
    push_resolution(r, f->path);
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

// these have the same sig so fragmentor can work
void readfile(file f, void *dest, u64 offset, u32 length)
{
    rpc r = allocate_rpc(f->s);

    push_sequence(r);
    push_resolution(r, f->path);
    push_op(r, OP_READ);
    push_stateid(r);
    push_be64(r->b, offset);
    push_be32(r->b, length);

    rpc_send(r);
    buffer b = read_response(f->s);
    parse_rpc(f->s, b);    
    read_until(b, OP_READ);
    verify_and_adv(b, 0);
    b->start += 4; // we dont care if its the end of file
    u32 len = read_beu32(b);

    memcpy(dest, b->contents+b->start, len);
}

void writefile(file f, void *source, u64 offset, u32 length)
{
    rpc r = allocate_rpc(f->s);

    push_sequence(r);
    push_resolution(r, f->path);
    push_op(r, OP_WRITE);
    push_stateid(r);
    push_be64(r->b, offset);
    // parameterization of file?
    push_be32(r->b, FILE_SYNC4);
    push_string(r->b, source, length);
    rpc_send(r);
    buffer b = read_response(f->s);
    parse_rpc(f->s, b);    
    read_until(b, OP_WRITE);
}




// error here
file file_open_read(server s, vector path)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->s = s;
    pf("read", f);
    // xxx lookup filehandle here - so we can ENOSUCHFILE
    return f;
}

boolean file_upgrade_write(file f)
{

}


buffer push_initial_path(rpc r, vector path)
{
    struct buffer initial;
    memcpy(&initial, path, sizeof(struct buffer));
    initial.end  -= sizeof(void *);
    push_resolution(r, &initial);
    return vector_get(path, vector_length(path)-1);
}


file file_open_write(server s, vector path)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->s = s;
    pf("open write", f);
    rpc r = allocate_rpc(f->s);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_open(r, final, false);
    rpc_send(r);
    buffer b = read_response(f->s);
    
    return f;

}

void file_close(file f)
{
    pf("close", f);
}

// permissions, user, a tuple?
file file_create(server s, vector path)
{
    file f = allocate(s->h, sizeof(struct file));
    f->path = path;
    f->s = s;

    rpc r = allocate_rpc(f->s);
    push_sequence(r);
    buffer final = push_initial_path(r, path);
    push_open(r, final, true);
    rpc_send(r);
    buffer b = read_response(f->s);

    return f;
}

// might as well implement something like stat()
boolean exists(server s, vector path)
{
    rpc r = allocate_rpc(s);
    push_sequence(r);
    push_resolution(r, path);
    push_op(r, OP_GETFH);
    rpc_send(r);
    buffer b = read_response(s);
    boolean x = parse_rpc(s, b);
    return x;

}

boolean delete(server s, vector path)
{
    rpc r = allocate_rpc(s);
    push_sequence(r);
    buffer final  = push_initial_path(r, path);
    push_op(r, OP_REMOVE);
    push_string(r->b, final->contents + final->start, length(final)); 
    rpc_send(r);
    buffer b = read_response(s);
    boolean x = parse_rpc(s, b);
    read_until(b, OP_REMOVE);
    return x;

}

server create_server(char *hostname)
{
    server s = allocate(0, sizeof(struct server));

    s->packet_trace = false;
    struct hostent *he = gethostbyname(hostname);
    memcpy(&s->address, he->h_addr, 4);
    nfs4_connect(s);     
    s->xid = 0xb956bea4;

    // sketch
    struct timeval p;
    gettimeofday(&p, 0);
    memcpy(s->instance_verifier, &p.tv_usec, NFS4_VERIFIER_SIZE);
    
    rpc r = allocate_rpc(s);
    push_exchange_id(r);
    rpc_send(r);
    
    buffer b = read_response(s);
    parse_rpc(s, b);
    verify_and_adv(b, 1);
    verify_and_adv(b, OP_EXCHANGE_ID);
    parse_exchange_id(s, b);

    r = allocate_rpc(s);
    // check - zero results in a seq error, it would be nice if
    // this were explicitly defined someplace
    r->s->sequence = 1;
    push_create_session(r);
    rpc_send(r);
    b = read_response(s);
    parse_rpc(s, b);
    verify_and_adv(b, 1);
    verify_and_adv(b, OP_CREATE_SESSION);
    parse_create_session(s, b);

    r = allocate_rpc(s);
    push_sequence(r);
    push_op(r, OP_RECLAIM_COMPLETE);
    push_be32(r->b, 0);
    rpc_send(r);
    read_response(s);
        
    return s;
}
