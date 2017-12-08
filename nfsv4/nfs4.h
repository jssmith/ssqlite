#include <stdlib.h>
#include <string.h>
#include <buffer.h>
#include <vector.h>

#define true (1)
#define false (0)
typedef int boolean;

typedef struct status *status;
typedef struct file *file;
typedef struct client *client;

status create_client(char *hostname, client *dest);

status file_open_read(client c, vector path, file *x);
status file_open_write(client c, vector path, file *x);
status file_create(client c, vector path, file *x);
void file_close(file f);
status file_size(file f, u64 *s); // should be path instead of requiring an open file?
status writefile(file f, void *source, u64 offset, u32 length);
status readfile(file f, void *dest, u64 offset, u32 length);;
buffer filename(file f);

status exists(client c, vector path);
status delete(client c, vector path);
status readdir(client c, vector path, vector result);
status mkdir(client c, vector path);

#define STATUS_OK 0
#define STATUS_ERROR (void *)1

static inline boolean is_ok(status s) {
    return s == STATUS_OK;
}

char *status_description(status s);
char *status_string(status x);
