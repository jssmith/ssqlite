#include <nfs4.h>
#include <runtime.h>
#include <unistd.h>
#include <stdio.h>
#include <md5.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


// have 7 of these in the low bit hack
#define BUFFER_TAG 1
#define ERROR_TAG 2
#define FILE_TAG 3
#define PATH_TAG 5

heap h;
char *tagnames[6] = {"invalid", "buffer", "error", "file", "path"};
typedef void *value;

typedef struct client *client;

typedef struct command {
    char *name;
    value (*f)(client, vector);
    char *desc;
} *command;

struct client {
    heap h;
    vector cwd;
    nfs4 c;
    command commands;
};
    
client allocate_client(command commands)
{
    heap h = init_heap();
    client c = allocate(h, sizeof (struct client));
    c->h = h;
    c->cwd = allocate_vector(h, 10);
    c->commands = commands;
    return c;
}


#define tagof(__x) (((u64)__x)&7)
#define valueof(__x) ((void *)(((u64)__x)&~7))

#define TAG(__X, __Y) ((void *)((unsigned long)__X | __Y))
#define CHECK(__X, __Y) (tagof(x) == tag)?(valueof(x)):TAG(aprintf(0, "error"), ERROR_TAG)


#define ncheck(__c, __code) if ((__code) != NFS4_OK) return(TAG(nfs4_error_string(__c->c), ERROR_TAG))

static void print_int(buffer b, u64 n)
{
    int log = 1;
    for (; log < n; log *= 10);
    if (n) log /= 10;
    for (; log > 0; log /= 10)  {
        int k = n/log;
        push_character(b, k + '0');
        n = n - k*log;
    }
}

    
value error(char *fmt, ...)
{
    buffer b = allocate_buffer(h, 80);
    va_list ap;
    buffer f = alloca_wrap_buffer(fmt, strlen(fmt));
    va_start (ap, fmt);
    vbprintf(b, f, ap);
    va_end(ap);
    push_character(b, 0);
    return TAG(b->contents, ERROR_TAG);
}


char *server; // support multiple servers

value dispatch(client c, vector n);

#define dispatch_tag(__c, __tag, __n)\
({\
    void *x = dispatch(__c, __n);\
    if (tagof(x) != __tag) {\
      if (tagof(x) == ERROR_TAG) return x;\
      return error ("bad tag\n");\
   }\
   valueof(x);\
 })


// destructively turn b->contents into a C string
char *terminate(buffer b)
{
    push_character(b, 0);
    return(b->contents);
}

buffer pop_path(vector v)
{
    buffer n = vector_pop(v);
    if (n == 0) error("command requires integer argument");
    return n;
}

u64 pop_integer_argument(vector v)
{
    buffer n = vector_pop(v);
    if (n == 0) error("command requires pathname argument");
    u64 result = 0;
    foreach_character(i, n) result = (result * 10) + (i - '0');
    return result;
}

char path[1024];

char *relative_path(client c, vector v)
{
    struct buffer b;
    b.contents = path;
    b.capacity = sizeof(path);
    b.start = b.end = 0;
    b.h = 0;
    buffer i;
    if (vector_length(c->cwd)) {
        vector_foreach(i, c->cwd) {
            push_character(&b, '/');             
            buffer_concat(&b, i);
        }
    }
    if (vector_length(v)) {
        buffer p = pop_path(v);        
        if (*(u8 *)(p->contents + p->start) != '/')
            push_character(&b, '/');             
        // split
        buffer_concat(&b, p);
    }
    push_character(&b, 0);
    return path;
}

static value cd(client c, vector args)
{
}


static value compare(client c, vector args)
{
    buffer a = dispatch_tag(c, BUFFER_TAG, args);
    buffer b = dispatch_tag(c, BUFFER_TAG, args);
    if ((buffer_length(a) != buffer_length(b)) ||
        (!memcpy(a->contents, b->contents, buffer_length(a))))
        // be able to generate our own errors
        return TAG(aprintf(0, "buffer mismatch"), ERROR_TAG);
    return 0;
}

