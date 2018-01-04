#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <nfs4.h>
#include <codepoint.h>
#include <config.h>
#include <stdio.h>
#include <assert.h>

#define ORIGVFS(p) (((appd)(p)->pAppData)->parent)

typedef struct appd {
    sqlite3_vfs *parent;
    client c; // xxx - single server assumption
    char *current_error;
    boolean trace;
} *appd;
     

typedef struct sqlfile {
    sqlite3_file base;              /* IO methods */
    appd ad;
    client c;
    file f;
    int eFileLock;
    boolean powersafe;
    boolean readonly;
    char filename[255];
} *sqlfile;

// maybe a macro that allocates b 
static void buffer_wrap_string(buffer b, char *x)
{
    b->contents = (void *)x;
    b->end = strlen(x);
    b->start = 0;
}

// try to translate if we can...maybe use unix errno in status as a translation bridge
// SQLITE_BUSY
// SQLITE_LOCKED
// SQLITE_NOMEM
// SQLITE_READONLY
// SQLITE_NOTFOUND
// SQLITE_CANTOPEN
// SQLITE_IOERR
// SQLITE_AUTH

static inline int translate_status(appd ad, status s)
{
    if (ad->trace) {
        eprintf ("status %s\n", status_string(s));
    }
    if (!is_ok(s)) return SQLITE_ERROR;
    return SQLITE_OK;
}
    
static int nfs4Close(sqlite3_file *pFile){
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace)
        eprintf ("close %s\n", f->filename);
    file_close(f->f);
    return SQLITE_OK;
}

static int nfs4Read(sqlite3_file *pFile, 
                    void *zBuf, 
                    int iAmt, 
                    sqlite_int64 iOfst) 
{
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace) {
        eprintf ("read %s offset:%lld bytes:%d ", f->filename, iOfst, iAmt);
    }
    translate_status(f->ad, readfile(f->f, zBuf, iOfst, iAmt));
}

static int nfs4Write(sqlite3_file *pFile,
                     const void *z,
                     int iAmt,
                     sqlite_int64 iOfst)
{
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace) 
        eprintf ("write %s offset:%lld bytes:%d ", f->filename, iOfst, iAmt);
    translate_status(f->ad, writefile(f->f, (void *)z, iOfst, iAmt, SYNCH_REMOTE));
}

static int nfs4Truncate(sqlite3_file *pFile,
                        sqlite_int64 size)
{
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace) 
        eprintf ("truncate %s - WARNING NOT IMPLEMENTED\n", f->filename);
    return SQLITE_ERROR;

}

static int nfs4Sync(sqlite3_file *pFile, int flags)
{ 
   sqlfile f = (sqlfile)pFile;
   if (f->ad->trace) 
       eprintf ("sync %s\n", f->filename);
    return SQLITE_OK;
}

static int nfs4FileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace) 
        eprintf ("filesize %s ", f->filename);
    u64 size;
    status s =file_size(f->f, &size);
    *pSize = size;
    return translate_status(f->ad, s);
}



static struct codepoint locktypes[] = {
    {"NONE",          0},
    {"SHARED",        1},
    {"RESERVED",      2},
    {"PENDING",       3},
    {"EXCLUSIVE",     4},
    {"", 0}};

#define PENDING_BYTE      (0x40000000)
#define RESERVED_BYTE     (PENDING_BYTE+1)
#define SHARED_FIRST      (PENDING_BYTE+2)
#define SHARED_SIZE       510

#define NO_LOCK         0
#define SHARED_LOCK     1
#define RESERVED_LOCK   2
#define PENDING_LOCK    3
#define EXCLUSIVE_LOCK  4

