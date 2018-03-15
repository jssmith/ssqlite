#include <nfs4.h>
#include <runtime.h>
#include <unistd.h>
#include <stdio.h>


static buffer cwd;

static boolean generator(buffer b, int len)
{
    u32 seed = 0;
    boolean was = true; 
    for (int i = b->start; i < len; i++) {
        seed = (seed * 1103515245 + 12345) & ((1U << 31) - 1);
        u8 rand = seed >> 21;
        u8 *old = b->contents + b->start + i;
        if (*old != rand) {
            was = false;
            *old = rand;
        }
    }
}
    
static int create(nfs4 c, char *path, vector args)
{
    // parse optionl mode arguments
    nfs4_file f;
    int st = nfs4_open(c, path, NFS4_CREAT, 0666, &f);
    nfs4_close(f);
    return st;
}

static int deletec(nfs4 c, char *path, vector args)
{
    return nfs4_unlink(c, path);
}

static int readc(nfs4 c, char *path, vector args)
{
    u64 length; 
    nfs4_file f;
    struct nfs4_properties n;
    int s = nfs4_open(c, path, NFS4_RDONLY, 0, &f);
    if (s) return s;
    s = nfs4_stat(c, path, &n);
    if (s) return s;
    buffer b = allocate_buffer(0, length);
    s = nfs4_pread(f, b->contents, 0, length);
    write(1, b->contents, length);
    return s;
}

static int writec(nfs4 c, char *path, vector args)
{
    nfs4_file f;
    int st = nfs4_open(c, path, NFS4_WRONLY, 0, &f);
    if (st) {
        buffer c = vector_pop(args);
        st = nfs4_pwrite(f, c->contents + c->start, 0, length(c));
        nfs4_close(f);
    }
    return st;
}

static int readd(nfs4 c, char *path, vector args)
{
    u64 length; 
    nfs4_file f;
    parse_u64(vector_pop(args), &length);
    buffer b = allocate_buffer(0, length);
    int s = nfs4_open(c, path, NFS4_RDONLY, 0, &f);
    if (s) return s;
    s = nfs4_pread(f, b->contents, 0, length);
    // check contents
    return s;
}


static int writed(nfs4 c, char *path, vector args)
{
    nfs4_file f;
    int s = nfs4_open(c, path, NFS4_CREAT, 0666, &f);
    if (s) return s;
    u64 length;
    s = parse_u64(vector_pop(args), &length);
    if (s) return s;    
    buffer b = allocate_buffer(0, length);
    generator(b, length);
    b->end = length;
    s = nfs4_pwrite(f, b->contents, 0, length);
    if (s) return s;    
    nfs4_close(f);
    return s;
}


static int lsc(nfs4 c, char *path, vector args)
{
    vector v = allocate_vector(0, 10);
    nfs4_dir d;
    int s = nfs4_opendir(c, path, &d);
    if (s) return s;
    struct nfs4_properties k;
    while (!s) {
        s = nfs4_readdir(d, &k);
        if (!s) {
            printf ("%s %lld\n", k.name, k.size);
        }
    }
    nfs4_closedir(d);
    if (s != NFS4_ENOENT) return s;
    return NFS4_OK;
}

static int mkdirc(nfs4 c, char *path, vector args)
{
    return nfs4_mkdir(c, path);
}


static int help(nfs4 c, char *path, vector args);

static status format_mode(nfs4_mode_t x, buffer b)
{
    for (int i = 8; i >= 0; i--) {
        char m[] ="rwx";
        if ((1<<i) & x)  push_character(b, m[i%3]);
        //        else push_char(b,'-');
    }
}

// directories
static struct {char *name; int (*f)(nfs4, char*, vector); char *desc;} commands[] = {
    {"create", create, ""},
    {"writed", writed, ""},
    {"readd", readd, ""},
    {"write", writec, ""},
    {"read", readc, ""},    
    {"delete", deletec, ""},    
    {"ls", lsc, ""},
    {"mkdir", mkdirc, ""},
    {"?", help, "help"},
    {"help", help, "help"},    
    {"", 0}
};


static int help(nfs4 c, char *path, vector args)
{
    for (int i = 0; commands[i].name[0] ; i++) {
        if (commands[i].f != help)
            printf ("%s\n", commands[i].name);
    }
}

int main(int argc, char **argv)
{
    nfs4 c;
    int s;

    if (argc == 1) {
        char *server = getenv("NFS4_SERVER");
        if (server) {
            s = nfs4_create(server, &c);
        } else {
            char error[] ="must pass server as first argument or set NFS4_SERVER";
            write (1, error, sizeof(error)-1);
            exit(-1);
        }
    } else {
        s = nfs4_create(argv[1], &c);
    }
    buffer z = allocate_buffer(0, 100);

    if (s) {
        printf ("open client failed\n");
        exit(-1);
    }

    write(1, "> ", 2);
    cwd = allocate_buffer(0, 10);
    push_bytes(cwd, "/", 1);
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
                buffer command = vector_pop(v);
                char *path;
                vector fn = 0;
                if (vector_length(v)) {
                    buffer p = vector_pop(v);
                    push_character(p, 0);
                    path = p->contents;
                }
                
                int i;
                for (i = 0; commands[i].name[0] ; i++ ){
                    if (strncmp(commands[i].name, command->contents, length(command)) == 0) {
                        ticks start = ktime();
                        int s = commands[i].f(c, path, v);
                        if (s) {
                            printf("error: %s\n", nfs4_error_string(c));
                        } else {
                            ticks total = ktime()- start;
                            buffer b = allocate_buffer(0, 100);
                            print_ticks(b, total);
                            push_character(b, 0);
                            printf ("%s\n", (char *)b->contents);
                        }
                        break;
                    }
                }
                if (!commands[i].name[0]) {
                    push_character(command, 0);
                    printf ("no shell command %s\n", (char *)command->contents);
                }
                z->start = z->end = 0;
                write(1, "> ", 2);                
            }
        } else push_character(z, x);
    }
}

