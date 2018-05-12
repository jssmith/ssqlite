
typedef struct nfs4 *nfs4;
typedef struct nfs4_file *nfs4_file;

typedef int nfs4_mode_t;
typedef unsigned long long nfs4_time;

typedef unsigned long long bytes;

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
#define NFS4_TRUNC 16
#define NFS4_SERVER_ASYNCH 32

// nfs4 filehandle size;
typedef unsigned char nfs4_ino_t[128];

// 1-1 from the xdr spec
typedef enum nfs_ftype4 {
        NF4REG       = 1,  /* Regular File */
        NF4DIR       = 2,  /* Directory */
        NF4BLK       = 3,  /* Special File -- block device */
        NF4CHR       = 4,  /* Special File -- character device */
        NF4LNK       = 5,  /* Symbolic Link */
        NF4SOCK      = 6,  /* Special File -- socket */
        NF4FIFO      = 7,  /* Special File -- fifo */
} nfs_ftype4;


#define MODE4_SUID  0x800;  /* set user id on execution */
#define MODE4_SGID  0x400;  /* set group id on execution */
#define MODE4_SVTX  0x200;  /* save text even after use */
#define MODE4_RUSR  0x100;  /* read permission: owner */
#define MODE4_WUSR  0x080;  /* write permission: owner */
#define MODE4_XUSR  0x040;  /* execute permission: owner */
#define MODE4_RGRP  0x020;  /* read permission: group */
#define MODE4_WGRP  0x010;  /* write permission: group */
#define MODE4_XGRP  0x008;  /* execute permission: group */
#define MODE4_ROTH  0x004;  /* read permission: other */
#define MODE4_WOTH  0x002;  /* write permission: other */
#define MODE4_XOTH  0x001;  /* execute permission: other */

#define NFS4_PROP_TYPE (1ull<<1)
#define NFS4_PROP_INO (1ull<<19)
#define NFS4_PROP_MODE (1ull<<33)
#define NFS4_PROP_USER (1ull<<36)
#define NFS4_PROP_GROUP (1ull<<37)
#define NFS4_PROP_SIZE (1ull<<4)
#define NFS4_PROP_ACCESS_TIME (1ull<<47)
#define NFS4_PROP_MODIFY_TIME (1ull<<53)

enum nfs_lock_type4 {
        READ_LT         = 1,
        WRITE_LT        = 2,
        READW_LT        = 3,    /* blocking read */
        WRITEW_LT       = 4     /* blocking write */
    };


typedef struct nfs4_properties {
    unsigned long long mask;
    char           name[256];
    nfs4_ino_t     ino;
    nfs4_mode_t    mode;        /* protection */
    int            nlink;       /* number of hard links */
    unsigned int   user;        /* user ID of owner */
    unsigned int   group;       /* group ID of owner */
    bytes          size;        /* total size, in bytes */
    nfs_ftype4     type; 
    nfs4_time      access_time;  /* time of last access */
    nfs4_time      modify_time;  /* time of last modification */
} *nfs4_properties;
/*
 * note about user and group ids:
 *  nfsv4 sensibly made these be strings..however, the linux client,
 *  the default user on server, and in general the linux environment
 *  uses integers, which are passed back and forth as printed strings.
 *  making this be an int makes it easier to deal with interoperability,
 *  but loses the generality.
 */


int nfs4_open(nfs4 n, char *filename, int flags, nfs4_properties p, nfs4_file *dest);
int nfs4_close(nfs4_file fd);
int nfs4_pwrite(nfs4_file f, void *source, bytes offset, bytes length);
int nfs4_pread(nfs4_file f, void *buf, bytes offset, bytes length);
int nfs4_unlink(nfs4 n, char *path);
int nfs4_stat(nfs4 n, char *path, nfs4_properties p);
int nfs4_fstat(nfs4_file fd, nfs4_properties p);
int nfs4_append(nfs4_file fd, void *source, bytes offset);
// consider making an open with create and ftype set
int nfs4_mkdir(nfs4 n, char *path, nfs4_properties p);
int nfs4_set_default_properties(nfs4 n, nfs4_properties p);

// consider collapsing nfs4_dir into nfs4_file
typedef struct nfs4_dir *nfs4_dir;
int nfs4_opendir(nfs4, char *path, nfs4_dir *);
int nfs4_readdir(nfs4_dir, nfs4_properties d);
int nfs4_closedir(nfs4_dir);

#define NFS4_ENTIRE_FILE (-1ull)

// upgrade?
int nfs4_lock_range(nfs4_file f, int locktype, bytes offset, bytes length);
int nfs4_unlock_range(nfs4_file f, int locktype, bytes offset, bytes length);
#define NFS4_ID_ANONYMOUS 65534
int nfs4_change_properties(nfs4_file, nfs4_properties);