/*
** Adapted from SQLite's locking implementation in os_unix.c
*/
static int nfs4Lock(sqlite3_file *pFile, int eFileLock)
{
    sqlfile f = (sqlfile)pFile;

    u64 l_start;
    u64 l_len = 1L;
    u32 l_type;
    status st;

    assert(f);

    if (f->ad->trace)
        eprintf ("lock %s %s\n", f->filename, codestring(locktypes, eFileLock));

    /* If there is already a lock of this type or more restrictive on the
    ** unixFile, do nothing.
    */
    if(f->eFileLock>=eFileLock){
        return SQLITE_OK;
    }

    /* Make sure the locking sequence is correct.
    **  (1) We never move from unlocked to anything higher than shared lock.
    **  (2) SQLite never explicitly requests a pendig lock.
    **  (3) A shared lock is always held when a reserve lock is requested.
    */
    assert( f->eFileLock!=NO_LOCK || eFileLock==SHARED_LOCK );
    assert( eFileLock!=PENDING_LOCK );
    assert( eFileLock!=RESERVED_LOCK || f->eFileLock==SHARED_LOCK );


    /* A PENDING lock is needed before acquiring a SHARED lock and before
    ** acquiring an EXCLUSIVE lock.  For the SHARED lock, the PENDING will
    ** be released.
    */
    if(eFileLock==SHARED_LOCK
            || (eFileLock==EXCLUSIVE_LOCK && f->eFileLock<PENDING_LOCK)){
        l_type = (eFileLock==SHARED_LOCK?READ_LT:WRITE_LT);
        l_start = PENDING_BYTE;
        st = lock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
    }

  /* If control gets to this point, then actually go ahead and make
  ** operating system calls for the specified lock.
  */
    if (eFileLock==SHARED_LOCK){
        /* Now get the read-lock */
        l_start = SHARED_FIRST;
        l_len = SHARED_SIZE;
        st = lock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
        /* Drop the temporary PENDING lock */
        l_start = PENDING_BYTE;
        l_len = 1L;
        st = unlock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
    } else {
        /* The request was for a RESERVED or EXCLUSIVE lock.  It is
        ** assumed that there is a SHARED or greater lock on the file
        ** already.
        */
        l_type = WRITE_LT;
        assert(0 != f->eFileLock);
        assert(eFileLock==RESERVED_LOCK || eFileLock==EXCLUSIVE_LOCK);
        if (eFileLock==RESERVED_LOCK) {
            l_start = RESERVED_BYTE;
            l_len = 1L;
        } else {
            l_start = SHARED_FIRST;
            l_len = SHARED_SIZE;
        }

        st = lock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
    }
    f->eFileLock = eFileLock;
    return SQLITE_OK;
    // return translate_status(f->ad, lock_range(f->f, WRITE_LT, 0x40000000, 512));
}

/*
** Adapted from SQLite's locking implementation in os_unix.c
*/
static int nfs4Unlock(sqlite3_file *pFile, int eFileLock)
{
    sqlfile f = (sqlfile)pFile;

    u64 l_start;
    u64 l_len = 1L;
    u32 l_type;
    status st;

    if (f->ad->trace)
        eprintf ("unlock %s %s\n", ((sqlfile)pFile)->filename, codestring(locktypes, eFileLock));

    assert(f);

    assert( eFileLock<=SHARED_LOCK );
    if( f->eFileLock<=eFileLock ){
        return SQLITE_OK;
    }

    if (f->eFileLock>SHARED_LOCK) {
        if (eFileLock == SHARED_LOCK) {
            l_type = READ_LT;
            l_start = SHARED_FIRST;
            l_len = SHARED_SIZE;
            st = lock_range(f->f, l_type, l_start, l_len);
            if (!is_ok(st)) {
                return translate_status(f->ad, st);
            }
        }

        l_type = READ_LT;
        l_start = PENDING_BYTE;
        l_len = 2L;  assert( PENDING_BYTE+1==RESERVED_BYTE );
        st = unlock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
    }
    if (eFileLock == NO_LOCK) {
        l_start = SHARED_FIRST;
        l_len = SHARED_SIZE;
        st = unlock_range(f->f, l_type, l_start, l_len);
        if (!is_ok(st)) {
            return translate_status(f->ad, st);
        }
    }
    f->eFileLock = eFileLock;
    return SQLITE_OK;
}

static int nfs4CheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
    sqlfile f = (sqlfile)pFile;
    if (f->ad->trace) 
        eprintf ("check lock %s\n", ((sqlfile)pFile)->filename);

    *pResOut = 0;
    return SQLITE_OK;
}

