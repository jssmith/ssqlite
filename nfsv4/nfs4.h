
typedef struct nfs4 *nfs4;
typedef struct nfs4_file *nfs4_file;

typedef unsigned long long bytes;
typedef int nfs4_mode_t;
typedef int nfs4_uid_t;
typedef int nfs4_gid_t;
typedef unsigned long long nfs4_time;

// cribbed neraly verbatim from include/asm-generic/errno-base.h
// could really use that instead..but its probably an unneccesary
// extern dependency

#define NFS4_OK            0
#define NFS4_EPERM        -1      /* Operation not permitted */
#define NFS4_ENOENT       -2     /* No such file or directory */
#define NFS4_EIO          -5     /* I/O error */
#define NFS4_ENXIO        -6     /* No such device or address */
#define NFS4_EBADF        -9     /* Bad file number */
#define NFS4_EAGAIN       -11     /* Try again */
#define NFS4_ENOMEM       -12     /* Out of memory */
#define NFS4_EACCES       -13     /* Permission denied */
#define NFS4_EFAULT       -14     /* Bad address */
#define NFS4_ENOTBLK      -15     /* Block device required */
#define NFS4_EBUSY        -16     /* Device or resource busy */
#define NFS4_EEXIST       -17     /* File exists */
#define NFS4_EXDEV        -18     /* Cross-device link */
#define NFS4_ENODEV       -19     /* No such device */
#define NFS4_ENOTDIR      -20     /* Not a directory */
#define NFS4_EISDIR       -21     /* Is a directory */
#define NFS4_EINVAL       -22     /* Invalid argument */
#define NFS4_ENFILE       -23     /* File table overflow */
#define NFS4_EMFILE       -24     /* Too many open files */
#define NFS4_ETXTBSY      -26     /* Text file busy */
#define NFS4_EFBIG        -27     /* File too large */
#define NFS4_ENOSPC       -28     /* No space left on device */
#define NFS4_ESPIPE       -29     /* Illegal seek */
#define NFS4_EROFS        -30     /* Read-only file system */
#define NFS4_EMLINK       -31     /* Too many links */
#define NFS4_PROTOCOL     -32     /* protocol/framing error */

int nfs4_create(char *hostname, nfs4 *dest);
void nfs4_destroy(nfs4);

char *nfs4_error_string(nfs4 n);

#define NFS4_RDONLY 1
#define NFS4_WRONLY 2
#define NFS4_RDWRITE 3
#define NFS4_CREAT 4
// not yet implemented
// #define NFS4_APPEND 8
#define NFS4_TRUNC 16
#define NFS4_SERVER_ASYNCH 32

int nfs4_open(nfs4 n, char *filename, int flags, nfs4_mode_t mode, nfs4_file *dest);
int nfs4_close(nfs4_file fd);
int nfs4_pwrite(nfs4_file f, void *source, bytes count, bytes offset);
// asymmetry of byte return to allow for negative errors
int nfs4_pread(nfs4_file f, void *buf, bytes, bytes offset);

// nfs4 filehandle size;
typedef unsigned char nfs4_ino_t[128];

typedef struct nfs4_properties {
    char           name[256];
    nfs4_ino_t     ino;
    nfs4_mode_t    mode;        /* protection */
    int            nlink;       /* number of hard links */
    nfs4_uid_t     uid;    /* user ID of owner */
    nfs4_gid_t     gid;    /* group ID of owner */
    bytes          size;        /* total size, in bytes */
    unsigned char  type; // enumeration
    nfs4_time st_atim;  /* time of last access */
    nfs4_time st_mtim;  /* time of last modification */
    nfs4_time st_ctim;  /* time of last status change */
} *nfs4_properties;

enum nfs_lock_type4 {
        READ_LT         = 1,
        WRITE_LT        = 2,
        READW_LT        = 3,    /* blocking read */
        WRITEW_LT       = 4     /* blocking write */
    };


int nfs4_unlink(nfs4 n, char *path);
int nfs4_stat(nfs4 n, char *path, nfs4_properties st);
int nfs4_fstat(nfs4_file fd, nfs4_properties st);
int nfs4_mkdir(nfs4 n, char *path);

typedef void *nfs4_dir;
int nfs4_opendir(nfs4, char *path, nfs4_dir *);
int nfs4_readdir(nfs4_dir, nfs4_properties *d);
int nfs4_closedir(nfs4_dir);
    
// special length for entire file?
int lock_range(nfs4_file f, int locktype, bytes offset, bytes length);
int unlock_range(nfs4_file f, int locktype, bytes offset, bytes length);
int nfs4_setid(nfs4_uid_t uid, nfs4_gid_t gid);



#ifndef eprintf
#define eprintf(format, ...) fprintf (stdout, format, ## __VA_ARGS__); fflush(stdout)
#endif

