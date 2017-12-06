#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <nfs4.h>
#include <stdio.h>

#define ORIGVFS(p) (((appd)(p)->pAppData)->parent)

typedef struct appd {
    sqlite3_vfs *parent;
    client c; // xxx - single server assumption
} *appd;
     

typedef struct sqlfile {
    sqlite3_file base;              /* IO methods */
    client c;
    file f;
#ifdef TRACE
    char filename[255];
#endif    
} *sqlfile;

// maybe a macro that allocates b 
static void bstring(buffer b, char *x)
{
    b->contents = (void *)x;
    b->end = strlen(x);
    b->start = 0;
}

static inline int translate_status(status s)
{
    return SQLITE_OK;
}
    
static int nfs4Close(sqlite3_file *pFile){
#ifdef TRACE
    printf ("close\n");
#endif
    sqlfile f = (sqlfile)pFile;
    file_close(f->f);
    return SQLITE_OK;
}

static int nfs4Read(sqlite3_file *pFile, 
                    void *zBuf, 
                    int iAmt, 
                    sqlite_int64 iOfst) 
{
#ifdef TRACE
    printf ("read %p\n", zBuf);
#endif
  sqlfile f = (sqlfile)pFile;
  translate_status(readfile(f->f, zBuf, iOfst, iAmt));
}

static int nfs4Write(sqlite3_file *pFile,
                     const void *z,
                     int iAmt,
                     sqlite_int64 iOfst)
{
#ifdef TRACE
    printf ("write\n");
#endif
    sqlfile f = (sqlfile)pFile;
    translate_status(writefile(f->f, (void *)z, iOfst, iAmt));
}

static int nfs4Truncate(sqlite3_file *pFile,
                        sqlite_int64 size)
{
#ifdef TRACE
    printf ("truncate\n");
#endif
    return SQLITE_OK;

}

static int nfs4Sync(sqlite3_file *pFile, int flags)
{
#ifdef TRACE
    printf ("sync\n");
#endif
    return SQLITE_OK;
}

static int nfs4FileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
#ifdef TRACE
    printf ("filesize\n");
#endif    
      sqlfile f = (sqlfile)pFile;
      u64 size;
      status s =file_size(f->f, &size);
      *pSize = size;
      return translate_status(s);
}


static int nfs4Lock(sqlite3_file *pFile, int eLock)
{
#ifdef TRACE
    printf ("lock %s\n", ((sqlfile)pFile)->filename);
#endif        
    return SQLITE_OK;
}

static int nfs4Unlock(sqlite3_file *pFile, int eLock)
{
#ifdef TRACE
    printf ("unlock %s\n", ((sqlfile)pFile)->filename);
#endif        
    return SQLITE_OK;
}

static int nfs4CheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
#ifdef TRACE
    printf ("check lock %s\n", ((sqlfile)pFile)->filename);
#endif            
    *pResOut = 0;
    return SQLITE_OK;
}

static int nfs4FileControl(sqlite3_file *pFile, int op, void *pArg)
{
#ifdef TRACE
    printf ("control %d\n", op);
#endif                
  sqlfile f = (sqlfile)pFile;
  int rc = SQLITE_OK;

  if (op == SQLITE_FCNTL_MMAP_SIZE) {
      *(u64 *) pArg = 0;
  }
      
  if( op==SQLITE_FCNTL_VFSNAME ){
      buffer z = filename(f->f);
      *(char**)pArg = sqlite3_mprintf("nfs4(%s)", z->contents + z->start);
      rc = SQLITE_OK;
  }

  return rc;
}

static int nfs4SectorSize(sqlite3_file *pFile){
    return 512;
}

static int nfs4DeviceCharacteristics(sqlite3_file *pFile){
    return 0; //SQLITE_IOCAP_IMMUTABLE;
}

static int nfs4ShmMap(sqlite3_file *pFile, int iPg, int pgsz, int bExtend, void volatile  **pp)
{
#ifdef TRACE
    printf ("shmap\n");
#endif                    
    return SQLITE_READONLY;
}


static int nfs4ShmLock(sqlite3_file *pFile, int offset, int n, int flags){
#ifdef TRACE
    printf ("lock %s\n", ((sqlfile)pFile)->filename);
#endif                        
    return SQLITE_READONLY;
}

static void nfs4ShmBarrier(sqlite3_file *pFile){
#ifdef TRACE
    printf ("barrier\n");
#endif                            
  return;
}


static int nfs4ShmUnmap(sqlite3_file *pFile, int deleteFlag){
#ifdef TRACE
    printf ("unmap\n");
#endif                                
  return SQLITE_OK;
}

static int nfs4Fetch(sqlite3_file *pFile,  sqlite3_int64 iOfst, int iAmt, void **pp)
{
    // why is fetch different from read? range lock?
#ifdef TRACE
    printf ("fetch\n");
#endif                                    
    file f = (file )pFile;
    translate_status(readfile(f, *pp, iOfst, iAmt));
}

static int nfs4Unfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage){
#ifdef TRACE
    printf ("unfetch\n");
#endif                                        
  return SQLITE_OK;
}

static sqlite3_io_methods *methods;

