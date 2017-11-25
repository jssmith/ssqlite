#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <nfs4.h>

#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))

typedef struct appd {
    sqlite3_vfs *parent;
    server s; // xxx - single server assumption
} *appd;
     

typedef struct file {
    sqlite3_file base;              /* IO methods */
    server s;
    // xxx - variable length...vector of path elements...cached filehandle?
    char filename[256];
} *file;

    
static int nfs4Close(sqlite3_file *pFile){
    return SQLITE_OK;
}

static int nfs4Read(sqlite3_file *pFile, 
                   void *zBuf, 
                   int iAmt, 
                   sqlite_int64 iOfst) 
{
  file f = (file)pFile;
  readfile(f->s, f->filename, zBuf, iOfst, iAmt);
  return SQLITE_OK;
}

static int nfs4Write(sqlite3_file *pFile,
                     const void *z,
                     int iAmt,
                     sqlite_int64 iOfst)
{
  return SQLITE_READONLY;
}

static int nfs4Truncate(sqlite3_file *pFile,
                        sqlite_int64 size)
{
  return SQLITE_READONLY;
}

static int nfs4Sync(sqlite3_file *pFile, int flags)
{
  return SQLITE_READONLY;
}

static int nfs4FileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
      file f = (file)pFile;
      *pSize =  file_size(f->s, f->filename);
      return SQLITE_OK;
}


static int nfs4Lock(sqlite3_file *pFile, int eLock){
    return SQLITE_OK;
}

static int nfs4Unlock(sqlite3_file *pFile, int eLock)
{
    return SQLITE_OK;
}

static int nfs4CheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
    *pResOut = 0;
    return SQLITE_OK;
}

static int nfs4FileControl(sqlite3_file *pFile, int op, void *pArg)
{
  file f = (file)pFile;
  int rc = SQLITE_OK;

  if (op == SQLITE_FCNTL_MMAP_SIZE) {
      *(u64 *) pArg = 0;
  }
      
  if( op==SQLITE_FCNTL_VFSNAME ){
      *(char**)pArg = sqlite3_mprintf("nfs4(%s)", f->filename);
      rc = SQLITE_OK;
  }
  return rc;
}

static int nfs4SectorSize(sqlite3_file *pFile){
    return 512;
}

static int nfs4DeviceCharacteristics(sqlite3_file *pFile){
    return SQLITE_IOCAP_IMMUTABLE;
}

static int nfs4ShmMap(sqlite3_file *pFile, int iPg, int pgsz, int bExtend, void volatile  **pp)
{
    return SQLITE_READONLY;
}


static int nfs4ShmLock(sqlite3_file *pFile, int offset, int n, int flags){
  return SQLITE_READONLY;
}

static void nfs4ShmBarrier(sqlite3_file *pFile){
  return;
}

static int nfs4ShmUnmap(sqlite3_file *pFile, int deleteFlag){
  return SQLITE_OK;
}

static int nfs4Fetch(sqlite3_file *pFile,  sqlite3_int64 iOfst, int iAmt, void **pp)
{
    file f = (file )pFile;
    readfile(f->s, f->filename, *pp, iOfst, iAmt);
    return SQLITE_OK;
}

static int nfs4Unfetch(sqlite3_file *pFile, sqlite3_int64 iOfst, void *pPage){
  return SQLITE_OK;
}

static sqlite3_io_methods *methods;

static int nfs4Open(sqlite3_vfs *pVfs,
                    const char *zName,
                    sqlite3_file *pFile,
                    int flags,
                    int *pOutFlags)
{
    int eType = flags&0xFFFFFF00;  /* Type of file to open */

    if (eType != SQLITE_OPEN_MAIN_DB) {
        return SQLITE_CANTOPEN;
    }

    appd ad = pVfs->pAppData;     
    file f = (file)(void *)pFile;
    memset(f, 0, sizeof(*f));
    char *i, *j;

    // xxx split
    buffer host = allocate_buffer(0, 100);
    for (i = (char *)zName; *i != '/'; i++) {
        push_char(host, *i);
    }
    push_char(host, 0);
    
    // xxx - single server assumption
    ad->s = create_server((char *)host->contents);
    f->base.pMethods = methods;
    i++;
    for (j = f->filename; *i != 'o'; i++, j++) *j = *i; 
    *j = 0;
    f->s = ad->s;
    return SQLITE_OK;
}

static int nfs4Delete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
  return SQLITE_READONLY;
}

static int nfs4Access(sqlite3_vfs *pVfs, 
                      const char *zPath, 
                      int flags, 
                      int *pResOut)
{
  /* The spec says there are three possible values for flags.  But only
  ** two of them are actually used */
      if( flags==SQLITE_ACCESS_EXISTS ){
          return SQLITE_CANTOPEN;
      }
  if( flags==SQLITE_ACCESS_READWRITE ){
    *pResOut = 0;
  }else{
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
    mtrace();
    SQLITE_EXTENSION_INIT2(pApi);
    // ? - this should be the server, right?
    appd ad = allocate(0, sizeof(struct appd));
    nfs4_vfs.pAppData = ad;
    ad->parent = sqlite3_vfs_find(0);
    nfs4_vfs.szOsFile = sizeof(struct file);
    methods = &nfs4_io_methods;
    int rc = sqlite3_vfs_register(&nfs4_vfs, 1);
    return rc;
}

                      
