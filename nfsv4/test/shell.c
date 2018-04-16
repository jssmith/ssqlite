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


char *tagnames[6] = {"invalid", "buffer", "error", "file", "path"};
typedef void *value;

typedef struct client *client;

typedef struct command {
    char *name;
    value (*f)(client, vector);
    char *desc;
} *command;

struct client {
    vector cwd;
    nfs4 c;
    command commands;
};
    
client allocate_client(command commands)
{
    client c = malloc(sizeof (struct client));    
    c->cwd = allocate_vector(0, 10);
    c->commands = commands;
    return c;
}


#define tagof(__x) (((u64)__x)&7)
#define valueof(__x) ((void *)(((u64)__x)&~7))

#define TAG(__X, __Y) ((void *)((unsigned long)__X | __Y))
#define CHECK(__X, __Y) (tagof(x) == tag)?x:TAG(aprintf(0, "error"), ERROR_TAG)


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

    
void error(char *fmt, ...)
{
    buffer b = allocate_buffer(0, 80);
    va_list ap;
    buffer f = alloca_wrap_buffer(fmt, strlen(fmt));
    va_start (ap, fmt);
    vbprintf(b, f, ap);
    va_end(ap);
    write(2, b->contents + b->start, length(b));
    write(2, "\n", 1);
    exit(-1);
}


char *server; // support multiple servers

value dispatch(client c, vector n);

void *dispatch_tag(client c, int tag, vector n)
{
    void *x = dispatch(c, n);
    return CHECK(x, tag);
}


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
    } else push_character(&b, '/');             
    if (vector_length(v)) {
        buffer p = pop_path(v);
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
    buffer a = dispatch(c, args);
    buffer b = dispatch(c, args);
    if ((length(a) != length(b)) ||
        (!memcpy(a->contents, b->contents, length(a))))
        error("buffer mismatch");
    return a;
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
    return result;
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

static value delete(client c, vector args)
{
    ncheck(c, nfs4_unlink(c->c, relative_path(c, args)));
    return 0;
}

static value open_command(client c, vector args)
{
    nfs4_file f;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_RDONLY, 0, &f));
    return TAG(f, FILE_TAG);
}

static value read_command(client c, vector args)
{
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);
    struct nfs4_properties n;
    ncheck(c, nfs4_fstat(f, &n));
    buffer b = allocate_buffer(0, n.size);
    ncheck(c, nfs4_pread(f, b->contents, 0, n.size));
    b->end  = n.size;
    return b;
}

static value set_mode(client c, vector args)
{
}

static value set_owner(client c, vector args)
{
}

static value md5_command(client c, vector args)
{
    MD5_CTX ctx;
    buffer out = allocate_buffer(0, MD5_LENGTH);
    buffer b = dispatch(c, args);
    MD5_Init(&ctx)    ;
    MD5_Update(&ctx, b->contents+b->start, length(b));
    MD5_Final(out->contents, &ctx);
    out->end = MD5_LENGTH;
    return out;
}

static value write_command(client c, vector args)
{
    nfs4_file f;
    struct nfs4_properties p;
    p.mask = NFS4_PROP_MODE;
    p.mode = 0666;
    ncheck(c, nfs4_open(c->c, relative_path(c, args), NFS4_CREAT | NFS4_WRONLY, &p, &f));
    // latent
    buffer body = dispatch(c, args);
    ncheck(c, nfs4_pwrite(f, body->contents + body->start, 0, length(body)));
    nfs4_close(f);
    return body;
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
    if (write(fd, b->contents, length(b) != length(b))){
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
    buffer b = allocate_buffer(b, st.st_size);
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
    vector v = allocate_vector(0, 10);
    nfs4_dir d;
    ncheck(c, nfs4_opendir(c->c, relative_path(c, args), &d));
    struct nfs4_properties k;
    buffer line = allocate_buffer(0, 100);
    int s = 0;
    while (!s) {
        s = nfs4_readdir(d, &k);
        if (!s) {
            line->start = line->end = 0;
            format_mode(line, &k);
            push_character(line, ' ');
            print_int(line, k.size);
            push_character(line, ' ');
            push_bytes(line, k.name, strlen(k.name));
            push_character(line, '\n');
            write(1, line->contents + line->start, length(line));
        }
    }
    nfs4_closedir(d);
    if (s != NFS4_ENOENT) error("mkdir error: %s\n", nfs4_error_string(c->c));
    return NFS4_OK;
}

static value mkdir_command (client c, vector args)
{
    struct nfs4_properties p;
    p.mask = 0;
    // merge properties default and null
    ncheck(c, nfs4_mkdir(c->c, relative_path(c, args), &p));    
    return allocate_buffer("", 0);
}


//  specify lock type and upgrade
static value lock(client c, vector args)
{
    u64 from = pop_integer_argument(args);
    u64 to = pop_integer_argument(args);
    nfs4_file f = dispatch_tag(c, FILE_TAG, args);
    ncheck(c, nfs4_lock_range(f, WRITE_LT, from, to));
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
    {"compare", compare, ""},    
    {"lock", lock, ""},
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
    buffer command = vector_pop(n);
    if (command) 
        for (i = 0; c->commands[i].name[0] ; i++ )
            if (strncmp(c->commands[i].name, command->contents, length(command)) == 0) 
                return c->commands[i].f(c, n);

    error("no such command %b\n", command);
}

             
int main(int argc, char **argv)
{
    int s;
    client c = allocate_client(nfs_commands);

    if (argc == 1) {
        if (!(server = getenv("NFS4_SERVER"))) {
            error("must pass server as first argument or set NFS4_SERVER");
        }
    } else {
        server = argv[1];
    }

    if (nfs4_create(server, &c->c)) {
        printf ("open client fail\n");
    }
    buffer z = allocate_buffer(0, 100);

    write(1, "> ", 2);

    while (1) {
        char x;
        // secondary loop around input chunks
        if (read(0, &x, 1) != 1) {
            exit(-1);
        }
        if (x == '\n')  {
            if (length(z)){
                vector v = allocate_vector(0, 10);
                split(v, 0, z, ' ');
                ticks start = ktime();
                value r = dispatch(c, v);
                if (tagof(r) == ERROR_TAG) {
                    printf ("error: %s\n", (char *) valueof(r));
                }
                
                ticks total = ktime()- start;
                // format with time
                buffer b = allocate_buffer(0, 100);
                print_ticks(b, total);
                push_character(b, 0);
                printf ("%s\n", (char *)b->contents);
                z->start = z->end = 0;
                write(1, "> ", 2);                
            }
        } else push_character(z, x);
    }
}