static struct codepoint fcntl[] = {
    { "LOCKSTATE",               1},
    { "GET_LOCKPROXYFILE",       2},
    { "SET_LOCKPROXYFILE",       3},
    { "LAST_ERRNO",              4},
    { "SIZE_HINT",               5},
    { "CHUNK_SIZE",              6},
    { "FILE_POINTER",            7},
    { "SYNC_OMITTED",            8},
    { "WIN32_AV_RETRY",          9},
    { "PERSIST_WAL",            10},
    { "OVERWRITE",              11},
    { "VFSNAME",                12},
    { "POWERSAFE_OVERWRITE",    13},
    { "PRAGMA",                 14},
    { "BUSYHANDLER",            15},
    { "TEMPFILENAME",           16},
    { "MMAP_SIZE",              18},
    { "TRACE",                  19},
    { "HAS_MOVED",              20},
    { "SYNC",                   21},
    { "COMMIT_PHASETWO",        22},
    { "WIN32_SET_HANDLE",       23},
    { "WAL_BLOCK",              24},
    { "ZIPVFS",                 25},
    { "RBU",                    26},
    { "VFS_POINTER",            27},
    { "JOURNAL_POINTER",        28},
    { "WIN32_GET_HANDLE",       29},
    { "PDB",                    30},
    { "BEGIN_ATOMIC_WRITE",     31},
    { "COMMIT_ATOMIC_WRITE",    32},
    { "ROLLBACK_ATOMIC_WRITE",  33},
    {"", 0}};    


static struct codepoint iocap[] = {
    {"SAFE_APPEND",            0x00000200},
    {"SEQUENTIAL",             0x00000400},
    {"UNDELETABLE_WHEN_OPEN",  0x00000800},
    {"POWERSAFE_OVERWRITE",    0x00001000},
    {"IMMUTABLE",              0x00002000},
    {"BATCH_ATOMIC",           0x00004000},
    {"", 0}};    

static int nfs4FileControl(sqlite3_file *pFile, int op, void *pArg)
{
    sqlfile f = (sqlfile)pFile;

    if (f->ad->trace) {
        eprintf ("control %s %s\n", f->filename, codestring(fcntl, op));
        if (op == SQLITE_FCNTL_PRAGMA) {
            char **z = pArg;
            eprintf ("pragma %s\n", z[1]);
        }
    }

    int rc = SQLITE_OK;

    // if you return SQLITE_OK for any pragma call, it assumes that
    // you understood and processed it correctly. notfound punts
    // to the generic code
    if (op == SQLITE_FCNTL_PRAGMA) {
        return SQLITE_NOTFOUND;
    }
    
    if (op == SQLITE_FCNTL_MMAP_SIZE) {
        *(u64 *) pArg = 0;
    }
    
    if( op==SQLITE_FCNTL_VFSNAME ){
        buffer z = filename(f->f);
        *(char**)pArg = sqlite3_mprintf("nfs4(%s)", z->contents + z->start);
    }
    
    return rc;
}

static int nfs4SectorSize(sqlite3_file *pFile)
{
    // see if we can get sqlite to use larger pages
    return 512;
}

static int nfs4DeviceCharacteristics(sqlite3_file *pFile){
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->powersafe) {
        return SQLITE_IOCAP_POWERSAFE_OVERWRITE;
    }
    if (f->readonly) {
        return SQLITE_IOCAP_IMMUTABLE;
    }
    return 0;

}

static int nfs4ShmMap(sqlite3_file *pFile, int iPg, int pgsz, int bExtend, void volatile  **pp)
{
    sqlfile f = (sqlfile)(void *)pFile;
    // jss - probably ok to leave this as a noop rather than forwarding to the
    // parent VFS since there is no shared memory support.
    if (f->ad->trace) 
        eprintf ("shmap %s\n", f->filename);
    return SQLITE_READONLY;
}


static int nfs4ShmLock(sqlite3_file *pFile, int offset, int n, int flags){
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->ad->trace) 
        eprintf ("shm lock %s\n", ((sqlfile)pFile)->filename);
    return SQLITE_READONLY;
}

static void nfs4ShmBarrier(sqlite3_file *pFile){
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->ad->trace) 
        eprintf ("shm barrier %s\n", f->filename);
    return;
}


static int nfs4ShmUnmap(sqlite3_file *pFile, int deleteFlag){
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->ad->trace) 
        eprintf ("shm unmap %s\n", f->filename);
    return SQLITE_OK;
}

static int nfs4Fetch(sqlite3_file *pFile,  sqlite3_int64 iOfst, int iAmt, void **pp)
{
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->ad->trace) 
        eprintf ("fetch %s\n", f->filename);
    translate_status(f->ad, readfile(f->f, *pp, iOfst, iAmt));
}
 
static int nfs4Unfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage)
{
    sqlfile f = (sqlfile)(void *)pFile;
    if (f->ad->trace) 
        eprintf ("unfetch %s\n", f->filename);
    return SQLITE_OK;
}