/*SQLITE_OPEN_READONLY         
SQLITE_OPEN_READWRITE        
SQLITE_OPEN_CREATE           
SQLITE_OPEN_DELETEONCLOSE    
SQLITE_OPEN_EXCLUSIVE        
SQLITE_OPEN_AUTOPROXY        
SQLITE_OPEN_URI              
SQLITE_OPEN_MEMORY           
SQLITE_OPEN_MAIN_DB          
SQLITE_OPEN_TEMP_DB          
SQLITE_OPEN_TRANSIENT_DB     
SQLITE_OPEN_MAIN_JOURNAL     
SQLITE_OPEN_TEMP_JOURNAL     
SQLITE_OPEN_SUBJOURNAL       
SQLITE_OPEN_MASTER_JOURNAL   
SQLITE_OPEN_NOMUTEX          
SQLITE_OPEN_FULLMUTEX        
SQLITE_OPEN_SHAREDCACHE      
SQLITE_OPEN_PRIVATECACHE     
SQLITE_OPEN_WAL              */

static int nfs4Open(sqlite3_vfs *pVfs,
                    const char *zName,
                    sqlite3_file *pFile,
                    int flags,
                    int *pOutFlags)
{
    sqlfile f = (sqlfile)(void *)pFile;
#ifdef TRACE
    printf ("open %s\n", zName);
    memcpy(f->filename, zName, strlen(zName));
#endif                                            
    int eType = flags&0xFFFFFF00;  /* Type of file to open */
    
    if (flags & SQLITE_OPEN_DELETEONCLOSE) {
        printf ("delete on close\n");
    }


    appd ad = pVfs->pAppData;
    memset(f, 0, sizeof(*f));
    struct buffer znb;
    bstring(&znb, (char *)zName);
    
    vector path = split(0, &znb, '/');
    buffer servername = vector_pop(path);
    buffer zeg =  vector_get(path, 0);
    push_char(servername, 0);

    if (ad->c == 0) {
        // change interface to buffer...or tuple!
        create_client((char *)servername->contents, &ad->c);
    }
    
    f->base.pMethods = methods;
    if (flags & SQLITE_OPEN_READONLY) {
        return translate_status(file_open_read(ad->c, path, &f->f));
    } else {
        if (flags & SQLITE_OPEN_CREATE) {
            return translate_status(file_create(ad->c, path, &f->f));
        } else {
            if (flags & SQLITE_OPEN_READWRITE) {
                return translate_status(file_open_write(ad->c, path, &f->f));
            }
        }
    }

    // stash in teh error string
    printf("unkown open mode\n");
    return SQLITE_CANTOPEN;
}

static int nfs4Delete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
#ifdef TRACE
    printf ("delete %s\n", zPath);
#endif                                                
    appd ad = pVfs->pAppData;
    struct buffer znb;
    bstring(&znb, (char *)zPath);
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
#ifdef TRACE
    printf ("access %s\n", zPath);
#endif                                                    
    /* The spec says there are three possible values for flags.  But only
    ** two of them are actually used */
    if( flags==SQLITE_ACCESS_EXISTS ){
        appd ad = pVfs->pAppData;
        struct buffer znb;
        bstring(&znb, (char *)zPath);
        vector path = split(0, &znb, '/');
        vector_pop(path);
        status s = exists(ad->c, path);
        *pResOut = is_ok(s)?1:0;
    }
    if( flags==SQLITE_ACCESS_READWRITE ){
        printf ("writei?\n");
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
  return ORIGVFS(pVfs)->xGetLastError(ORIGVFS(pVfs), a, b);
}
static int nfs4CurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *p){
  return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), p);
}


static sqlite3_vfs nfs4_vfs = {
  2,                           /* iVersion */
  0,                           /* szOsFile (set when registered) */
  1024,                        /* mxPathname */
  0,                           /* pNext */
  "nfs4",                    /* zName */
  0,                           /* pAppData (set when registered) */ 
  nfs4Open,                     /* xOpen */
  nfs4Delete,                   /* xDelete */
  nfs4Access,                   /* xAccess */
  nfs4FullPathname,             /* xFullPathname */
  nfs4DlOpen,                   /* xDlOpen */
  nfs4DlError,                  /* xDlError */
  nfs4DlSym,                    /* xDlSym */
  nfs4DlClose,                  /* xDlClose */
  nfs4Randomness,               /* xRandomness */
  nfs4Sleep,                    /* xSleep */
  nfs4CurrentTime,              /* xCurrentTime */
  nfs4GetLastError,             /* xGetLastError */
  nfs4CurrentTimeInt64          /* xCurrentTimeInt64 */
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

// why is this not sqlite_nfs4_init?
int sqlite3_nfs_init(sqlite3 *db,
                      char **pzErrMsg,
                      const sqlite3_api_routines *pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    appd ad = allocate(0, sizeof(struct appd));
    nfs4_vfs.pAppData = ad;
    ad->parent = sqlite3_vfs_find(0);
    ad->c = 0;
    nfs4_vfs.pNext = sqlite3_vfs_find(0);
    nfs4_vfs.szOsFile = sizeof(struct sqlfile);
    methods = &nfs4_io_methods;
    int rc = sqlite3_vfs_register(&nfs4_vfs, 1);
    return rc;
}

                      
