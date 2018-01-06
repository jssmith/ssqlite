#include <runtime.h>

typedef struct file *file;
typedef struct client *client;

status create_client(char *hostname, client *dest);

status file_open_read(client c, vector path, file *x);
status file_open_write(client c, vector path, file *x);
status file_create(client c, vector path, file *x);
void file_close(file f);
status file_size(file f, u64 *s); // should be path instead of requiring an open file?
status writefile(file f, void *source, u64 offset, u32 length, u32 synch);
status readfile(file f, void *dest, u64 offset, u32 length);
buffer filename(file f);
status lock_range(file f, u32 locktype, u64 offset, u64 length);
status unlock_range(file f, u32 locktype, u64 offset, u64 length);

status exists(client c, vector path);
status delete(client c, vector path);
status readdir(client c, vector path, vector result);
status mkdir(client c, vector path);

enum file_synch {
    SYNCH_LOCAL,
    SYNCH_REMOTE, /* UNSTABLE */
    SYNCH_COMMIT, /* DATA/FILE  - some difference in the amonut of metadata? */
};

enum nfs_lock_type4 {
    READ_LT         = 1,
    WRITE_LT        = 2,
    READW_LT        = 3,    /* blocking read */
    WRITEW_LT       = 4     /* blocking write */
};

