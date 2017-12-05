#include <stdlib.h>
#include <string.h>
#include <buffer.h>
#include <vector.h>

#define true (1)
#define false (0)
typedef int boolean;

typedef struct status *status;
typedef struct file *file;
typedef struct server *server;
server create_server(char *hostname);

status file_open_read(server s, vector path, file *x);
status file_open_write(server s, vector path, file *x);
status file_create(server s, vector path, file *x);
void file_close(file f);
status file_size(file f, u64 *s); // should be path?
status writefile(file f, void *source, u64 offset, u32 length);
status readfile(file f, void *dest, u64 offset, u32 length);;
buffer filename(file f);

boolean exists(server s, vector path);
status delete(server s, vector path);
status readdir(server s, vector path, vector result);
status mkdir(server s, vector path);

#define STATUS_OK 0

static inline boolean is_ok(status s) {
    return s == STATUS_OK;
}

char *status_description(status s);
char *status_string(status x);