// optional seed
static value generate(client v, vector args)
{
    u32 seed = 0;
    u64 len = pop_integer_argument(args);
    buffer result = allocate_buffer(0, len);
    
    for (int i = 0; i < len; i++) {
        seed = (seed * 1103515245 + 12345) & ((1U << 31) - 1);
        u8 rand = seed >> 21;
        *(u8 *)(result->contents + result->start + i) = rand;
    }
    result->end = len;
    return TAG(result, BUFFER_TAG);
}
    
static value create(client c, vector args)
{
    // parse optionl mode arguments
    nfs4_file f;
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_CREAT, &p, &f));
    nfs4_close(f);
    return 0;
}

static value recursive_delete(client c, char *base)
{
    int res = nfs4_unlink(c->c, base);        
    if (res == NFS4_EISDIR) {
        int s = 0;
        struct nfs4_properties p;
        nfs4_dir d;
        char path[1024];
        ncheck(c, nfs4_opendir(c->c, base, &d));
        while (!(s = nfs4_readdir(d, &p))) {
            sprintf(path, "%s/%s", base, p.name);
            recursive_delete(c, path);
        }
        nfs4_closedir(d);
        nfs4_unlink(c->c, path);        
    }
}

static value delete(client c, vector args)
{
    buffer first = fifo_peek(args);
    if (buffer_compare(first, alloca_wrap_string("-rf"))) {
        fifo_pop(args);
        return(recursive_delete(c, fifo_pop(args)));
    }
    ncheck(c, nfs4_unlink(c->c, relative_path(c, args)));
    return 0;
}


// maybe break out creat also
static value open_command(client c, vector args)
{
    u64 flags = NFS4_WRONLY | NFS4_CREAT | NFS4_RDONLY;
    buffer first = fifo_peek(args);
    if (*(u8 *)buffer_ref(first, 0) == '-') {
        flags = 0;
        for (int i = 1; i < buffer_length(first); i++){
            switch(*(u8 *)buffer_ref(first, i)){
            case 'w':
                flags |= NFS4_WRONLY;
                break;
            case 't':
                flags |=  NFS4_TRUNC;
                break;
            case 'c':
                flags |=  NFS4_CREAT;
                break;                
            }
        }
        fifo_pop(args);
    }
    nfs4_file f;
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_WRONLY | NFS4_CREAT, &p, &f));
    return TAG(f, FILE_TAG);
}

static value read_command(client c, vector args)
{
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);
    struct nfs4_properties n;
    ncheck(c, nfs4_fstat(f, &n));
    buffer b = allocate_buffer(h, n.size);
    ncheck(c, nfs4_pread(f, b->contents, 0, n.size));
    b->end  = n.size;
    return TAG(b, BUFFER_TAG);
}

static value set_mode(client c, vector args)
{
    nfs4_file f;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_WRONLY, 0, &f));
    struct nfs4_properties n;
    n.mask = NFS4_PROP_MODE;
    u64 m;
    parse_u64(fifo_pop(args), 8, &m);
    n.mode = m;
    nfs4_change_properties(f, &n);
    nfs4_close(f);
}

static value set_owner(client c, vector args)
{
    nfs4_file f;
    struct nfs4_properties p;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_WRONLY, 0, &f));    
    p.mask |= NFS4_PROP_USER;
    u64 u;
    parse_u64(fifo_pop(args), 10, &u);
    p.user = u;    
    nfs4_change_properties(f, &p);
    nfs4_close(f);    
}

static value md5_command(client c, vector args)
{
    MD5_CTX ctx;
    buffer out = allocate_buffer(0, MD5_LENGTH);
    buffer b = dispatch(c, args);
    MD5_Init(&ctx)    ;
    MD5_Update(&ctx, b->contents+b->start, buffer_length(b));
    MD5_Final(out->contents, &ctx);
    out->end = MD5_LENGTH;
    return out;
}

static value append_command(client c, vector args)
{
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);    
    buffer body = dispatch_tag(c, BUFFER_TAG, args);
    // asynch flag?
    ncheck(c, nfs4_append(f, body->contents + body->start, buffer_length(body)));
}

static value write_command(client c, vector args)
{
    // could wire these up monadically to permit asynch streaming
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);    
    buffer body = dispatch_tag(c, BUFFER_TAG, args);
    ncheck(c, nfs4_pwrite(f, body->contents + body->start, 0, buffer_length(body)));
    nfs4_close(f);
    return TAG(f, FILE_TAG);
}

