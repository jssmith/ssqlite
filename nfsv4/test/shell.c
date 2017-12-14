#include <nfs4.h>
#include <unistd.h>
#include <stdio.h>

client c;

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
    
static status create(vector path, vector args)
{
    file f;
    status st = file_create(c, path, &f);
    file_close(f);
    return st;
}

static status sizec(vector path, vector args)
{
    file f;
    status st = file_open_read(c, path, &f);
    if (!is_ok(st)) return st;
    u64 dest;
    st = file_size(f, &dest);
    if (!is_ok(st)) return st;
    printf ("%ld", dest);
    return STATUS_OK;
}

static status deletec(vector path, vector args)
{
    return delete(c, path);
}

static status readc(vector path, vector args)
{
    u64 length; 
    file f;
    status s = file_open_read(c, path, &f);
    if (!is_ok(s)) return s;
    s = file_size(f, &length);
    if (!is_ok(s)) return s;
    buffer b = allocate_buffer(0, length);
    s = readfile(f, b->contents, 0, length);
    write(1, b->contents, length);
    return s;
}

static status writec(vector path, vector args)
{
    file f;
    status st = file_open_write(c, path, &f);
    if (is_ok(st)) {
        buffer c = vector_pop(args);
        st = writefile(f, c->contents + c->start, 0, length(c), SYNCH_COMMIT);
        file_close(f);
    }
    return st;
}

static status readd(vector path, vector args)
{
    u64 length; 
    file f;
    parse_u64(vector_pop(args), &length);
    buffer b = allocate_buffer(0, length);
    status s = file_open_read(c, path, &f);
    if (!is_ok(s)) return s;
    s = readfile(f, b->contents, 0, length);
    // check contents
    return s;
}


static status writed(vector path, vector args)
{
    file f;
    status s = file_create(c, path, &f);
    if (!is_ok(s)) return s;
    u64 length;
    s = parse_u64(vector_pop(args), &length);
    if (!is_ok(s)) return s;    
    buffer b = allocate_buffer(0, length);
    generator(b, length);
    b->end = length;
    s = writefile(f, b->contents, 0, length, SYNCH_REMOTE);
    if (!is_ok(s)) return s;    
    file_close(f);
    return s;
}


static status lsc(vector path, vector args)
{
    vector v = allocate_vector(0, 10);
    status s = readdir(c, path, v);
    if (!is_ok(s)) return s;    
    buffer i;
    vector_foreach(i, v) {
        push_char(i, 0);
        printf ("%s\n", (char *)i->contents);
    }
}

static status mkdirc(vector path, vector args)
{
}


// directories
static struct {char *name; status (*f)(vector, vector);} commands[] = {
    {"create", create},
    {"writed", writed},
    {"readd", readd},
    {"write", writec},
    {"read", readc},    
    {"delete", deletec},    
    {"size", sizec},
    {"ls", lsc},
    {"mkdir", mkdirc},
    {"", 0}
};

int main(int argc, char **argv)
{
    status s = create_client(argv[1], &c);
    buffer z = allocate_buffer(0, 100);

    if (!is_ok(s)) {
        printf ("open client failed %s\n", status_string(s));
        exit(-1);
    }
    
    while (1) {
        char x;
        // secondary loop around input chunks
        if (read(0, &x, 1) != 1) {
            exit(-1);
        }
        if (x == '\n')  {
            if (length(z)){
                vector v = split(0, z, ' ');
                buffer command = vector_pop(v);
                
                vector fn = 0;
                if (vector_length(v)) {
                    fn = split(0, vector_pop(v), '/');
                }
                
                int i;
                for (i = 0; commands[i].name[0] ; i++ ){
                    if (strncmp(commands[i].name, command->contents, length(command)) == 0) {
                        ticks start = ktime();
                        status s = commands[i].f(fn, v);
                        if (!is_ok(s)) {
                            printf("%s\n", status_string(s));
                        } else {
                            ticks total = ktime()- start;
                            buffer b = allocate_buffer(0, 100);
                            print_ticks(b, total);
                            push_char(b, 0);
                            printf ("%s\n", (char *)b->contents);
                        }
                        break;
                    }
                }
                if (!commands[i].name[0]) {
                    push_char(command, 0);
                    printf ("no shell command %s\n", (char *)command->contents);
                }
                z->start = z->end = 0;
            }
        } else push_char(z, x);
    }
}