static sqlite3_io_methods *methods;

static struct codepoint openflags[] = {
    {"READONLY",         0x00000001  },
    {"READWRITE",        0x00000002  },
    {"CREATE",           0x00000004  },
    {"DELETEONCLOSE",    0x00000008  },
    {"EXCLUSIVE",        0x00000010  },
    {"AUTOPROXY",        0x00000020  },
    {"URI",              0x00000040  },
    {"MEMORY",           0x00000080  },
    {"MAIN_DB",          0x00000100  },
    {"TEMP_DB",          0x00000200  },
    {"TRANSIENT_DB",     0x00000400  },
    {"MAIN_JOURNAL",     0x00000800  },
    {"TEMP_JOURNAL",     0x00001000  },
    {"SUBJOURNAL",       0x00002000  },
    {"MASTER_JOURNAL",   0x00004000  },
    {"NOMUTEX",          0x00008000  },
    {"FULLMUTEX",        0x00010000  },
    {"SHAREDCACHE",      0x00020000  },
    {"PRIVATECACHE",     0x00040000  },
    {"WAL",              0x00080000  },
    {"", 0}};

static int nfs4Open(sqlite3_vfs *pVfs,
                    const char *zName,
                    sqlite3_file *pFile,
                    int flags,
                    int *pOutFlags)
{
    sqlfile f = (sqlfile)(void *)pFile;
    appd ad = pVfs->pAppData;
    
    f->ad = ad;
    f->eFileLock = NO_LOCK;
    f->powersafe = true;
    f->readonly = false;

    if (ad->trace) {
        eprintf ("open %s %s ", zName, codepoint_set_string(openflags, flags));
        memcpy(f->filename, zName, strlen(zName));
    }

    struct buffer znb;
    buffer_wrap_string(&znb, (char *)zName);
    vector path = split(0, &znb, '/');
    buffer servername = vector_pop(path);
    buffer zeg =  vector_get(path, 0);
    push_char(servername, 0);

    if (ad->c == 0) {
        // change interface to tuple in order to parameterize
        status st = create_client((char *)servername->contents, &ad->c);
        if (!is_ok(st)) {
            return translate_status(ad, st);
        }
    }
    
    f->base.pMethods = methods;

    if (flags & SQLITE_OPEN_READONLY) {
        return translate_status(ad, file_open_read(ad->c, path, &f->f));
    }
    
    if (flags & SQLITE_OPEN_CREATE) {
        return translate_status(ad, file_create(ad->c, path, &f->f));
    }
    
    if (flags & SQLITE_OPEN_READWRITE) {
        return translate_status(ad, file_open_write(ad->c, path, &f->f));
    }

    return SQLITE_CANTOPEN;
}

static int nfs4Delete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
    appd ad = pVfs->pAppData;
    if (ad->trace)
        eprintf ("delete %s\n", zPath);
    
    // note - its not clear if there is an appropriate nfs4 implmentation of 
    // dirSync, and it might affect consistency at least probibalistically
    struct buffer znb;
    buffer_wrap_string(&znb, (char *)zPath);
    vector path = split(0, &znb, '/');
    vector_pop(path);
    delete(ad->c, path);
        
    return SQLITE_OK;
}


static int nfs4Access(sqlite3_vfs *pVfs, 
                      const char *zPath, 
                      int flags, 
                      int *pResOut)
{
    appd ad = pVfs->pAppData;
    if (ad->trace)
        eprintf ("access %s %d\n", zPath, flags);
    
    /* The spec says there are three possible values for flags.  But only
    ** two of them are actually used */
    if( flags==SQLITE_ACCESS_EXISTS ){
        struct buffer znb;
        buffer_wrap_string(&znb, (char *)zPath);
        vector path = split(0, &znb, '/');
        vector_pop(path);
        status s = exists(ad->c, path);
        *pResOut = is_ok(s)?1:0;
    }
    if( flags==SQLITE_ACCESS_READWRITE ){
        *pResOut = 1;
    }
    return SQLITE_OK;
}

static int nfs4FullPathname(sqlite3_vfs *pVfs, 
                            const char *zPath, 
                            int nOut, 
                            char *zOut)
{
    sqlite3_snprintf(nOut, zOut, "%s", zPath);
    return SQLITE_OK;
}

static void *nfs4DlOpen(sqlite3_vfs *pVfs, const char *zPath){
    return ORIGVFS(pVfs)->xDlOpen(ORIGVFS(pVfs), zPath);
}