// dup, of above write_command, dont want to break fixed arity model
static value asynch_write_command(client c, vector args)
{
    nfs4_file f;
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_CREAT | NFS4_WRONLY | NFS4_SERVER_ASYNCH, &p, &f));
    // could wire these up monadically
    buffer body = dispatch_tag(c, BUFFER_TAG, args);
    ncheck(c, nfs4_pwrite(f, body->contents + body->start, 0, buffer_length(body)));
    nfs4_close(f);
    return TAG(body, BUFFER_TAG);
}

static value conn(client c, vector args)
{
    // we can support a set if its interesting
    static client c2 = 0;
    if (c2 == NULL) {
        c2 = malloc(sizeof(struct client));
        ncheck(c2, nfs4_create(server, &c2->c));
    }
    dispatch(c2, args);
}

static value local_write(client c, vector args)
{
    char *path = terminate(pop_path(args));
    buffer b = dispatch(c, args);
    int fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if (fd < 0) error("couldn't open local %s for writing", path);
    if (write(fd, b->contents, buffer_length(b) != buffer_length(b))){
        error("failed write local %s", path);
    }
    return NFS4_OK;
}

static value local_read(client c, vector args)
{
    struct stat st;
    char *x = terminate(pop_path(args));
    int fd  = open(x, O_RDONLY);
    if (fd < 0) error("couldn't open local %s for readingx", x);    
    fstat(fd, &st);
    // reference counts
    buffer b = allocate_buffer(h, st.st_size);
    if (read(fd, b->contents, st.st_size) != st.st_size) {
        error("failed read local %s", x);
    }
    b->end = st.st_size;
    return NFS4_OK;
}

static struct command local_commands[] = {
    {"write", local_write, ""},
    {"read", local_read, ""},
};

static value local(client c, vector args)
{
    // we can support a set if its interesting
    static client c2 = 0;
    if (c2 == NULL) 
        c2 = allocate_client(local_commands);
    dispatch(c2, args);
}

static status format_mode(buffer b, nfs4_properties p)
{
    push_character(b, (p->type == NF4DIR)?'d':'-');
    for (int i = 8; i >= 0; i--) {
        char m[] ="xwr";
        if ((1<<i) & p->mode){
            push_character(b, m[i%3]);
        } else {
            push_character(b,'-');
        }
    }
}


static value ls(client c, vector args)
{
    nfs4_dir d;
    ncheck(c, nfs4_opendir(c->c, relative_path(c, args), &d));
    struct nfs4_properties k;
    vector out = allocate_vector(c->h, 10);
    int s = 0;
    while (!s) {
        s = nfs4_readdir(d, &k);
        if (!s) {
            vector row = allocate_vector(c->h, 10);
            vector mode = allocate_buffer(c->h, 10);                                  
            format_mode(mode, &k);
            vector_push(row, mode);
            vector_push(row, aprintf(c->h, "%d", k.user));
            vector_push(row, aprintf(c->h, "%d", k.group));
            vector_push(row, aprintf(c->h, "%d", k.size));
            vector_push(row, aprintf(c->h, "%s", k.name));
            vector_push(out, row);
        }
    }
    buffer formatted = tabular(mallocheap, out);
    write(1, formatted->contents, buffer_length(formatted));
    nfs4_closedir(d);
    printf ("zig: %d %d\n", s, NFS4_ENOENT);
    if (s != NFS4_ENOENT) return error("readdir: %s\n", nfs4_error_string(c->c));
    return NFS4_OK;
}

static value truncate_command (client c, vector args)
{
    nfs4_file f;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_RDWRITE, 0, &f));    
    struct nfs4_properties p;
    p.mask = NFS4_PROP_SIZE;
    p.size = 0;
    ncheck(c, nfs4_change_properties(f, &p));
    return NFS4_OK;
}

static value mkdir_command (client c, vector args)
{
    struct nfs4_properties p;
    p.mask = 0;
    // merge properties default and null
    ncheck(c, nfs4_mkdir(c->c, relative_path(c, args), &p));    
    return 0;
}


//  specify lock type and upgrade
static value lock(client c, vector args)
{
    u64 from = pop_integer_argument(args);
    u64 to = pop_integer_argument(args);
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);
    ncheck(c, nfs4_lock_range(f, WRITE_LT, from, to));
}

