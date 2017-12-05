#include <nfs4.h>
#include <unistd.h>

server s;

static void create(vector path, vector args)
{
    file f = file_create(s, path);
    file_close(f);
}

static void sizec(vector path, vector args)
{
}

static void writec(vector path, vector args)
{
    file f = file_open_write(s, path);
    buffer c = vector_pop(args);
    writefile(f, c->contents + c->start, 0, length(c));
    file_close(f);
}

// directories
static struct {char *name; void (*f)(vector, vector);} commands[] = {
    {"create", create},
    {"write", writec},
    {"size", sizec},
    {"", 0}
};

int main(int argc, char **argv)
{
    s = create_server(argv[1]);
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
                    commands[i].f(fn, v);
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