static void nfs4DlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg){
    ORIGVFS(pVfs)->xDlError(ORIGVFS(pVfs), nByte, zErrMsg);
}


static void (*nfs4DlSym(sqlite3_vfs *pVfs, void *p, const char *zSym))(void){
    return ORIGVFS(pVfs)->xDlSym(ORIGVFS(pVfs), p, zSym);
}

static void nfs4DlClose(sqlite3_vfs *pVfs, void *pHandle){
    ORIGVFS(pVfs)->xDlClose(ORIGVFS(pVfs), pHandle);
}

static int nfs4Randomness(sqlite3_vfs *pVfs, int nByte, char *zBufOut){
    return ORIGVFS(pVfs)->xRandomness(ORIGVFS(pVfs), nByte, zBufOut);
}

static int nfs4Sleep(sqlite3_vfs *pVfs, int nMicro){
    return ORIGVFS(pVfs)->xSleep(ORIGVFS(pVfs), nMicro);
}

static int nfs4CurrentTime(sqlite3_vfs *pVfs, double *pTimeOut){
    return ORIGVFS(pVfs)->xCurrentTime(ORIGVFS(pVfs), pTimeOut);
}

static int nfs4GetLastError(sqlite3_vfs *pVfs, int a, char *b){
    // this is apparently...not used which is tragic.
    // if it were, we would have to merge errors fromthe 
    // lower level
    return SQLITE_OK;
}

static int nfs4CurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *p){
    return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), p);
}


static sqlite3_vfs nfs4_vfs = {
    2,                           /* iVersion */
    0,                           /* szOsFile (set when registered) */
    1024,                        /* mxPathname */
    0,                           /* pNext */
    "nfs4",                      /* zName */
    0,                           /* pAppData (set when registered) */ 
    nfs4Open,                    /* xOpen */
    nfs4Delete,                  /* xDelete */
    nfs4Access,                  /* xAccess */
    nfs4FullPathname,            /* xFullPathname */
    nfs4DlOpen,                  /* xDlOpen */
    nfs4DlError,                 /* xDlError */
    nfs4DlSym,                   /* xDlSym */
    nfs4DlClose,                 /* xDlClose */
    nfs4Randomness,              /* xRandomness */
    nfs4Sleep,                   /* xSleep */
    nfs4CurrentTime,             /* xCurrentTime */
    nfs4GetLastError,            /* xGetLastError */
    nfs4CurrentTimeInt64         /* xCurrentTimeInt64 */
};

static sqlite3_io_methods nfs4_io_methods = {
    3,                              /* iVersion */
    nfs4Close,                      /* xClose */
    nfs4Read,                       /* xRead */
    nfs4Write,                      /* xWrite */
    nfs4Truncate,                   /* xTruncate */
    nfs4Sync,                       /* xSync */
    nfs4FileSize,                   /* xFileSize */
    nfs4Lock,                       /* xLock */
    nfs4Unlock,                     /* xUnlock */
    nfs4CheckReservedLock,          /* xCheckReservedLock */
    nfs4FileControl,                /* xFileControl */
    nfs4SectorSize,                 /* xSectorSize */
    nfs4DeviceCharacteristics,      /* xDeviceCharacteristics */
    nfs4ShmMap,                     /* xShmMap */
    nfs4ShmLock,                    /* xShmLock */
    nfs4ShmBarrier,                 /* xShmBarrier */
    nfs4ShmUnmap,                   /* xShmUnmap */
    nfs4Fetch,                      /* xFetch */
    nfs4Unfetch                     /* xUnfetch */
};

// xxx - why is this not sqlite_nfs4_init?
int sqlite3_nfs_init(sqlite3 *db,
                     char **pzErrMsg,
                     const sqlite3_api_routines *pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    appd ad = allocate(0, sizeof(struct appd));
    nfs4_vfs.pAppData = ad;
    ad->parent = sqlite3_vfs_find(0);
    ad->c = 0;
    ad->trace = config_boolean("NFS_TRACE", false);
    nfs4_vfs.pNext = sqlite3_vfs_find(0);
    nfs4_vfs.szOsFile = sizeof(struct sqlfile);
    methods = &nfs4_io_methods;
    int rc = sqlite3_vfs_register(&nfs4_vfs, 1);
    if (rc == SQLITE_OK) return SQLITE_OK_LOAD_PERMANENTLY;
    return rc;
}

                      
