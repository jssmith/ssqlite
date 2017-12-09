#include <nfs4.h>
#include <unistd.h>
#include <stdio.h>

client c;

static status create(vector path, vector args)
{
    file f;
    status st = file_create(c, path, &f);
    file_close(f);
    return st;
}

static status sizec(vector path, vector args)
{
}

static status deletec(vector path, vector args)
{
}

static status readc(vector path, vector args)
{
}

static status lsc(vector path, vector args)
{
}

static status cdc(vector path, vector args)
{
    // exists
}

static status writec(vector path, vector args)
{
    file f;
    status st = file_open_write(c, path, &f);
    if (is_ok(st)) {
        buffer c = vector_pop(args);
        st = writefile(f, c->contents + c->start, 0, length(c));
        file_close(f);
    }
    return st;
}

// directories
static struct {char *name; status (*f)(vector, vector);} commands[] = {
    {"create", create},
    {"write", writec},
    {"read", readc},
    {"delete", deletec},    
    {"size", sizec},
    {"ls", sizec},
    {"cd", sizec},    
    {"", 0}
};

int main(int argc, char **argv)
{
    status s = create_client(argv[1], &c);
    buffer z = allocate_buffer(0, 100);
    
    while (1) {
        char x;
        // secondary loop around input chunks
        if (read(0, &x, 1) != 1) {
            exit(-1);
        }
        if ((x == '\n') && length(z)){
            vector v = split(0, z, ' ');
            buffer command = vector_pop(v);

            vector fn = 0;
            if (vector_length(v)) {
                fn = split(0, vector_pop(v), '/');
            }
            
            int i;
            for (i = 0; commands[i].name[0] ; i++ ){
                if (strncmp(commands[i].name, command->contents, length(command)) == 0) {
                    status s = commands[i].f(fn, v);
                    if (!is_ok(s)) {
                        printf("%s\n", status_string(s));
                    }
                    break;
                }
            }
            if (!commands[i].name[0]) {
                push_char(command, 0);
                printf ("no shell command %s\n", (char *)command->contents);
            }
            z->start = z->end = 0;
        } else push_char(z, x);
    }
}