static value set_config(client c, vector args)
{
    buffer n = fifo_pop(args);
    buffer v = fifo_pop(args);
    push_character(n, 0);
    push_character(v, 0);
    char *ns = buffer_ref(n, 0);
    char *old = getenv(ns);
    setenv(ns, buffer_ref(v, 0), true);
    value result =  dispatch (c, args);
    if (old) {
        setenv(ns, old, true);
    } else {
        unsetenv(ns);
    }
    return result;
}

static value unlock(client c, vector args)
{
    u64 from = pop_integer_argument(args);
    u64 to = pop_integer_argument(args);
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);
    ncheck(c, nfs4_unlock_range(f, WRITE_LT, from, to));
}

static value help(client c, vector args);

                                                                        
static struct command nfs_commands[] = {
    {"create", create, "create an empty file"},
    {"write", write_command, ""},
    {"append", append_command, ""},    
    {"awrite", asynch_write_command, ""},    
    {"read", read_command, ""},
    {"md5", md5_command, ""},
    {"rm", delete, ""},
    {"generate", generate, ""},        
    {"ls", ls, ""},
    {"cd", cd, ""},
    {"open", open_command, ""},        
    {"chmod", set_mode, ""},
    {"chown", set_owner, ""},
    {"conn", conn, ""},
    {"local", local, ""},
    {"config", set_config, ""},        
    {"compare", compare, ""},
    {"lock", lock, ""},
    {"truncate", truncate_command, ""},                        
    {"unlock", unlock, ""},                            
    {"mkdir", mkdir_command, ""},
    {"?", help, "help"},
    {"help", help, "help"},    
    {"", 0}
};
                                                                                
static value help(client c, vector args)
{
    for (int i = 0; c->commands[i].name[0] ; i++) {
        if (c->commands[i].f != help)
            printf ("%s\n", c->commands[i].name);
    }
    return 0;
}

value dispatch(client c, vector n)
{
    int i;
    buffer command = fifo_pop(n);
    if (!c) return TAG("no session, set NFS4_SERVER environment or use explcit connect command", ERROR_TAG);
    if (command) {
        for (i = 0; c->commands[i].name[0] ; i++ )
            if (strncmp(c->commands[i].name, command->contents, buffer_length(command)) == 0) 
                return c->commands[i].f(c, n);
    }

    error("no such command %b\n", command);
}


void print_value(value v)
{
    if (v == 0) return;
    
    switch(tagof(v)) {
    case ERROR_TAG:
        printf ("error: %s\n", (char *) valueof(v));
        return;

    // maybe we should force these to be closed?
    // or provide variables or another way to refer to the lhs?        
    case FILE_TAG:
        return;
        
    case BUFFER_TAG:
        printf ("[%lld]\n", buffer_length(valueof(v)));
        return;

    default:
        printf ("unknown tag: %ld\n", tagof(v));
    }
}

             
int main(int argc, char **argv)
{
    int s;
    client c = 0;
    h = init_heap();
    vector commandline = allocate_vector(h, 10);
    
    if ((server = getenv("NFS4_SERVER"))) {
        c = allocate_client(nfs_commands);
        if (nfs4_create(server, &c->c)) {
            printf ("open client fail %s\n", nfs4_error_string(c->c));
        }
    }

    if (argc > 1) {
        for (int i= 1; i <argc; i++) {
            vector_push(commandline, wrap_string(h, argv[i]));
        }
        print_value(dispatch(c, commandline));
        exit(0);
    }
        
    buffer z = allocate_buffer(h, 100);

    if (isatty(0))
        write(1, "> ", 2);

    while (1) {
        char x;
        // secondary loop around input chunks
        if (read(0, &x, 1) != 1) {
            exit(-1);
        }
        if (x == '\n')  {
            if (buffer_length(z)){
                vector v = allocate_vector(h, 10);
                split(v, h, z, ' ');
                ticks start = ktime();
                print_value(dispatch(c, v));
                
                ticks total = ktime()- start;
                // add time to format
                buffer b = allocate_buffer(h, 100);                print_ticks(b, total);
                push_character(b, 0);
                printf ("%s\n", (char *)b->contents);
                z->start = z->end = 0;
                if (isatty(0)) write(1, "> ", 2);                
            }
        } else push_character(z, x);
    }
}

